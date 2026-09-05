//! Bounded source discovery and atomic publication for directory documentation.
use fern_prototype::documentation::{self, Output, SourceDocument};
use std::{
    fs,
    io::Read,
    path::{Path, PathBuf},
};

/// Parse every discovered source before protecting all input identities and writing output.
pub(super) fn run(root: &Path, output: Option<&Path>, format: Output) -> Result<u8, String> {
    let files = discover(root)?;
    let mut sources = Vec::new();
    let mut bytes = 0;
    for path in &files {
        let mut source = String::new();
        fs::File::open(path)
            .map_err(|error| format!("{}: {error}", path.display()))?
            .take(1024 * 1024 + 1)
            .read_to_string(&mut source)
            .map_err(|error| format!("{}: {error}", path.display()))?;
        bytes += source.len();
        if bytes > 8 * 1024 * 1024 {
            return Err("project documentation source exceeds 8 MiB".into());
        }
        sources.push(source);
    }
    let names: Vec<_> = files
        .iter()
        .map(|path| {
            path.strip_prefix(root)
                .unwrap_or(path)
                .components()
                .map(|part| part.as_os_str().to_string_lossy())
                .collect::<Vec<_>>()
                .join("/")
        })
        .collect();
    let documents: Vec<_> = names
        .iter()
        .zip(&sources)
        .map(|(path, source)| SourceDocument { path, source })
        .collect();
    let title = root.canonicalize().map_err(|error| error.to_string())?;
    let title = title.file_name().unwrap_or_default().to_string_lossy();
    let text =
        documentation::render_project(&documents, &title, format).map_err(|error| error.message)?;
    if let Some(output) = output {
        for file in &files {
            super::super::output_destination(file, output)?;
        }
        super::super::emit_file(&files[0], output, &text)?;
    } else {
        use std::io::Write;
        std::io::stdout()
            .lock()
            .write_all(text.as_bytes())
            .map_err(|error| error.to_string())?;
    }
    Ok(0)
}

/// Traverse without following child links; bound depth, entries, path bytes and source count.
pub(super) fn discover(root: &Path) -> Result<Vec<PathBuf>, String> {
    let mut pending = vec![(root.to_path_buf(), 0)];
    let mut files = Vec::new();
    let mut count = 0;
    while let Some((directory, depth)) = pending.pop() {
        if depth > 32 {
            return Err("documentation directory nesting exceeds 32".into());
        }
        for entry in
            fs::read_dir(&directory).map_err(|error| format!("{}: {error}", directory.display()))?
        {
            count += 1;
            if count > 8192 {
                return Err("documentation directory entry limit exceeded".into());
            }
            let entry = entry.map_err(|error| error.to_string())?;
            let path = entry.path();
            if path.as_os_str().len() > 4096 {
                return Err("documentation path exceeds 4096 bytes".into());
            }
            let kind = entry.file_type().map_err(|error| error.to_string())?;
            let name = entry.file_name();
            let name = name.to_string_lossy();
            if name.starts_with('.') || kind.is_symlink() {
                continue;
            }
            if kind.is_dir() {
                if !matches!(
                    name.as_ref(),
                    "target" | "build" | "bin" | "deps" | "node_modules"
                ) {
                    pending.push((path, depth + 1));
                }
            } else if kind.is_file() && path.extension().is_some_and(|extension| extension == "fn")
            {
                if files.len() == 256 {
                    return Err("documentation file limit exceeds 256".into());
                }
                files.push(path);
            }
        }
    }
    files.sort();
    if files.is_empty() {
        return Err("documentation directory contains no Fern source files".into());
    }
    Ok(files)
}
