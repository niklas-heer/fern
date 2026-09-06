//! Strict version-one framing; no native test payload can impersonate metadata.
use super::{Captured, FRAME_MAX, OUTPUT_MAX};
use std::os::unix::process::ExitStatusExt;

/// Parse canonical unsigned fields without signs, leading zeroes or overflow.
fn number(text: &str) -> Result<usize, String> {
    if text.is_empty()
        || (text.len() > 1 && text.starts_with('0'))
        || !text.bytes().all(|byte| byte.is_ascii_digit())
    {
        return Err("invalid test supervisor numeric field".into());
    }
    text.parse()
        .map_err(|_| "test supervisor numeric field overflow".into())
}

/// Validate an exited/signaled waitpid status rather than accepting stopped states.
fn status(code: usize) -> Result<std::process::ExitStatus, String> {
    #[cfg(target_os = "macos")]
    const SIGNAL_MAX: usize = 31;
    #[cfg(not(target_os = "macos"))]
    const SIGNAL_MAX: usize = 64;
    let signal = code & 127;
    if code > 65535 || (code & 255 != 0 && (code > 255 || signal == 0 || signal > SIGNAL_MAX)) {
        return Err("invalid test supervisor native status".into());
    }
    Ok(std::process::ExitStatus::from_raw(code as i32))
}

/// Map discriminated helper failures without conflating ordinary native exit125.
fn failure(code: usize) -> Result<Captured, String> {
    let message = match code {
        1 => "test supervisor rejected invocation arguments",
        2 => "cannot execute doc test: native spawn failed",
        3 => "doc test timed out",
        4 => "doc test output limit exceeded",
        5 => "test supervisor IO or ownership failure",
        6 => "test supervisor interrupted",
        7 => "test supervisor child ownership or reaper policy failure",
        8 => "test supervisor parent disconnected",
        _ => "invalid test supervisor error code",
    };
    Err(message.into())
}

/// Validate header, exact binary lengths, trailer and EOF before publishing values.
pub(super) fn decode(bytes: &[u8]) -> Result<Captured, String> {
    let end = bytes
        .iter()
        .take(128)
        .position(|byte| *byte == b'\n')
        .ok_or("invalid test supervisor header")?;
    if bytes.len() > FRAME_MAX {
        return Err("test supervisor frame limit exceeded".into());
    }
    let header =
        std::str::from_utf8(&bytes[..end]).map_err(|_| "invalid test supervisor header")?;
    let words: Vec<_> = header.split(' ').collect();
    if words.len() != 6 || words[0] != "FERN_TEST" || words[1] != "1" {
        return Err("incompatible test supervisor protocol".into());
    }
    let code = number(words[3])?;
    let out = number(words[4])?;
    let err = number(words[5])?;
    if out > OUTPUT_MAX || err > OUTPUT_MAX {
        return Err("doc test output limit exceeded".into());
    }
    let payload = end + 1;
    let tail = payload + out + err;
    if bytes.get(tail..) != Some(b"\nFERN_TEST_END 1\n") {
        return Err("incomplete or trailing test supervisor record".into());
    }
    match words[2] {
        "N" => Ok(Captured {
            status: status(code)?,
            stdout: bytes[payload..payload + out].to_vec(),
            stderr: bytes[payload + out..tail].to_vec(),
        }),
        "E" => failure(code),
        _ => Err("invalid test supervisor record kind".into()),
    }
}
