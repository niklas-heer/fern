use std::{
    fs,
    path::PathBuf,
    process::Command,
    sync::atomic::{AtomicUsize, Ordering},
};
static NEXT: AtomicUsize = AtomicUsize::new(0);
struct Directory(PathBuf);
impl Directory {
    fn new() -> Self {
        let path = std::env::temp_dir().join(format!(
            "fern-docs-{}-{}",
            std::process::id(),
            NEXT.fetch_add(1, Ordering::Relaxed)
        ));
        fs::create_dir(&path).unwrap();
        fs::write(
            path.join("library.fn"),
            "@doc \"\"\"A helper.\"\"\"\npub fn helper(value: Int) -> Int: value + 1\n",
        )
        .unwrap();
        Self(path)
    }
    fn run(&self, args: &[&str]) -> std::process::Output {
        Command::new(env!("CARGO_BIN_EXE_fern-rs"))
            .current_dir(&self.0)
            .args(args)
            .env("FERN_QBE", self.0.join("missing-backend"))
            .output()
            .unwrap()
    }
}
impl Drop for Directory {
    fn drop(&mut self) {
        let _ = fs::remove_dir_all(&self.0);
    }
}
#[test]
fn doc_prints_markdown_without_main_or_backend_and_writes_html() {
    let dir = Directory::new();
    let result = dir.run(&["doc", "library.fn"]);
    assert!(result.status.success(), "{:?}", result);
    assert!(String::from_utf8_lossy(&result.stdout).contains("A helper."));
    assert!(result.stderr.is_empty());
    let result = dir.run(&["doc", "--html", "-o", "docs.html", "library.fn"]);
    assert!(result.status.success(), "{:?}", result);
    assert!(fs::read_to_string(dir.0.join("docs.html"))
        .unwrap()
        .starts_with("<!doctype html>"));
    assert!(result.stdout.is_empty());
}
#[test]
fn doc_invalid_source_preserves_output_and_reports_source_location() {
    let dir = Directory::new();
    fs::write(dir.0.join("bad.fn"), "fn bad():\n    (\n").unwrap();
    fs::write(dir.0.join("output.md"), "preserved").unwrap();
    let result = dir.run(&["doc", "bad.fn", "-o", "output.md"]);
    assert_eq!(result.status.code(), Some(1));
    assert!(String::from_utf8_lossy(&result.stderr).contains("bad.fn:"));
    assert_eq!(
        fs::read_to_string(dir.0.join("output.md")).unwrap(),
        "preserved"
    );
}
#[test]
fn doc_rejects_source_aliases_and_malformed_options() {
    let dir = Directory::new();
    let original = fs::read(dir.0.join("library.fn")).unwrap();
    fs::hard_link(dir.0.join("library.fn"), dir.0.join("hard.fn")).unwrap();
    for destination in ["library.fn", "hard.fn"] {
        let result = dir.run(&["doc", "library.fn", "-o", destination]);
        assert_eq!(result.status.code(), Some(1), "{:?}", result);
        assert_eq!(fs::read(dir.0.join("library.fn")).unwrap(), original);
    }
    for args in [
        vec!["doc"],
        vec!["doc", "library.fn", "-o"],
        vec!["doc", "--html", "--html", "library.fn"],
        vec!["doc", "library.fn", "extra.fn"],
        vec!["doc", "--unknown", "library.fn"],
    ] {
        assert_eq!(dir.run(&args).status.code(), Some(1));
    }
}
#[cfg(unix)]
#[test]
fn doc_preserves_source_when_output_is_a_symlink_to_it() {
    let dir = Directory::new();
    std::os::unix::fs::symlink("library.fn", dir.0.join("alias.fn")).unwrap();
    let original = fs::read(dir.0.join("library.fn")).unwrap();
    let result = dir.run(&["doc", "library.fn", "-o", "alias.fn"]);
    assert_eq!(result.status.code(), Some(1));
    assert_eq!(fs::read(dir.0.join("library.fn")).unwrap(), original);
}

#[test]
fn doc_help_describes_formats_and_source_only_generation() {
    let dir = Directory::new();
    let result = dir.run(&["doc", "--help"]);
    assert!(result.status.success());
    let text = String::from_utf8(result.stdout).unwrap();
    assert!(text.contains("--html"));
    assert!(text.contains("-o"));
    assert!(text.contains("source"));
}
