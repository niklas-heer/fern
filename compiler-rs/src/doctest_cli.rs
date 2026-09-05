//! Explicit native documentation-test command with bounded discovery and execution.
use super::native;
use fern_prototype::{check, doctest, modules, qbe};
use std::{
    collections::HashMap,
    ffi::OsString,
    fs,
    io::Read,
    path::{Path, PathBuf},
    time::Duration,
};
struct Options {
    path: PathBuf,
    timeout: Duration,
}

/// Run source-owned examples in independent native processes and report every failed example.
pub(super) fn run(arguments: Vec<OsString>) -> Result<u8, String> {
    if arguments.len() == 2 && (arguments[1] == "--help" || arguments[1] == "-h") {
        println!("Usage: fern-rs test [--doc] [source.fn|directory] [--timeout seconds]\nExecute fenced Fern documentation examples and # => pattern expectations.\nDefaults to the current directory and a 10-second timeout per example (1–60).\nExamples execute user code. General unit-test syntax, coverage and watch mode are not yet supported.");
        return Ok(0);
    }
    let options = options(arguments)?;
    let files = super::documentation_cli::sources(&options.path)?;
    let mut passed = 0;
    let mut total = 0;
    let mut bytes = 0;
    for file in files {
        let mut source = String::new();
        fs::File::open(&file)
            .map_err(|error| format!("{}: {error}", file.display()))?
            .take(1024 * 1024 + 1)
            .read_to_string(&mut source)
            .map_err(|error| error.to_string())?;
        bytes += source.len();
        if bytes > 8 * 1024 * 1024 {
            return Err("doc test source exceeds 8 MiB".into());
        }
        let examples = doctest::extract(&source)
            .map_err(|error| format!("{}: {}", file.display(), error.message))?;
        for example in examples {
            total += 1;
            if total > 256 {
                return Err("doc test example limit exceeds 256".into());
            }
            match execute(&file, &source, &example, options.timeout) {
                Ok(()) => passed += 1,
                Err(error) => {
                    let line = source[..example.doc_span.start]
                        .bytes()
                        .filter(|b| *b == b'\n')
                        .count()
                        + 1;
                    eprintln!(
                        "{}:{line}: doc example {} failed: {error}",
                        file.display(),
                        example.ordinal
                    );
                }
            }
        }
    }
    println!("doc tests: {passed}/{total} passed");
    Ok(u8::from(passed != total))
}

/// Preserve ordinary module/private visibility through an overlay, then select only the test entry.
fn execute(
    path: &Path,
    source: &str,
    example: &doctest::Example,
    timeout: Duration,
) -> Result<(), String> {
    let prepared = doctest::prepare(source, example).map_err(|error| error.message)?;
    let path = modules::source_identity(path).map_err(|error| error.message)?;
    let overlays = HashMap::from([(path.clone(), prepared.source)]);
    let loaded = modules::load_with_sources(&path, &overlays).map_err(|error| error.message)?;
    let offset = loaded
        .sources()
        .find(|source| source.path == path)
        .ok_or("missing doc source identity")?
        .start;
    let selected = loaded
        .program
        .functions
        .iter()
        .find(|function| function.span.start == offset + prepared.function_span.start)
        .ok_or("missing resolved doc function")?
        .name
        .clone();
    let mut program = check::check_library(&loaded.program).map_err(|error| error.message)?;
    doctest::select_entry(&mut program, &selected).map_err(|error| error.message)?;
    let il = qbe::emit(&program).map_err(|error| error.message)?;
    let workspace =
        native::Workspace::new(&std::env::temp_dir()).map_err(|error| error.to_string())?;
    let executable = native::compile(&il, &workspace)?;
    let result = native::capture::run(&executable, timeout)?;
    if result.status.success() {
        return Ok(());
    }
    Err(format!(
        "expected pattern mismatch or runtime failure ({})\n{}{}",
        result.status,
        String::from_utf8_lossy(&result.stdout),
        String::from_utf8_lossy(&result.stderr)
    ))
}

/// Parse one optional source path and a bounded whole-second timeout; reject unsupported modes.
fn options(arguments: Vec<OsString>) -> Result<Options, String> {
    let mut path = None;
    let mut timeout = None;
    let mut doc = false;
    let mut arguments = arguments.into_iter().skip(1);
    while let Some(argument) = arguments.next() {
        if argument == "--doc" {
            if doc {
                return Err("--doc specified more than once".into());
            }
            doc = true;
        } else if argument == "--timeout" {
            if timeout.is_some() {
                return Err("--timeout specified more than once".into());
            }
            let seconds = arguments
                .next()
                .and_then(|s| s.to_str().and_then(|s| s.parse::<u64>().ok()))
                .ok_or("--timeout requires seconds from 1 to 60")?;
            if !(1..=60).contains(&seconds) {
                return Err("--timeout requires seconds from 1 to 60".into());
            }
            timeout = Some(Duration::from_secs(seconds));
        } else if argument.to_string_lossy().starts_with('-') {
            return Err(format!(
                "unknown test option: {}",
                argument.to_string_lossy()
            ));
        } else if path.replace(PathBuf::from(argument)).is_some() {
            return Err("test accepts one source file or directory".into());
        }
    }
    Ok(Options {
        path: path.unwrap_or_else(|| PathBuf::from(".")),
        timeout: timeout.unwrap_or(Duration::from_secs(10)),
    })
}
