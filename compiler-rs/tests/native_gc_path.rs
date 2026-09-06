//! Real child-process metadata, fake tool execution, and no process-global environment mutation.
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
            "fern-gc-path-{}-{}",
            std::process::id(),
            NEXT.fetch_add(1, Ordering::Relaxed)
        ));
        fs::create_dir(&path).unwrap();
        fs::create_dir(path.join("tools")).unwrap();
        fs::write(path.join("source.fn"), "fn main():()\n").unwrap();
        fs::write(path.join("runtime.a"), "").unwrap();
        let fixture = Self(path);
        fixture.script("qbe", "/bin/cp \"$1\" \"$2\"\n");
        fixture.script(
            "cc",
            r#"if [ "$1" != '-c' ]; then printf '%s\0' "$@" > "$LINK_ARGS"; fi
for last do :; done
printf 'fake artifact' > "$last"
"#,
        );
        fixture.script(
            "pkg-config",
            r#"printf '%s\n' "$@" >> "$PKG_LOG"
if [ "$1" = '--variable=libdir' ]; then /bin/cat "$METADATA"; exit "${METADATA_STATUS:-0}"; fi
printf '%s\n' '-lGCfallback'
"#,
        );
        fixture
    }
    fn script(&self, name: &str, body: &str) {
        let path = self.0.join("tools").join(name);
        fs::write(&path, format!("#!/bin/sh\n{body}")).unwrap();
        fs::set_permissions(path, fs::Permissions::from_mode(0o755)).unwrap();
    }
    fn archive(&self, path: &str) {
        fs::create_dir_all(self.0.join(path)).unwrap();
        fs::write(self.0.join(path).join("libgc.a"), "archive").unwrap();
    }
    fn run(&self, metadata: &[u8], status: &str) -> Output {
        fs::write(self.0.join("metadata"), metadata).unwrap();
        let _ = fs::remove_file(self.0.join("link_args"));
        let _ = fs::remove_file(self.0.join("pkg_log"));
        Command::new(env!("CARGO_BIN_EXE_fern-rs"))
            .current_dir(&self.0)
            .args(["build", "source.fn", "-o", "output"])
            .env("PATH", self.0.join("tools"))
            .env("FERN_QBE", self.0.join("tools/qbe"))
            .env("FERN_RUNTIME_LIB", self.0.join("runtime.a"))
            .env("CC", self.0.join("tools/cc"))
            .env("METADATA", self.0.join("metadata"))
            .env("METADATA_STATUS", status)
            .env("LINK_ARGS", self.0.join("link_args"))
            .env("PKG_LOG", self.0.join("pkg_log"))
            .output()
            .unwrap()
    }
    fn linked(&self, word: &str) {
        let args = fs::read(self.0.join("link_args")).unwrap();
        assert!(
            args.split(|b| *b == 0).any(|arg| arg == word.as_bytes()),
            "{args:?}"
        );
    }
}
impl Drop for Fixture {
    fn drop(&mut self) {
        let _ = fs::remove_dir_all(&self.0);
    }
}
#[test]
fn directory_metadata_preserves_edge_whitespace_and_literal_shell_bytes() {
    let f = Fixture::new();
    for name in [
        " gc ",
        "\tgc\t",
        "\u{a0}gc\u{a0}",
        "gc ' $(touch INJECTED); ",
    ] {
        f.archive(name);
        f.archive(name.trim());
        let result = f.run(format!("{name}\n").as_bytes(), "0");
        assert!(result.status.success(), "{result:?}");
        f.linked(&format!("{name}/libgc.a"));
        assert!(!f.0.join("INJECTED").exists());
    }
}
#[test]
fn invalid_successful_metadata_cannot_select_decoys_or_fall_back() {
    let f = Fixture::new();
    f.archive("");
    f.archive("gc�");
    f.archive("line\nnext");
    for metadata in [
        b"".to_vec(),
        b"\n".to_vec(),
        b"\r\n".to_vec(),
        b"gc\xff\n".to_vec(),
        b"gc\0\n".to_vec(),
        b"line\nnext\n".to_vec(),
        b"gc\r".to_vec(),
        b"gc\n\n".to_vec(),
        vec![b'x'; 4097],
    ] {
        fs::write(f.0.join("output"), "retained").unwrap();
        let result = f.run(&metadata, "0");
        assert!(
            !result.status.success(),
            "metadata={metadata:?} result={result:?}"
        );
        assert!(String::from_utf8_lossy(&result.stderr).contains("pkg-config"));
        assert!(!f.0.join("link_args").exists());
        assert_eq!(fs::read_to_string(f.0.join("output")).unwrap(), "retained");
        assert!(!fs::read_to_string(f.0.join("pkg_log"))
            .unwrap()
            .contains("--libs"));
    }
}
#[test]
fn framing_removes_only_one_terminal_lf_or_crlf() {
    let f = Fixture::new();
    f.archive("gc");
    f.archive(" ");
    for metadata in [b"gc".as_slice(), b"gc\n", b"gc\r\n"] {
        let result = f.run(metadata, "0");
        assert!(result.status.success(), "{result:?}");
        f.linked("gc/libgc.a");
    }
    let result = f.run(b" \n", "0");
    assert!(result.status.success(), "{result:?}");
    f.linked(" /libgc.a");
}
#[test]
fn unavailable_failed_or_valid_missing_archive_retains_fallback() {
    let f = Fixture::new();
    let result = f.run(b"missing\n", "0");
    assert!(result.status.success(), "{result:?}");
    f.linked("-lGCfallback");
    let result = f.run(b"invalid\xff", "17");
    assert!(result.status.success(), "{result:?}");
    f.linked("-lGCfallback");
    fs::remove_file(f.0.join("tools/pkg-config")).unwrap();
    let result = f.run(b"unused", "0");
    assert!(result.status.success(), "{result:?}");
    f.linked("-lgc");
}
#[test]
fn exact_metadata_byte_limit_reaches_archive_lookup_or_fallback() {
    let f = Fixture::new();
    for ending in ["", "\n", "\r\n"] {
        let path = format!("{}λ{ending}", "x".repeat(4094));
        let result = f.run(path.as_bytes(), "0");
        assert!(result.status.success(), "{result:?}");
        f.linked("-lGCfallback");
    }
}

#[test]
fn invalid_utf8_cannot_select_the_replacement_character_archive() {
    let f = Fixture::new();
    f.archive("gc�");
    let result = f.run(b"gc\xff\n", "0");
    assert!(!result.status.success(), "{result:?}");
    assert!(!f.0.join("link_args").exists());
}
