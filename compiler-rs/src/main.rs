//! Experimental CLI; parsing and type checking never call the C frontend.
#![forbid(unsafe_code)]
// Rust1.75 has no allow-panic-in-tests option; keep production and test scopes explicit.
#![cfg_attr(not(test), deny(clippy::panic, clippy::panic_in_result_fn))]
mod cli_controls;
mod doctest_cli;
mod documentation_cli;
mod format_cli;
mod native;
mod source_directory;
mod syntax_cli;
use fern_prototype::{check, modules, qbe};
use std::{
    env,
    ffi::OsString,
    fs,
    path::{Path, PathBuf},
    process::{Command, ExitCode},
};

/// Supported CLI actions and literal source/output paths.
struct Options {
    command: String,
    source: PathBuf,
    output: Option<PathBuf>,
    arguments: Vec<OsString>,
    format_check: bool,
    controls: cli_controls::Controls,
}

/// Print explicit user-requested help, even when informational output is quiet.
fn help() {
    println!(
            "fern-rs: experimental Rust frontend (C remains the default)\n\
Usage: fern-rs <check|emit|build|run|fmt|doc|lex|parse> <source.fn> [-o output]\n\
Run arguments: fern-rs run source.fn -- [arguments]\n\
Global controls: --quiet, --verbose, --color=auto|always|never; -v aliases --version.\n\
Subset: generic functions, custom types, modules, Int/Bool/String, List/Option/Result, guarded match, and Result ?.\n\
Documentation: fern-rs doc <source.fn|directory> [--html] [-o output] generates source documentation.\n\
Tests: fern-rs test --doc [source.fn|directory] executes documentation examples.\n\
Formatting: fern-rs fmt <source.fn|directory> updates sources after validating every file.\n\
Format validation: fern-rs fmt --check <source.fn|directory> checks canonical formatting without writing.\n\
Interactive evaluation: fern-rs repl retains successful bindings and typed functions.\n\
Editor protocol: fern-rs lsp communicates over standard input/output.\n\
Native builds: run mise run rust-build; FERN_QBE and FERN_RUNTIME_LIB override backend paths."
        );
}

/// Parse options without interpreting shell syntax or silently ignoring extra arguments.
fn options(
    arguments: Vec<OsString>,
    controls: cli_controls::Controls,
) -> Result<Option<Options>, String> {
    if arguments.is_empty() {
        return Err("Usage: fern-rs <command> [options] <source.fn>\nUse fern-rs --help for commands and global controls.".into());
    }
    if arguments[0] == "--help" || arguments[0] == "-h" {
        help();
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
    if !["check", "emit", "build", "run", "fmt", "lex", "parse"].contains(&command.as_str()) {
        return Err(format!("unknown command: {command}"));
    }
    let mut source = None;
    let mut output = None;
    let mut forwarded = Vec::new();
    let mut format_check = false;
    let mut rest = arguments.into_iter().skip(1);
    while let Some(argument) = rest.next() {
        if argument == "--" && source.is_some() && command == "run" {
            forwarded.extend(rest);
            break;
        }
        if argument == "--check" {
            if command != "fmt" {
                return Err("--check is only valid for fmt".into());
            }
            if std::mem::replace(&mut format_check, true) {
                return Err("--check specified more than once".into());
            }
        } else if argument == "-o" || argument == "--output" {
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
        format_check,
        controls,
    }))
}

/// Parse and check source before producing any artifacts or running backend tools.
fn run(options: Options) -> Result<u8, String> {
    if matches!(options.command.as_str(), "lex" | "parse") {
        return syntax_cli::run(&options.command, &options.source);
    }
    if options.command == "fmt" {
        return format_cli::run(&options.source, options.format_check);
    }
    let loaded = modules::load(&options.source).map_err(|error| error.message)?;
    let typed = check::check(&loaded.program).map_err(|error| loaded.render(error))?;
    if options.command == "check" {
        options
            .controls
            .information("No type errors (Rust prototype subset)");
        return Ok(0);
    }
    let il = qbe::emit(&typed).map_err(|error| loaded.render(error))?;
    if options.command == "emit" {
        if let Some(output) = options.output {
            emit_file(&options.source, &output, &il)?;
        } else {
            print!("{il}");
        }
        return Ok(0);
    }
    if options.command == "build" {
        return build(&options.source, options.output, &il, options.controls);
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
fn build(
    source: &Path,
    output: Option<PathBuf>,
    il: &str,
    controls: cli_controls::Controls,
) -> Result<u8, String> {
    let output = output.unwrap_or_else(|| PathBuf::from(source.file_stem().unwrap_or_default()));
    let destination = output_destination(source, &output)?;
    let parent = destination.parent().ok_or("invalid output directory")?;
    let workspace = native::Workspace::new(parent).map_err(|e| e.to_string())?;
    let executable = native::compile(il, &workspace)?;
    fs::rename(executable, destination).map_err(|e| format!("cannot install output: {e}"))?;
    controls.information(&format!("Created executable: {}", output.display()));
    Ok(0)
}

/// Dispatch only after global validation; data commands retain their own literal parsers.
fn dispatch(arguments: Vec<OsString>, controls: cli_controls::Controls) -> Result<u8, String> {
    controls.announce(&arguments);
    if arguments.first().is_some_and(|arg| arg == "test") {
        doctest_cli::run(arguments, controls)
    } else if arguments.first().is_some_and(|arg| arg == "doc") {
        documentation_cli::run(arguments)
    } else if arguments.first().is_some_and(|arg| arg == "repl") {
        if arguments.len() != 1 {
            Err("repl accepts no additional arguments".into())
        } else {
            use std::io::IsTerminal;
            fern_prototype::repl::serve(
                std::io::stdin().lock(),
                std::io::stdout().lock(),
                std::io::stdin().is_terminal() && !controls.quiet,
            )
            .map(|()| 0)
        }
    } else if arguments.first().is_some_and(|arg| arg == "lsp") {
        if arguments.len() != 1 {
            Err("lsp accepts no additional arguments".into())
        } else {
            fern_prototype::lsp::serve(std::io::stdin().lock(), std::io::stdout().lock())
                .map(|()| 0)
        }
    } else {
        options(arguments, controls).and_then(|options| options.map_or(Ok(0), run))
    }
}

/// Convert expected diagnostics and I/O failures into stable nonzero process exits.
fn main() -> ExitCode {
    let mut controls = cli_controls::Controls::default();
    let result = controls
        .arguments(env::args_os().skip(1).collect())
        .and_then(|arguments| dispatch(arguments, controls));
    match result {
        Ok(code) => ExitCode::from(code),
        Err(message) => {
            controls.error(&message);
            ExitCode::FAILURE
        }
    }
}
