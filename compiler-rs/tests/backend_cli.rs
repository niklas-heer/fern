//! Backend selection uses real CLI processes and isolated fake native tools.
#![cfg(unix)]
use std::{
    fs,
    os::unix::fs::PermissionsExt,
    path::PathBuf,
    process::{Command, Output},
    sync::atomic::{AtomicUsize, Ordering},
};

static NEXT: AtomicUsize = AtomicUsize::new(0);
struct Fixture(PathBuf);
impl Fixture {
    fn new() -> Self {
        let path = std::env::temp_dir().join(format!(
            "fern-backend-cli-{}-{}",
            std::process::id(),
            NEXT.fetch_add(1, Ordering::Relaxed)
        ));
        fs::create_dir(&path).unwrap();
        fs::write(path.join("source.fn"), "fn main(): ()\n").unwrap();
        fs::write(path.join("runtime.a"), "runtime fixture").unwrap();
        let fixture = Self(path);
        fixture.script(
            "qbe",
            "printf 'qbe\\n' >> \"$TOOL_LOG\"\n/bin/cp \"$1\" \"$2\"\n",
        );
        fixture.script("pkg-config", "exit 1\n");
        fixture.script(
            "cc",
            r#"printf '%s\n' "$1" >> "$TOOL_LOG"
for last do :; done
if [ "$1" = '-c' ]; then printf 'assembly object' > "$last"; exit 0; fi
if [ "${FAIL_LINK:-0}" = 1 ]; then printf 'partial artifact' > "$last"; exit 17; fi
/bin/cat > "$last" <<'PROGRAM'
#!/bin/sh
printf '%s\n' "$@"
PROGRAM
/bin/chmod 755 "$last"
"#,
        );
        fixture
    }
    fn script(&self, name: &str, body: &str) {
        let path = self.0.join(name);
        fs::write(&path, format!("#!/bin/sh\n{body}")).unwrap();
        fs::set_permissions(path, fs::Permissions::from_mode(0o755)).unwrap();
    }
    fn command(&self, arguments: &[&str]) -> Command {
        let mut command = Command::new(env!("CARGO_BIN_EXE_fern-rs"));
        command
            .current_dir(&self.0)
            .args(arguments)
            .env("PATH", &self.0)
            .env("FERN_QBE", self.0.join("qbe"))
            .env("FERN_RUNTIME_LIB", self.0.join("runtime.a"))
            .env("CC", self.0.join("cc"))
            .env("TOOL_LOG", self.0.join("tools.log"));
        command
    }
    fn run(&self, arguments: &[&str]) -> Output {
        self.command(arguments).output().unwrap()
    }
    fn retained(&self) {
        assert_eq!(fs::read(self.0.join("output")).unwrap(), b"retained");
        assert!(!fs::read_dir(&self.0).unwrap().any(|entry| entry
            .unwrap()
            .file_name()
            .to_string_lossy()
            .starts_with(".fern-rs-")));
    }
}
impl Drop for Fixture {
    fn drop(&mut self) {
        let _ = fs::remove_dir_all(&self.0);
    }
}

#[test]
fn explicit_qbe_matches_default_emission_without_native_tools() {
    let fixture = Fixture::new();
    let default = fixture.run(&["emit", "source.fn"]);
    assert!(default.status.success(), "{default:?}");
    for arguments in [
        ["emit", "--backend=qbe", "source.fn"],
        ["emit", "source.fn", "--backend=qbe"],
    ] {
        let explicit = fixture.run(&arguments);
        assert!(explicit.status.success(), "{explicit:?}");
        assert_eq!(explicit.stdout, default.stdout);
    }
    assert!(!fixture.0.join("tools.log").exists());
    let separate = fixture.run(&["emit", "--backend", "qbe", "source.fn"]);
    assert!(separate.status.success(), "{separate:?}");
    assert_eq!(separate.stdout, default.stdout);
}

