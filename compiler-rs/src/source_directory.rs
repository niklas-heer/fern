//! Shared bounded discovery for explicit source-directory commands.
use std::{
    fs,
    path::{Path, PathBuf},
};

/// Traverse without following child links; bound depth, entries, path bytes and source count.
pub(super) fn discover(root: &Path, purpose: &str) -> Result<Vec<PathBuf>, String> {
    let mut pending = vec![(root.to_path_buf(), 0)];
    let mut files = Vec::new();
    let mut count = 0;
    while let Some((directory, depth)) = pending.pop() {
        if depth > 32 {
            return Err(format!("{purpose} directory nesting exceeds 32"));
        }
        for entry in
            fs::read_dir(&directory).map_err(|error| format!("{}: {error}", directory.display()))?
        {
            count += 1;
            if count > 8192 {
                return Err(format!("{purpose} directory entry limit exceeded"));
            }
            let entry = entry.map_err(|error| error.to_string())?;
            let path = entry.path();
            if path.as_os_str().len() > 4096 {
                return Err(format!("{purpose} path exceeds 4096 bytes"));
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
                    return Err(format!("{purpose} file limit exceeds 256"));
                }
                files.push(path);
            }
        }
    }
    files.sort();
    if files.is_empty() {
        return Err(format!("{purpose} directory contains no Fern source files"));
    }
    Ok(files)
}
