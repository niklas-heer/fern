//! Experimental CLI; parsing and type checking never call the C frontend.
#![forbid(unsafe_code)]
mod native;
use fern_prototype::{check, parse, qbe, Diagnostic};
use std::{
    env,
    ffi::OsString,
    fs,
    io::Read,
    path::{Path, PathBuf},
    process::{Command, ExitCode},
};

/// Supported CLI actions and literal source/output paths.
struct Options {
    command: String,
    source: PathBuf,
    output: Option<PathBuf>,
    arguments: Vec<OsString>,
}

/// Parse options without interpreting shell syntax or silently ignoring extra arguments.
fn options(arguments: Vec<OsString>) -> Result<Option<Options>, String> {
    if arguments.is_empty() || arguments[0] == "--help" || arguments[0] == "-h" {
        println!(
            "fern-rs: experimental Rust frontend (C remains the default)\n\
Usage: fern-rs <check|emit|build|run> <source.fn> [-o output]\n\
Run arguments: fern-rs run source.fn -- [arguments]\n\
Subset: typed functions, Int/Bool/String, List/Option/Result, let, if, match, and Result ?.\n\
Native builds: run just rust-build; FERN_QBE and FERN_RUNTIME_LIB override backend paths."
        );
        return Ok(None);
    }
    if arguments[0] == "--version" {
        println!("fern-rs {} (experimental)", env!("CARGO_PKG_VERSION"));
        return Ok(None);
    }
    let command = arguments[0]
        .to_str()
        .ok_or("command must be UTF-8")?
        .to_owned();
    if !["check", "emit", "build", "run"].contains(&command.as_str()) {
        return Err(format!("unknown command: {command}"));
    }
    let mut source = None;
    let mut output = None;
    let mut forwarded = Vec::new();
    let mut rest = arguments.into_iter().skip(1);
    while let Some(argument) = rest.next() {
        if argument == "--" && source.is_some() && command == "run" {
            forwarded.extend(rest);
            break;
        }
        if argument == "-o" || argument == "--output" {
            if !["emit", "build"].contains(&command.as_str()) {
                return Err("-o is only valid for emit/build".into());
            }
            if output.is_some() {
                return Err("output specified more than once".into());
            }
            output = Some(PathBuf::from(rest.next().ok_or("-o requires a path")?));
        } else if argument.to_string_lossy().starts_with('-') {
            return Err(format!(
                "unknown option: {} (use ./ for a source beginning with '-')",
                argument.to_string_lossy()
            ));
        } else if source.replace(PathBuf::from(argument)).is_some() {
            return Err("only one source file is accepted".into());
        }
    }
    Ok(Some(Options {
        command,
        source: source.ok_or("missing source file")?,
        output,
        arguments: forwarded,
    }))
}

/// Format a source diagnostic with Unicode-aware line/column and the offending line.
fn diagnostic(path: &Path, source: &str, error: Diagnostic) -> String {
    let mut offset = error.span.start.min(source.len());
    while !source.is_char_boundary(offset) {
        offset -= 1;
    }
    let before = &source[..offset];
    let line = before.bytes().filter(|byte| *byte == b'\n').count() + 1;
    let line_start = before.rfind('\n').map_or(0, |position| position + 1);
    let column = source[line_start..offset].chars().count() + 1;
    let text = source[line_start..].lines().next().unwrap_or("");
    format!(
        "{}:{line}:{column}: error: {}\n  {text}",
        path.display(),
        error.message
    )
}

/// Parse and check source before producing any artifacts or running backend tools.
fn run(options: Options) -> Result<u8, String> {
    let file = fs::File::open(&options.source)
        .map_err(|e| format!("{}: {e}", options.source.display()))?;
    let mut source = String::new();
    file.take(1024 * 1024 + 1)
        .read_to_string(&mut source)
        .map_err(|e| format!("{}: {e}", options.source.display()))?;
    let parsed = parse::parse(&source).map_err(|e| diagnostic(&options.source, &source, e))?;
    let typed = check::check(&parsed).map_err(|e| diagnostic(&options.source, &source, e))?;
    if options.command == "check" {
        println!("No type errors (Rust prototype subset)");
        return Ok(0);
    }
    let il = qbe::emit(&typed).map_err(|e| diagnostic(&options.source, &source, e))?;
    if options.command == "emit" {
        if let Some(output) = options.output {
            emit_file(&options.source, &output, &il)?;
        } else {
            print!("{il}");
        }
        return Ok(0);
    }
    if options.command == "build" {
        return build(&options.source, options.output, &il);
    }
    let workspace = native::Workspace::new(&env::temp_dir()).map_err(|e| e.to_string())?;
    let executable = native::compile(&il, &workspace)?;
    let status = Command::new(executable)
        .args(options.arguments)
        .status()
        .map_err(|e| e.to_string())?;
    #[cfg(unix)]
    {
        use std::os::unix::process::ExitStatusExt;
        Ok(status
            .code()
            .unwrap_or_else(|| 128 + status.signal().unwrap_or(1)) as u8)
    }
    #[cfg(not(unix))]
    Ok(status.code().unwrap_or(1) as u8)
}

