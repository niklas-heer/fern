//! Literal-path documentation command; generation is independent of runtime/backend availability.
use fern_prototype::documentation::{self, Output};
use std::{ffi::OsString, fs, io::Read, path::PathBuf};
mod directory;
struct Options {
    source: PathBuf,
    output: Option<PathBuf>,
    format: Output,
}

/// Parse the doc action, validate source, then print or atomically install the complete document.
pub(super) fn run(arguments: Vec<OsString>) -> Result<u8, String> {
    if arguments.len() == 2 && (arguments[1] == "--help" || arguments[1] == "-h") {
        use std::io::Write;
        std::io::stdout().lock().write_all(b"Usage: fern-rs doc <source.fn|directory> [--html] [-o output]\nGenerate source documentation without executing code. Markdown is written to stdout by default. Directory HTML includes module navigation and local search.\n")
            .map_err(|error| error.to_string())?;
        return Ok(0);
    }
    let options = options(arguments)?;
    if options.source.is_dir() {
        return directory::run(&options.source, options.output.as_deref(), options.format);
    }
    let mut source = String::new();
    fs::File::open(&options.source)
        .map_err(|error| format!("{}: {error}", options.source.display()))?
        .take(1024 * 1024 + 1)
        .read_to_string(&mut source)
        .map_err(|error| format!("{}: {error}", options.source.display()))?;
    let title = options
        .source
        .file_name()
        .unwrap_or_default()
        .to_string_lossy();
    let rendered = documentation::render(&source, &title, options.format).map_err(|error| {
        let prefix = source.get(..error.span.start).unwrap_or("");
        let line = prefix.bytes().filter(|byte| *byte == b'\n').count() + 1;
        let column = prefix.rsplit('\n').next().unwrap_or("").chars().count() + 1;
        format!(
            "{}:{line}:{column}: error: {}",
            options.source.display(),
            error.message
        )
    })?;
    if let Some(output) = options.output {
        super::emit_file(&options.source, &output, &rendered)?;
    } else {
        use std::io::Write;
        std::io::stdout()
            .lock()
            .write_all(rendered.as_bytes())
            .map_err(|error| error.to_string())?;
    }
    Ok(0)
}

/// Accept one source, one optional output and an explicit HTML switch in any argument order.
fn options(arguments: Vec<OsString>) -> Result<Options, String> {
    let mut source = None;
    let mut output = None;
    let mut html = false;
    let mut arguments = arguments.into_iter().skip(1);
    while let Some(argument) = arguments.next() {
        if argument == "--html" {
            if html {
                return Err("--html specified more than once".into());
            }
            html = true;
        } else if argument == "-o" || argument == "--output" {
            if output.is_some() {
                return Err("output specified more than once".into());
            }
            output = Some(PathBuf::from(arguments.next().ok_or("-o requires a path")?));
        } else if argument.to_string_lossy().starts_with('-') {
            return Err(format!(
                "unknown doc option: {}",
                argument.to_string_lossy()
            ));
        } else if source.replace(PathBuf::from(argument)).is_some() {
            return Err("doc accepts one source file or directory".into());
        }
    }
    Ok(Options {
        source: source.ok_or("doc requires a source file or directory")?,
        output,
        format: if html { Output::Html } else { Output::Markdown },
    })
}

/// Share bounded directory discovery with the explicit executable documentation-test command.
pub(super) fn sources(path: &std::path::Path) -> Result<Vec<PathBuf>, String> {
    if path.is_dir() {
        directory::discover(path)
    } else {
        Ok(vec![path.to_path_buf()])
    }
}
