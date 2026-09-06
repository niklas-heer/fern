#!/usr/bin/env python3
"""Check the pinned-nightly lint contract through isolated, offline positive and negative crates."""
import os
from pathlib import Path
import subprocess
import tempfile
import re
import tomllib

ROOT = Path(__file__).resolve().parents[1]
REQUIRED = {"dbg_macro", "todo", "unimplemented", "exit", "unchecked_time_subtraction", "unused_peekable", "redundant_clone", "or_fun_call"}


def policy():
    """Fail before invoking Cargo when the agreed package policy is absent or weakened."""
    source = (ROOT / "compiler-rs/Cargo.toml").read_text()
    assert "[lints.clippy]" in source, "missing incremental strict Clippy policy"
    tables = source[source.index("[lints."):]
    denied = set(re.findall(r'^([a-z_]+) = "deny"$', tables, re.MULTILINE))
    assert REQUIRED <= denied, "production lint was weakened"
    assert "unchecked_duration_subtraction" not in tables, "renamed lint spelling"
    production = "#![cfg_attr(not(test), deny(clippy::panic, clippy::panic_in_result_fn))]"
    for crate in ["lib.rs", "main.rs"]:
        assert production in (ROOT / "compiler-rs/src" / crate).read_text(), "production panic policy missing"
    return tables, production


def run():
    """Exercise real lint names under the selected project toolchain without installing tools."""
    lint_tables, production = policy()
    failures = {
        "panic": "pub fn value()->u32 { panic!(\"unexpected\") }",
        "panic_in_result_fn": "pub fn value()->Result<u32,String> { panic!(\"unexpected\") }",
        "dbg_macro": "pub fn value(x:u32)->u32 { dbg!(x) }",
        "todo": "pub fn value()->u32 { todo!() }",
        "unimplemented": "pub fn value()->u32 { unimplemented!() }",
        "exit": "pub fn value() { std::process::exit(2); }",
        "unchecked_time_subtraction": "pub fn value(a:std::time::Instant,b:std::time::Duration)->std::time::Instant { a-b }",
        "unused_peekable": "pub fn value(x:&[u32])->u32 { let mut values=x.iter().peekable(); values.next().copied().unwrap_or(0) }",
        "redundant_clone": "pub fn value(x:String)->String { x.clone() }",
        "or_fun_call": "pub fn value(x:Option<String>)->String { x.unwrap_or(String::from(\"fallback\")) }",
    }
    environment = dict(os.environ)
    environment.pop("RUSTFLAGS", None)
    environment.pop("CARGO_ENCODED_RUSTFLAGS", None)
    with tempfile.TemporaryDirectory(prefix="fern-lint-policy-") as temporary:
        directory = Path(temporary)
        (directory / "src").mkdir()
        minimum = tomllib.loads((ROOT / "compiler-rs/Cargo.toml").read_text())["package"]["rust-version"]
        (directory / "Cargo.toml").write_text('[package]\nname="fern-lint-contract"\nversion="0.0.0"\nedition="2021"\n'
                                              + f'rust-version="{minimum}"\n\n' + lint_tables)
        boundary = (ROOT / "compiler-rs/src/source_directory.rs").read_text().split("use std", 1)[0]
        scoped = {
            "unwrap_used": "pub fn value(x:Option<u32>)->u32 { x.unwrap() }",
            "expect_used": "pub fn value(x:Option<u32>)->u32 { x.expect(\"required\") }",
            "indexing_slicing": "pub fn value(x:&[u32],n:usize)->u32 { x[n] }",
            "as_conversions": "pub fn value(x:u64)->u8 { x as u8 }",
            "unreachable": "pub fn value()->u32 { unreachable!() }",
            "string_slice": "pub fn value(x:&str)->&str { &x[1..] }",
            "arithmetic_side_effects": "pub fn value(a:u32,b:u32)->u32 { a+b }",
        }
        failures.update({lint: boundary + source for lint, source in scoped.items()})
        cases = {lint: (lint, source) for lint, source in failures.items()}
        cases["duration_subtraction"] = ("unchecked_time_subtraction",
            "pub fn value(a:std::time::Duration,b:std::time::Duration)->std::time::Duration { a-b }")
        for name, (lint, source) in cases.items():
            (directory / "src/lib.rs").write_text(production + "\n" + source)
            command = ["cargo", "clippy", "--offline", "--manifest-path", str(directory / "Cargo.toml"), "--", "-D", "warnings"]
            result = subprocess.run(command, cwd=ROOT, env=environment, capture_output=True, text=True, timeout=60)
            assert result.returncode != 0, f"{name} was accepted"
            assert lint.replace("_", "-") in result.stderr or f"clippy::{lint}" in result.stderr, (lint, result.stderr)
            assert "unknown lint" not in result.stderr, result.stderr
        (directory / "src/lib.rs").write_text("pub fn value(a:std::time::Instant,b:std::time::Duration)->Option<std::time::Instant>{a.checked_sub(b)}\n")
        result = subprocess.run(command, cwd=ROOT, env=environment, capture_output=True, text=True, timeout=60)
        assert result.returncode == 0, result.stderr
        (directory / "src/lib.rs").write_text(production + '\n#[cfg(test)] mod tests { #[test] fn assertion() { panic!("test oracle"); } }\n')
        test_command = command[:-3] + ["--tests", "--", "-D", "warnings"]
        result = subprocess.run(test_command, cwd=ROOT, env=environment, capture_output=True, text=True, timeout=60)
        assert result.returncode == 0, result.stderr
    print(f"Rust lint policy: {len(cases)} forbidden cases, checked positive and test-only panic passed")


if __name__ == "__main__":
    run()
