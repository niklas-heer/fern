//! Bounded native-test output and lifetime; child process groups are private to the test.
use std::{
    io::Read,
    path::Path,
    process::{Child, Command, ExitStatus, Stdio},
    sync::mpsc::{self, Receiver, TryRecvError},
    time::{Duration, Instant},
};
const OUTPUT_MAX: usize = 256 * 1024;

/// Captured native test result, bounded independently for stdout and stderr.
pub struct Captured {
    pub status: ExitStatus,
    pub stdout: Vec<u8>,
    pub stderr: Vec<u8>,
}
struct Process {
    child: Child,
    stopped: bool,
}
impl Process {
    /// Terminate only this test's private process group, including inherited-output descendants.
    fn stop(&mut self) {
        if self.stopped {
            return;
        }
        self.stopped = true;
        #[cfg(unix)]
        {
            let _ = Command::new("/bin/kill")
                .args(["-KILL", "--", &format!("-{}", self.child.id())])
                .stdout(Stdio::null())
                .stderr(Stdio::null())
                .status();
        }
        let _ = self.child.kill();
    }
}
impl Drop for Process {
    /// Reap the owned child on all success, failure and timeout paths.
    fn drop(&mut self) {
        self.stop();
        let _ = self.child.wait();
    }
}

/// Execute a native test for at most sixty seconds and capture at most 256 KiB per stream.
/// Standard input is closed; each Unix test owns a separate process group for cleanup.
pub fn run(executable: &Path, timeout: Duration) -> Result<Captured, String> {
    run_command(Command::new(executable), timeout)
}

/// Apply the same capture limits to an already constructed literal command.
/// The private seam lets tests use a stable interpreter without executing a freshly written inode.
fn run_command(mut command: Command, timeout: Duration) -> Result<Captured, String> {
    if timeout.is_zero() || timeout > Duration::from_secs(60) {
        return Err("test timeout must be between 1 and 60 seconds".into());
    }
    command
        .stdin(Stdio::null())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped());
    #[cfg(unix)]
    {
        use std::os::unix::process::CommandExt;
        command.process_group(0);
    }
    let mut process = Process {
        child: command
            .spawn()
            .map_err(|error| format!("cannot execute doc test: {error}"))?,
        stopped: false,
    };
    let stdout = reader(process.child.stdout.take().ok_or("missing test stdout")?);
    let stderr = reader(process.child.stderr.take().ok_or("missing test stderr")?);
    let started = Instant::now();
    let mut status = None;
    let mut output = None;
    let mut errors = None;
    for _ in 0..6001 {
        poll(&stdout, &mut output)?;
        poll(&stderr, &mut errors)?;
        if status.is_none() {
            status = process
                .child
                .try_wait()
                .map_err(|error| error.to_string())?;
            if status.is_some() {
                process.stop();
            }
        }
        if let (Some(status), Some(stdout), Some(stderr)) = (status, &output, &errors) {
            return Ok(Captured {
                status,
                stdout: stdout.clone(),
                stderr: stderr.clone(),
            });
        }
        if started.elapsed() >= timeout {
            return Err("doc test timed out".into());
        }
        std::thread::sleep(Duration::from_millis(10));
    }
    Err("doc test timed out".into())
}

/// Read fixed-size chunks and stop before retaining an over-limit native output stream.
fn reader(input: impl Read + Send + 'static) -> Receiver<Result<Vec<u8>, String>> {
    let (sender, receiver) = mpsc::sync_channel(1);
    std::thread::spawn(move || {
        let mut bytes = Vec::new();
        let result = input
            .take(OUTPUT_MAX as u64 + 1)
            .read_to_end(&mut bytes)
            .map_err(|error| error.to_string())
            .and_then(|_| {
                if bytes.len() > OUTPUT_MAX {
                    Err("doc test output limit exceeded".into())
                } else {
                    Ok(bytes)
                }
            });
        let _ = sender.send(result);
    });
    receiver
}

/// Drain each stream result once, propagating capture failures before waiting for the child.
fn poll(
    receiver: &Receiver<Result<Vec<u8>, String>>,
    target: &mut Option<Vec<u8>>,
) -> Result<(), String> {
    if target.is_some() {
        return Ok(());
    }
    match receiver.try_recv() {
        Ok(result) => *target = Some(result?),
        Err(TryRecvError::Empty) => {}
        Err(TryRecvError::Disconnected) => return Err("doc test output reader disconnected".into()),
    }
    Ok(())
}

#[cfg(all(test, unix))]
mod tests {
    use super::*;

    /// Execute a fixed test body through the existing interpreter, avoiding write-to-exec races.
    fn script(body: &str, timeout: Duration) -> Result<Captured, String> {
        let mut command = Command::new("/bin/sh");
        command.args(["-c", body]);
        run_command(command, timeout)
    }

    #[test]
    fn captures_streams_and_preserves_nonzero_exit_status() {
        let result = script(
            "printf output; printf errors >&2; exit 7",
            Duration::from_secs(1),
        )
        .unwrap();
        assert_eq!(result.status.code(), Some(7));
        assert_eq!(result.stdout, b"output");
        assert_eq!(result.stderr, b"errors");
    }

    #[test]
    fn descendant_inheriting_output_does_not_hold_capture_open() {
        let started = Instant::now();
        let result = script("/bin/sleep 30 &\nprintf captured", Duration::from_secs(1)).unwrap();
        assert!(result.status.success());
        assert_eq!(result.stdout, b"captured");
        assert!(started.elapsed() < Duration::from_secs(2));
    }

    #[test]
    fn timeout_and_stream_budgets_fail_without_waiting_for_unbounded_work() {
        assert!(script("exec /bin/sleep 30", Duration::from_millis(100))
            .err()
            .unwrap()
            .contains("timed out"));
        let bytes = std::io::Cursor::new(vec![0; OUTPUT_MAX + 1]);
        let result = reader(bytes).recv_timeout(Duration::from_secs(1)).unwrap();
        assert!(result.unwrap_err().contains("output limit"));
        assert!(run(Path::new("unused"), Duration::ZERO).is_err());
        assert!(run(Path::new("unused"), Duration::from_secs(61)).is_err());
    }

    #[test]
    fn concurrent_shell_fixtures_keep_output_and_status_independent() {
        std::thread::scope(|scope| {
            let jobs: Vec<_> = (0..8)
                .map(|worker| {
                    scope.spawn(move || {
                        for iteration in 0..20 {
                            let expected = format!("{worker}:{iteration}");
                            let result = script(
                                &format!("printf '%s' '{expected}'; exit 7"),
                                Duration::from_secs(3),
                            )
                            .unwrap();
                            assert_eq!(result.status.code(), Some(7));
                            assert_eq!(result.stdout, expected.as_bytes());
                            assert!(result.stderr.is_empty());
                        }
                    })
                })
                .collect();
            for job in jobs {
                job.join().unwrap();
            }
        });
    }
}
