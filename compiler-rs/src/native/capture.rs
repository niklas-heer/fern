//! Bounded native capture through a retained-child supervisor; no Rust group signals.
use std::{
    io::Read,
    path::{Path, PathBuf},
    process::{Command, ExitStatus, Stdio},
    time::Duration,
};
mod frame;
use frame::decode;
const OUTPUT_MAX: usize = 256 * 1024;
const FRAME_MAX: usize = 2 * OUTPUT_MAX + 256;

/// Captured native test result, bounded independently for stdout and stderr.
#[derive(Debug)]
pub struct Captured {
    pub status: ExitStatus,
    pub stdout: Vec<u8>,
    pub stderr: Vec<u8>,
}

/// Execute with a positive timeout up to sixty seconds and 256 KiB per stream.
/// The deadline initiates retained-child cleanup; kernel reaping can extend it.
pub fn run(executable: &Path, timeout: Duration) -> Result<Captured, String> {
    run_command(Command::new(executable), timeout)
}

/// Round a checked positive duration upward without allowing a zero deadline.
fn timeout_ms(timeout: Duration) -> Result<u64, String> {
    if timeout.is_zero() || timeout > Duration::from_secs(60) {
        return Err("test timeout must be greater than zero and at most 60 seconds".into());
    }
    Ok(timeout.as_nanos().div_ceil(1_000_000) as u64)
}

/// Keep native spool ownership separate from Rust; only remove the empty parent.
struct Directory(PathBuf);
impl Directory {
    /// Allocate an exclusive private directory; no existing path is reused.
    fn new() -> Result<Self, String> {
        use std::os::unix::fs::DirBuilderExt;
        let epoch = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .map_err(|error| error.to_string())?
            .as_nanos();
        for attempt in 0..100 {
            let path = std::env::temp_dir().join(format!(
                ".fern-test-{}-{epoch}-{attempt}",
                std::process::id()
            ));
            match std::fs::DirBuilder::new().mode(0o700).create(&path) {
                Ok(()) => return Ok(Self(path)),
                Err(error) if error.kind() == std::io::ErrorKind::AlreadyExists => continue,
                Err(error) => return Err(format!("cannot create test capture directory: {error}")),
            }
        }
        Err("cannot allocate test capture directory".into())
    }
}
impl Drop for Directory {
    /// Never traverse remaining or replaced spool names; the helper owns those.
    fn drop(&mut self) {
        let _ = std::fs::remove_dir(&self.0);
    }
}

/// Resolve the trusted native component; tests inject one stable compiled fixture.
fn helper() -> Result<PathBuf, String> {
    #[cfg(not(test))]
    {
        super::component("FERN_TEST_SUPERVISOR", "fern-test-supervisor")
    }
    #[cfg(test)]
    {
        tests::helper()
    }
}

/// Pass literal executable/arguments and capture only the bounded protocol pipe.
/// A taken stdin guard remains alive through wait; wait must not signal parent death.
fn run_command(command: Command, timeout: Duration) -> Result<Captured, String> {
    timeout_ms(timeout)?;
    run_supervised(command, Command::new(helper()?), timeout)
}

/// Execute a trusted supervisor command; tests can inject stable protocol producers.
fn run_supervised(
    command: Command,
    mut supervisor: Command,
    timeout: Duration,
) -> Result<Captured, String> {
    let milliseconds = timeout_ms(timeout)?;
    let directory = Directory::new()?;
    let executable = std::fs::canonicalize(command.get_program())
        .map_err(|error| format!("cannot execute doc test: {error}"))?;
    let mut child = supervisor
        .arg(milliseconds.to_string())
        .arg(&directory.0)
        .arg("--")
        .arg(executable)
        .args(command.get_args())
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .stderr(Stdio::null())
        .spawn()
        .map_err(|error| format!("cannot execute test supervisor: {error}"))?;
    let liveness = child.stdin.take();
    let mut output = Vec::with_capacity(FRAME_MAX + 1);
    let read = child
        .stdout
        .take()
        .ok_or("missing test supervisor stdout")?
        .take(FRAME_MAX as u64 + 1)
        .read_to_end(&mut output);
    // On read failure/overflow, closing liveness asks the helper to finish cleanup.
    if read.is_err() || output.len() > FRAME_MAX {
        drop(liveness);
    } else {
        let status = child.wait().map_err(|error| error.to_string())?;
        drop(liveness);
        read.map_err(|error| format!("test supervisor protocol: {error}"))?;
        if !status.success() {
            return Err("test supervisor failed to publish a complete result".into());
        }
        return decode(&output);
    }
    let _ = child.wait();
    Err("test supervisor protocol exceeds capture limit or cannot be read".into())
}

#[cfg(all(test, unix))]
mod tests {
    use super::*;
    use std::time::Instant;

    /// Build stable native fixture executables once; never execute written shell scripts.
    fn artifacts() -> Result<&'static (super::super::Workspace, PathBuf, PathBuf), String> {
        type Artifacts = (super::super::Workspace, PathBuf, PathBuf);
        static ARTIFACTS: std::sync::OnceLock<Result<Artifacts, String>> =
            std::sync::OnceLock::new();
        ARTIFACTS
            .get_or_init(|| {
                let workspace = super::super::Workspace::new(&std::env::temp_dir())
                    .map_err(|e| e.to_string())?;
                let helper = compile_fixture(&workspace, "tools/test_supervisor.c", "helper")?;
                let child = compile_fixture(
                    &workspace,
                    "tests/fixtures/test_supervisor_child.c",
                    "child",
                )?;
                Ok((workspace, helper, child))
            })
            .as_ref()
            .map_err(Clone::clone)
    }

    /// Compile one authored C fixture with an explicit bounded argument vector.
    fn compile_fixture(
        workspace: &super::super::Workspace,
        source: &str,
        name: &str,
    ) -> Result<PathBuf, String> {
        let executable = workspace.file(name);
        let source = Path::new(env!("CARGO_MANIFEST_DIR"))
            .join("..")
            .join(source);
        let result = Command::new("cc")
            .args(["-std=c11", "-O2", "-Wall", "-Wextra", "-Werror"])
            .arg(source)
            .arg("-o")
            .arg(&executable)
            .output()
            .map_err(|e| e.to_string())?;
        if !result.status.success() {
            return Err(String::from_utf8_lossy(&result.stderr).into_owned());
        }
        Ok(executable)
    }

    /// Return the stable helper without changing process-global environment variables.
    pub(super) fn helper() -> Result<PathBuf, String> {
        Ok(artifacts()?.1.clone())
    }

    /// Return the stable literal-argument lifecycle child fixture.
    pub(super) fn fixture() -> Result<PathBuf, String> {
        Ok(artifacts()?.2.clone())
    }

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
        assert!(script("head -c 262145 /dev/zero", Duration::from_secs(3))
            .err()
            .unwrap()
            .contains("output limit"));
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

#[cfg(all(test, unix))]
#[path = "capture/frame_tests.rs"]
mod frame_tests;