#[test]
fn invalid_duplicate_or_misplaced_backend_fails_before_output_or_tools() {
    let fixture = Fixture::new();
    fs::write(fixture.0.join("output"), "retained").unwrap();
    for (arguments, message) in [
        (
            vec!["build", "source.fn", "-o", "output", "--backend=unknown"],
            "backend must be qbe or cranelift",
        ),
        (
            vec!["build", "source.fn", "-o", "output", "--backend="],
            "backend must be qbe or cranelift",
        ),
        (
            vec!["build", "source.fn", "--backend"],
            "--backend requires qbe or cranelift",
        ),
        (
            vec!["build", "source.fn", "--backend", "--quiet"],
            "backend must be qbe or cranelift",
        ),
        (
            vec![
                "build",
                "source.fn",
                "--backend",
                "qbe",
                "--backend=cranelift",
            ],
            "backend specified more than once",
        ),
        (
            vec![
                "build",
                "source.fn",
                "-o",
                "output",
                "--backend=qbe",
                "--backend=qbe",
            ],
            "backend specified more than once",
        ),
        (
            vec!["check", "source.fn", "--backend=qbe"],
            "--backend is only valid for emit/build/run",
        ),
        (
            vec!["emit", "source.fn", "-o", "output", "--backend=cranelift"],
            "Cranelift supports build/run",
        ),
    ] {
        let result = fixture.run(&arguments);
        assert!(!result.status.success(), "{result:?}");
        assert!(
            String::from_utf8_lossy(&result.stderr).contains(message),
            "{result:?}"
        );
        fixture.retained();
    }
    assert!(!fixture.0.join("tools.log").exists());
}

#[test]
fn run_tail_preserves_backend_looking_and_literal_arguments() {
    let fixture = Fixture::new();
    let result = fixture.run(&[
        "run",
        "--backend=qbe",
        "source.fn",
        "--",
        "--backend=unknown",
        "--quiet",
        "",
        "a b",
        "$(touch INJECTED)",
    ]);
    assert!(result.status.success(), "{result:?}");
    assert_eq!(
        result.stdout,
        b"--backend=unknown\n--quiet\n\na b\n$(touch INJECTED)\n"
    );
    assert!(!fixture.0.join("INJECTED").exists());
}

#[test]
fn explicit_backend_link_failure_preserves_destination_and_cleans_staging() {
    let fixture = Fixture::new();
    fs::write(fixture.0.join("output"), "retained").unwrap();
    let mut backends = vec!["--backend=qbe"];
    if cfg!(feature = "cranelift") {
        backends.push("--backend=cranelift");
    }
    for backend in backends {
        let result = fixture
            .command(&["build", backend, "source.fn", "-o", "output"])
            .env("FAIL_LINK", "1")
            .output()
            .unwrap();
        assert!(!result.status.success(), "{result:?}");
        assert!(
            String::from_utf8_lossy(&result.stderr).contains("link failed"),
            "{result:?}"
        );
        fixture.retained();
    }
}

#[test]
fn every_available_backend_rejects_source_aliases_before_native_tools() {
    let fixture = Fixture::new();
    fs::hard_link(fixture.0.join("source.fn"), fixture.0.join("hard-link")).unwrap();
    std::os::unix::fs::symlink("source.fn", fixture.0.join("symbolic-link")).unwrap();
    let mut backends = vec!["--backend=qbe"];
    if cfg!(feature = "cranelift") {
        backends.push("--backend=cranelift");
    }
    for backend in backends {
        for output in ["source.fn", "hard-link", "symbolic-link"] {
            let result = fixture.run(&["build", "source.fn", backend, "-o", output]);
            assert!(!result.status.success(), "{result:?}");
            assert!(
                String::from_utf8_lossy(&result.stderr).contains("refusing to overwrite source"),
                "{result:?}"
            );
            assert_eq!(
                fs::read(fixture.0.join("source.fn")).unwrap(),
                b"fn main(): ()\n"
            );
        }
    }
    assert!(!fixture.0.join("tools.log").exists());
}

#[cfg(not(feature = "cranelift"))]
#[test]
fn uncompiled_backend_is_an_explicit_error_without_qbe_fallback() {
    let fixture = Fixture::new();
    fs::write(fixture.0.join("output"), "retained").unwrap();
    let result = fixture.run(&["build", "source.fn", "-o", "output", "--backend=cranelift"]);
    assert!(!result.status.success(), "{result:?}");
    assert!(
        String::from_utf8_lossy(&result.stderr).contains("rebuild with --features cranelift"),
        "{result:?}"
    );
    assert!(!fixture.0.join("tools.log").exists());
    fixture.retained();
}

#[cfg(feature = "cranelift")]
#[test]
fn cranelift_object_pipeline_skips_qbe_and_assembly_and_preserves_run_tail() {
    let fixture = Fixture::new();
    let result = fixture
        .command(&[
            "run",
            "source.fn",
            "--backend=cranelift",
            "--",
            "--backend=qbe",
            "a b",
        ])
        .env("FERN_QBE", fixture.0.join("absent-qbe"))
        .output()
        .unwrap();
    assert!(result.status.success(), "{result:?}");
    assert_eq!(result.stdout, b"--backend=qbe\na b\n");
    let log = fs::read_to_string(fixture.0.join("tools.log")).unwrap();
    assert_eq!(log.lines().count(), 1, "{log}");
    assert!(log.trim_end().ends_with("program.o"), "{log}");
}
