#!/usr/bin/env python3
"""Opt-in macOS Zed smoke using an owned temporary profile; never installs into the normal profile."""
import argparse
import errno
import json
import os
from pathlib import Path
import pty
import selectors
import shutil
import signal
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
SOURCE = '''newtype Count derive(Json) = Count(Int)
type Item derive(Json):
    count:Count
fn encoded()->Result(String,json.Error):json.encode(Item(Count(3)))
type Choice = Int | String
fn size(value:Choice)->Int:
    match value:
        number:Int -> number
        text:String -> String.len(text)
fn add(left:Int,right:Int)->Int:left+right
fn main():
    println(add(right:2,left:1))
    match encoded():
        Ok(text)->println(text)
        Err(error)->println(json.error_message(error))
    for value in [1, 2]:
        println(size(value))
'''


def messages(path):
    """Read only complete bounded JSON-RPC frames from the actual transparent LSP transcript."""
    if not path.exists():
        return []
    data = path.read_bytes()
    if len(data) > 1048576:
        raise ValueError("oversized LSP smoke transcript")
    result = []
    while b"\r\n\r\n" in data:
        header, rest = data.split(b"\r\n\r\n", 1)
        length = int(header.split(b":", 1)[1])
        if len(rest) < length:
            break
        result.append(json.loads(rest[:length]))
        data = rest[length:]
    return result


def prepare(args, root, discovery):
    """Stage only into this test's profile and create a literal-path relay to the supplied compiler."""
    profile, project, tools = root / "profile", root / "project", root / "tools with spaces"
    (profile / "config").mkdir(parents=True)
    project.mkdir()
    tools.mkdir()
    shutil.copytree(args.package / "extension", profile / "extensions/installed/fern")
    (project / "smoke.fn").write_text(SOURCE)
    proxy = tools / ("fern-rs" if discovery else "Fern LSP proxy")
    fixture = (ROOT / "scripts/editor/zed_lsp_proxy.py").read_text()
    proxy.write_text("#!" + sys.executable + "\n" + fixture)
    proxy.chmod(0o755)
    (tools / "proxy.json").write_text(json.dumps({"rust": str(args.rust.resolve())}))
    settings = {"session": {"trust_all_worktrees": True}, "auto_update": False,
                "telemetry": {"metrics": False, "diagnostics": False}}
    if not discovery:
        settings["lsp"] = {"fern-lsp": {"binary": {"path": str(proxy), "arguments": ["lsp"]}}}
    (profile / "config/settings.json").write_text(json.dumps(settings))
    return profile, project, tools


def observed(tools):
    """Demand actual editor requests, a successful initialize result and empty diagnostics."""
    requests = messages(tools / "requests.bin")
    responses = messages(tools / "responses.bin")
    methods = {item.get("method") for item in requests}
    initialized = any(item.get("id") == 0 and "result" in item for item in responses)
    diagnostics = [item for item in responses if item.get("method") == "textDocument/publishDiagnostics"]
    if not initialized or "textDocument/didOpen" not in methods or not diagnostics:
        return False
    if diagnostics[-1]["params"]["diagnostics"]:
        raise ValueError("unexpected Fern diagnostics: " + str(diagnostics[-1]))
    invocation = json.loads((tools / "invocation.json").read_text())
    if invocation[1:] != ["lsp"]:
        raise ValueError("Zed did not pass exactly the LSP subcommand")
    if (tools / "lsp.stderr").read_bytes():
        raise ValueError("Fern LSP emitted stderr")
    return True


def wait_for_editor(process, master, tools, log):
    """Bound the real GUI launch, startup output and semantic protocol observation."""
    deadline, total = time.monotonic() + 30, 0
    selector = selectors.DefaultSelector()
    selector.register(master, selectors.EVENT_READ)
    try:
        while time.monotonic() < deadline:
            for _, _ in selector.select(0.1):
                try:
                    data = os.read(master, 65536)
                except OSError as error:
                    if error.errno == errno.EIO:
                        data = b""
                    else:
                        raise
                total += len(data)
                if total > 8388608:
                    raise ValueError("Zed smoke output exceeded 8 MiB")
                log.write(data)
                log.flush()
            if observed(tools):
                return
            if process.poll() is not None:
                raise ValueError("Zed exited before successful language registration")
        raise ValueError("Zed registration/LSP smoke deadline exceeded")
    finally:
        selector.close()


def smoke(args, root, discovery):
    """Launch and terminate only the PID owned by this invocation, with a separate profile per mode."""
    profile, project, tools = prepare(args, root, discovery)
    master, slave = pty.openpty()
    environment = dict(os.environ, PATH=str(tools) + os.pathsep + os.environ.get("PATH", ""),
                       ZED_LOG="info")
    process = subprocess.Popen([args.zed, "--user-data-dir", str(profile), str(project / "smoke.fn")],
                               env=environment, stdin=subprocess.DEVNULL, stdout=slave,
                               stderr=slave, start_new_session=True)
    os.close(slave)
    try:
        with (root / "zed.log").open("wb") as log:
            wait_for_editor(process, master, tools, log)
    finally:
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait(timeout=5)
        os.close(master)
    return root


def main():
    """Require explicit app/package/compiler paths; retain test-owned logs for review."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--zed", type=Path, required=True, help="Zed.app/Contents/MacOS/zed")
    parser.add_argument("--package", type=Path, required=True)
    parser.add_argument("--rust", type=Path, required=True)
    args = parser.parse_args()
    root = Path(tempfile.mkdtemp(prefix="fern-zed-smoke-"))
    print("Owned test data: " + str(root), flush=True)
    for name, discovery in [("override", False), ("discovery", True)]:
        directory = root / name
        directory.mkdir()
        smoke(args, directory, discovery)
        print("Zed staged package " + name + ": initialize/didOpen/clean diagnostics passed", flush=True)


if __name__ == "__main__":
    main()
