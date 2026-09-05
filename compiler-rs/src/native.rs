//! Process-based native backend: no C pointers or unsafe Rust cross the boundary.
#[cfg(unix)]
use std::os::unix::fs::DirBuilderExt;
use std::{
    env,
    ffi::OsString,
    fs, io,
    path::{Path, PathBuf},
    process::{Command, Output},
    time::{SystemTime, UNIX_EPOCH},
};

/// A private directory owns all compilation artifacts and cleans up on every exit.
pub struct Workspace {
    path: PathBuf,
}

impl Workspace {
    /// Allocate an exclusive directory with restrictive Unix permissions.
    pub fn new(parent: &Path) -> io::Result<Self> {
        let epoch = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .map_err(io::Error::other)?
            .as_nanos();
        for attempt in 0..100 {
            let path = parent.join(format!(".fern-rs-{}-{epoch}-{attempt}", std::process::id()));
            let mut builder = fs::DirBuilder::new();
            #[cfg(unix)]
            builder.mode(0o700);
            match builder.create(&path) {
                Ok(()) => return Ok(Self { path }),
                Err(error) if error.kind() == io::ErrorKind::AlreadyExists => continue,
                Err(error) => return Err(error),
            }
        }
        Err(io::Error::new(
            io::ErrorKind::AlreadyExists,
            "cannot allocate compilation workspace",
        ))
    }

    /// Locate a compiler-owned file inside this workspace.
    pub fn file(&self, name: &str) -> PathBuf {
        self.path.join(name)
    }
}

impl Drop for Workspace {
    fn drop(&mut self) {
        let _ = fs::remove_dir_all(&self.path);
    }
}

/// Resolve a backend component beside the binary or in the development checkout.
fn component(variable: &str, filename: &str) -> Result<PathBuf, String> {
    if let Some(path) = env::var_os(variable) {
        return Ok(PathBuf::from(path));
    }
    let executable = env::current_exe().map_err(|e| e.to_string())?;
    if let Some(directory) = executable.parent() {
        let candidate = directory.join(filename);
        if candidate.is_file() {
            return Ok(candidate);
        }
    }
    let development = Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("../bin")
        .join(filename);
    if development.is_file() {
        return Ok(development);
    }
    Err(format!(
        "missing {filename}; run `just rust-build` or set {variable}"
    ))
}

/// Run a native tool using literal argument vectors, preserving failure diagnostics.
fn execute(command: &mut Command, stage: &str) -> Result<Output, String> {
    let output = command
        .output()
        .map_err(|error| format!("{stage}: {error}"))?;
    if !output.status.success() {
        return Err(format!(
            "{stage} failed ({}):\n{}{}",
            output.status,
            String::from_utf8_lossy(&output.stdout),
            String::from_utf8_lossy(&output.stderr)
        ));
    }
    Ok(output)
}

/// Read pkg-config output as linker arguments without invoking a shell.
fn package_flags(packages: &[&str]) -> Result<Vec<OsString>, String> {
    let output = Command::new("pkg-config")
        .arg("--libs")
        .args(packages)
        .output();
    if let Ok(output) = output {
        if output.status.success() {
            let flags = std::str::from_utf8(&output.stdout)
                .map_err(|_| "pkg-config returned non-UTF-8 linker flags")?;
            return parse_linker_flags(flags);
        }
    }
    Ok(packages
        .iter()
        .flat_map(|package| match *package {
            "bdw-gc" => vec!["-lgc"],
            "sqlite3" => vec!["-lsqlite3"],
            "openssl" => vec!["-lssl", "-lcrypto"],
            _ => Vec::new(),
        })
        .map(OsString::from)
        .collect())
}

/// Decode pkg-config's shell-escaped `flags` into literal argv words.
/// Only quoting and escapes are recognized: variable/command expansion never occurs.
fn parse_linker_flags(flags: &str) -> Result<Vec<OsString>, String> {
    let mut chars = flags.chars().peekable();
    let mut words = Vec::new();
    let mut word = String::new();
    let mut quote = None;
    let mut started = false;
    while let Some(character) = chars.next() {
        match (quote, character) {
            (Some('\''), '\'') | (Some('"'), '"') => quote = None,
            (None, '\'' | '"') => {
                quote = Some(character);
                started = true;
            }
            (Some('\''), _) => word.push(character),
            (_, '\\') => {
                let next = chars
                    .next()
                    .ok_or("pkg-config linker flags end in an escape")?;
                if quote == Some('"') && !matches!(next, '$' | '`' | '"' | '\\' | '\n') {
                    word.push('\\');
                }
                if next != '\n' {
                    word.push(next);
                    started = true;
                }
            }
            (None, c) if c.is_whitespace() => {
                if started {
                    words.push(OsString::from(std::mem::take(&mut word)));
                    started = false;
                }
            }
            _ => {
                word.push(character);
                started = true;
            }
        }
    }
    if quote.is_some() {
        return Err("pkg-config linker flags contain an unterminated quote".into());
    }
    if started {
        words.push(word.into());
    }
    Ok(words)
}

