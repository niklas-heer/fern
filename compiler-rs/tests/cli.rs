//! CLI behavior tests that do not depend on the native backend.
use std::{
    fs,
    process::Command,
    sync::atomic::{AtomicUsize, Ordering},
};
static NEXT_FILE: AtomicUsize = AtomicUsize::new(0);

fn run_source(source: &str, command: &str) -> std::process::Output {
    let file = std::env::temp_dir().join(format!(
        "fern-rs-test-{}-{}-{}.fn",
        std::process::id(),
        command,
        NEXT_FILE.fetch_add(1, Ordering::Relaxed)
    ));
    fs::write(&file, source).unwrap();
    let output = Command::new(env!("CARGO_BIN_EXE_fern-rs"))
        .args([command, file.to_str().unwrap()])
        .output()
        .unwrap();
    fs::remove_file(file).unwrap();
    output
}

#[test]
fn help_describes_experimental_boundary() {
    let output = Command::new(env!("CARGO_BIN_EXE_fern-rs"))
        .arg("--help")
        .output()
        .unwrap();
    assert!(output.status.success());
    assert!(String::from_utf8_lossy(&output.stdout).contains("experimental"));
}

#[test]
fn check_reports_file_line_and_column() {
    let output = run_source("fn main():\n    unknown\n", "check");
    assert!(!output.status.success());
    let stderr = String::from_utf8_lossy(&output.stderr);
    assert!(stderr.contains(".fn:2:5:"), "{stderr}");
    assert!(!stderr.contains("panicked"));
}

#[test]
fn check_and_emit_need_no_backend() {
    for command in ["check", "emit"] {
        let output = run_source("fn main():\n    println(\"hello\")\n", command);
        assert!(
            output.status.success(),
            "{}",
            String::from_utf8_lossy(&output.stderr)
        );
    }
}

/// Keep CLI test artifacts isolated, including source/output aliases.
struct TestDirectory(std::path::PathBuf);
impl TestDirectory {
    fn new() -> Self {
        let path = std::env::temp_dir().join(format!(
            "fern-rs-cli-{}-{}",
            std::process::id(),
            NEXT_FILE.fetch_add(1, Ordering::Relaxed)
        ));
        fs::create_dir(&path).unwrap();
        Self(path)
    }
    fn invoke(
        &self,
        action: &str,
        source: &std::path::Path,
        output: Option<&std::path::Path>,
    ) -> std::process::Output {
        let mut command = Command::new(env!("CARGO_BIN_EXE_fern-rs"));
        command
            .current_dir(&self.0)
            .arg(action)
            .arg(source)
            .env("FERN_QBE", self.0.join("missing-backend"))
            .env("FERN_RUNTIME_LIB", source);
        if let Some(output) = output {
            command.arg("-o").arg(output);
        }
        command.output().unwrap()
    }
}
impl Drop for TestDirectory {
    fn drop(&mut self) {
        let _ = fs::remove_dir_all(&self.0);
    }
}

#[test]
fn refuses_source_output_collision_before_writing_or_running_backend() {
    let directory = TestDirectory::new();
    let source = directory.0.join("source.fn");
    let original = "fn main():\n    println(42)\n";
    fs::write(&source, original).unwrap();
    for action in ["emit", "build"] {
        let result = directory.invoke(action, &source, Some(&source));
        assert!(!result.status.success());
        assert!(
            String::from_utf8_lossy(&result.stderr).contains("overwrite source"),
            "{:?}",
            result
        );
        assert_eq!(fs::read_to_string(&source).unwrap(), original);
    }
    let extensionless = directory.0.join("source");
    fs::write(&extensionless, original).unwrap();
    let result = directory.invoke("build", &extensionless, None);
    assert!(!result.status.success());
    assert!(String::from_utf8_lossy(&result.stderr).contains("overwrite source"));
    assert_eq!(fs::read_to_string(extensionless).unwrap(), original);
}

#[cfg(unix)]
#[test]
fn refuses_symlink_and_hardlink_source_output_aliases() {
    let directory = TestDirectory::new();
    let source = directory.0.join("source.fn");
    let original = "fn main():\n    println(42)\n";
    fs::write(&source, original).unwrap();
    let symlink = directory.0.join("symlink");
    let hardlink = directory.0.join("hardlink");
    std::os::unix::fs::symlink(&source, &symlink).unwrap();
    fs::hard_link(&source, &hardlink).unwrap();
    for output in [&symlink, &hardlink] {
        for action in ["emit", "build"] {
            let result = directory.invoke(action, &source, Some(output));
            assert!(!result.status.success());
            assert!(
                String::from_utf8_lossy(&result.stderr).contains("overwrite source"),
                "{:?}",
                result
            );
            assert_eq!(fs::read_to_string(&source).unwrap(), original);
        }
    }
}

#[test]
fn failures_preserve_previous_output_and_remove_workspaces() {
    let directory = TestDirectory::new();
    let source = directory.0.join("source.fn");
    let output = directory.0.join("existing");
    fs::write(&source, "fn main():\n    println(42)\n").unwrap();
    fs::write(&output, "previous output").unwrap();
    let result = directory.invoke("build", &source, Some(&output));
    assert!(!result.status.success());
    assert!(String::from_utf8_lossy(&result.stderr).contains("QBE"));
    assert_eq!(fs::read_to_string(&output).unwrap(), "previous output");
    let output_directory = directory.0.join("directory-output");
    fs::create_dir(&output_directory).unwrap();
    let result = directory.invoke("emit", &source, Some(&output_directory));
    assert!(!result.status.success());
    assert!(output_directory.is_dir());
    assert!(fs::read_dir(&directory.0).unwrap().all(|entry| !entry
        .unwrap()
        .file_name()
        .to_string_lossy()
        .starts_with(".fern-rs-")));
}

#[cfg(unix)]
#[test]
fn emit_replaces_output_symlink_without_modifying_target() {
    let directory = TestDirectory::new();
    let source = directory.0.join("source.fn");
    let target = directory.0.join("target");
    let output = directory.0.join("output");
    fs::write(&source, "fn main():\n    println(42)\n").unwrap();
    fs::write(&target, "previous target").unwrap();
    std::os::unix::fs::symlink(&target, &output).unwrap();
    let result = directory.invoke("emit", &source, Some(&output));
    assert!(result.status.success(), "{:?}", result);
    assert_eq!(fs::read_to_string(&target).unwrap(), "previous target");
    assert!(!fs::symlink_metadata(&output)
        .unwrap()
        .file_type()
        .is_symlink());
    assert!(fs::read_to_string(output).unwrap().contains("function"));
}