/// Resolve `output` beside its canonical parent and reject aliases of `source`.
/// Canonical paths catch symlinks; Unix inode identities also catch hardlinks.
fn output_destination(source: &Path, output: &Path) -> Result<PathBuf, String> {
    let parent = output
        .parent()
        .filter(|p| !p.as_os_str().is_empty())
        .unwrap_or_else(|| Path::new("."))
        .canonicalize()
        .map_err(|e| format!("output directory: {e}"))?;
    let name = output.file_name().ok_or("invalid output filename")?;
    let destination = parent.join(name);
    let source_path = source.canonicalize().map_err(|e| e.to_string())?;
    let same_path = destination
        .canonicalize()
        .is_ok_and(|path| path == source_path);
    if same_path || same_file(source, &destination)? {
        return Err(format!(
            "refusing to overwrite source file: {}",
            output.display()
        ));
    }
    Ok(destination)
}

/// Compare `source` and `output` file identities where the platform exposes them.
/// A nonexistent output cannot alias the source; other metadata errors are reported.
fn same_file(source: &Path, output: &Path) -> Result<bool, String> {
    let output_metadata = match fs::metadata(output) {
        Ok(metadata) => metadata,
        Err(error) if error.kind() == std::io::ErrorKind::NotFound => return Ok(false),
        Err(error) => return Err(format!("output metadata: {error}")),
    };
    let source_metadata = fs::metadata(source).map_err(|error| error.to_string())?;
    #[cfg(unix)]
    {
        use std::os::unix::fs::MetadataExt;
        Ok(source_metadata.dev() == output_metadata.dev()
            && source_metadata.ino() == output_metadata.ino())
    }
    #[cfg(not(unix))]
    {
        let _ = (source_metadata, output_metadata);
        Ok(false)
    }
}

/// Atomically install `il` at `output` without truncating files on write failure.
/// Source aliases are rejected before a private staging directory is created.
fn emit_file(source: &Path, output: &Path, il: &str) -> Result<(), String> {
    let destination = output_destination(source, output)?;
    let parent = destination.parent().ok_or("invalid output directory")?;
    let workspace = native::Workspace::new(parent).map_err(|e| e.to_string())?;
    let staged = workspace.file("program.ssa");
    fs::write(&staged, il).map_err(|e| format!("cannot write output: {e}"))?;
    fs::rename(staged, destination).map_err(|e| format!("cannot install output: {e}"))
}

/// Build beside the final output and atomically replace it only after successful linking.
fn build(source: &Path, output: Option<PathBuf>, il: &str) -> Result<u8, String> {
    let output = output.unwrap_or_else(|| PathBuf::from(source.file_stem().unwrap_or_default()));
    let destination = output_destination(source, &output)?;
    let parent = destination.parent().ok_or("invalid output directory")?;
    let workspace = native::Workspace::new(parent).map_err(|e| e.to_string())?;
    let executable = native::compile(il, &workspace)?;
    fs::rename(executable, destination).map_err(|e| format!("cannot install output: {e}"))?;
    println!("Created executable: {}", output.display());
    Ok(0)
}

/// Convert expected diagnostics and I/O failures into stable nonzero process exits.
fn main() -> ExitCode {
    let result =
        options(env::args_os().skip(1).collect()).and_then(|options| options.map_or(Ok(0), run));
    match result {
        Ok(code) => ExitCode::from(code),
        Err(message) => {
            eprintln!("{message}");
            ExitCode::FAILURE
        }
    }
}