/// Match the reference compiler's static GC linkage when the archive is available.
fn gc_flags() -> Result<Vec<OsString>, String> {
    if let Ok(output) = Command::new("pkg-config")
        .args(["--variable=libdir", "bdw-gc"])
        .output()
    {
        if output.status.success() {
            let directory = String::from_utf8_lossy(&output.stdout);
            let archive = Path::new(directory.trim()).join("libgc.a");
            if archive.is_file() {
                return Ok(vec![archive.into_os_string()]);
            }
        }
    }
    package_flags(&["bdw-gc"])
}

/// Compile checked QBE in a private workspace and retain the resulting executable there.
pub fn compile(il: &str, workspace: &Workspace) -> Result<PathBuf, String> {
    let backend = component("FERN_QBE", "fern-qbe")?;
    let runtime = component("FERN_RUNTIME_LIB", "libfern_runtime.a")?;
    if !runtime.is_file() {
        return Err(format!(
            "runtime archive does not exist: {}",
            runtime.display()
        ));
    }
    let source = workspace.file("program.ssa");
    let assembly = workspace.file("program.s");
    let object = workspace.file("program.o");
    let executable = workspace.file("program");
    fs::write(&source, il).map_err(|e| e.to_string())?;
    execute(Command::new(backend).arg(&source).arg(&assembly), "QBE")?;
    let compiler = env::var_os("CC").unwrap_or_else(|| "cc".into());
    execute(
        Command::new(&compiler)
            .arg("-c")
            .arg(&assembly)
            .arg("-o")
            .arg(&object),
        "assembly",
    )?;
    execute(
        Command::new(&compiler)
            .arg(&object)
            .arg(runtime)
            .args(gc_flags()?)
            .args(package_flags(&["sqlite3", "openssl"])?)
            .arg("-pthread")
            .arg("-lm")
            .arg("-o")
            .arg(&executable),
        "link",
    )?;
    Ok(executable)
}

#[cfg(test)]
mod tests {
    use super::parse_linker_flags;
    use std::ffi::OsString;

    #[test]
    fn linker_words_preserve_escaped_quoted_and_literal_shell_characters() {
        let flags = parse_linker_flags(
            r#"-L/a\ b '-L/single quoted' "-L/double quoted" -lssl -Wl,$ORIGIN \$HOME \`literal\`"#,
        )
        .unwrap();
        let expected: Vec<OsString> = [
            "-L/a b",
            "-L/single quoted",
            "-L/double quoted",
            "-lssl",
            "-Wl,$ORIGIN",
            "$HOME",
            "`literal`",
        ]
        .into_iter()
        .map(Into::into)
        .collect();
        assert_eq!(flags, expected);
        assert_eq!(
            parse_linker_flags("  -framework\tSecurity\n").unwrap(),
            vec![OsString::from("-framework"), OsString::from("Security")]
        );
    }

    #[test]
    fn linker_words_handle_quote_adjacency_and_shell_double_quote_backslashes() {
        assert_eq!(
            parse_linker_flags(r#"-L"a b"/c '' "a\qb" "a\\b" "a\"b""#).unwrap(),
            vec![
                OsString::from("-La b/c"),
                OsString::from(""),
                OsString::from(r"a\qb"),
                OsString::from(r"a\b"),
                OsString::from("a\"b")
            ]
        );
    }

    #[test]
    fn linker_words_reject_truncated_quoting_and_escapes() {
        for malformed in ["-L'bad", "-L\"bad", "-Lbad\\", "\"bad\\"] {
            let error = parse_linker_flags(malformed).unwrap_err();
            assert!(error.contains("pkg-config"), "{error}");
        }
    }
}

pub mod capture;
