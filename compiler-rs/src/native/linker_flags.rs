//! Bounded literal linker arguments; no shell evaluation or partial output.
#![deny(clippy::pedantic, clippy::nursery)]
#![deny(clippy::unwrap_used, clippy::expect_used, clippy::indexing_slicing)]
#![deny(clippy::as_conversions, clippy::unreachable, clippy::string_slice)]
#![deny(clippy::arithmetic_side_effects)]
use std::ffi::OsString;

const RECORD_MAX: usize = 65536;
const WORD_MAX: usize = 16384;
const WORDS_MAX: usize = 4096;

/// Decode pkg-config's shell-escaped `flags` into literal argv words.
/// Only quoting and escapes are recognized: variable/command expansion never occurs.
pub(super) fn parse(flags: &str) -> Result<Vec<OsString>, String> {
    if flags.len() > RECORD_MAX {
        return Err("pkg-config linker flags exceed 65536 bytes".into());
    }
    if flags.contains('\0') {
        return Err("pkg-config linker flags contain NUL".into());
    }
    let mut chars = flags.chars();
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
            (Some('\''), _) => append(&mut word, character)?,
            (_, '\\') => {
                let next = chars
                    .next()
                    .ok_or("pkg-config linker flags end in an escape")?;
                if quote == Some('"') && !matches!(next, '$' | '`' | '"' | '\\' | '\n') {
                    append(&mut word, '\\')?;
                }
                if next != '\n' {
                    append(&mut word, next)?;
                    started = true;
                }
            }
            (None, ' ' | '\t' | '\n') => {
                if started {
                    publish(&mut words, &mut word)?;
                    started = false;
                }
            }
            _ => {
                append(&mut word, character)?;
                started = true;
            }
        }
    }
    if quote.is_some() {
        return Err("pkg-config linker flags contain an unterminated quote".into());
    }
    if started {
        publish(&mut words, &mut word)?;
    }
    Ok(words)
}

/// Charge UTF-8 byte growth before allocation; an error discards the entire parse.
fn append(word: &mut String, character: char) -> Result<(), String> {
    if word.len().saturating_add(character.len_utf8()) > WORD_MAX {
        return Err("pkg-config linker word exceeds 16384 bytes".into());
    }
    word.push(character);
    Ok(())
}

/// Enforce the aggregate argument count before moving a completed word.
fn publish(words: &mut Vec<OsString>, word: &mut String) -> Result<(), String> {
    if words.len() == WORDS_MAX {
        return Err("pkg-config linker flags exceed 4096 arguments".into());
    }
    words.push(OsString::from(std::mem::take(word)));
    Ok(())
}
