//! A pkg-config directory variable is one literal path, not escaped linker arguments.
#![deny(clippy::pedantic, clippy::nursery)]
#![deny(clippy::unwrap_used, clippy::expect_used, clippy::indexing_slicing)]
#![deny(clippy::as_conversions, clippy::unreachable, clippy::string_slice)]
#![deny(clippy::arithmetic_side_effects)]
use std::path::Path;

/// Borrow a validated UTF-8 path before any filesystem probe; only one terminal line ending is framing.
pub(super) fn parse(record: &[u8]) -> Result<&Path, &'static str> {
    if record.len() > 4098 {
        return Err("pkg-config library directory exceeds 4096 bytes");
    }
    let path = record
        .strip_suffix(b"\r\n")
        .or_else(|| record.strip_suffix(b"\n"))
        .unwrap_or(record);
    if path.len() > 4096 {
        return Err("pkg-config library directory exceeds 4096 bytes");
    }
    if path.is_empty() {
        return Err("pkg-config returned an empty library directory");
    }
    if path.contains(&0) {
        return Err("pkg-config library directory contains NUL");
    }
    if path.contains(&b'\n') || path.contains(&b'\r') {
        return Err("pkg-config library directory must be one line");
    }
    let text = std::str::from_utf8(path)
        .map_err(|_| "pkg-config returned a non-UTF-8 library directory")?;
    Ok(Path::new(text))
}

#[cfg(test)]
mod tests {
    use super::parse;
    use std::path::Path;
    #[test]
    fn exact_byte_limits_apply_after_only_the_single_line_ending() {
        let exact = format!("{}λ", "x".repeat(4094));
        for ending in ["", "\n", "\r\n"] {
            let record = format!("{exact}{ending}");
            assert_eq!(parse(record.as_bytes()), Ok(Path::new(&exact)));
            let too_long = format!("x{record}");
            assert!(parse(too_long.as_bytes()).is_err());
        }
    }
    #[test]
    fn literal_edges_are_not_shell_escaped_or_trimmed() {
        for name in [
            " ",
            "\tpath\t",
            "\u{a0}path\u{a0}",
            "'quoted'",
            "$HOME;$(literal)",
            "a\\b",
        ] {
            let record = format!("{name}\r\n");
            assert_eq!(parse(record.as_bytes()), Ok(Path::new(name)));
        }
    }
    #[test]
    fn malformed_records_never_publish_a_path() {
        for record in [
            b"".as_slice(),
            b"\n",
            b"\r\n",
            b"a\0",
            b"a\nb",
            b"a\r",
            b"a\n\n",
            b"\xff",
        ] {
            assert!(parse(record).is_err());
        }
    }
}
