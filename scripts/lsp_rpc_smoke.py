#!/usr/bin/env python3
"""Run a JSON-RPC smoke flow against `fern lsp`."""

from __future__ import annotations

import json
import subprocess
from dataclasses import dataclass
from pathlib import Path


FERN_BIN = Path("bin/fern")


@dataclass
class RpcEnvelope:
    """JSON-RPC envelope payload."""

    payload: dict

    def encode(self) -> bytes:
        """Encode envelope to LSP wire format."""
        body = json.dumps(self.payload, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
        header = f"Content-Length: {len(body)}\r\n\r\n".encode("ascii")
        return header + body


def parse_lsp_output(raw: bytes) -> list[dict]:
    """Parse concatenated LSP messages from bytes."""
    messages: list[dict] = []
    offset = 0

    while offset < len(raw):
        header_end = raw.find(b"\r\n\r\n", offset)
        if header_end < 0:
            break

        header_block = raw[offset:header_end].decode("ascii", errors="replace")
        offset = header_end + 4

        content_length = None
        for line in header_block.split("\r\n"):
            if ":" not in line:
                continue
            name, value = line.split(":", 1)
            if name.strip().lower() == "content-length":
                content_length = int(value.strip())
                break

        if content_length is None or content_length < 0:
            raise ValueError("invalid LSP output: missing Content-Length")

        body = raw[offset:offset + content_length]
        if len(body) != content_length:
            raise ValueError("invalid LSP output: truncated frame")
        offset += content_length
        messages.append(json.loads(body.decode("utf-8")))

    return messages


def find_response(messages: list[dict], req_id: int) -> dict:
    """Find response message by id."""
    for msg in messages:
        if msg.get("id") == req_id and ("result" in msg or "error" in msg):
            return msg
    raise AssertionError(f"missing response for id={req_id}")


def assert_true(condition: bool, message: str) -> None:
    """Raise AssertionError when condition is false."""
    if not condition:
        raise AssertionError(message)


def main() -> int:
    """Execute smoke flow and validate core responses."""
    if not FERN_BIN.exists():
        print(f"ERROR: expected compiler at {FERN_BIN}")
        return 1

    doc1_uri = "file:///tmp/lsp_rpc_doc1.fn"
    doc1_text = (
        "fn add(x: Int, y: Int) -> Int:\n"
        "    x + y\n\n"
        "fn main() -> Int:\n"
        "    ad\n"
    )
    doc2_uri = "file:///tmp/lsp_rpc_doc2.fn"
    doc2_text = (
        "fn main() -> Int:\n"
        "    fs.read(\"notes.txt\")\n"
        "    0\n"
    )

    envelopes = [
        RpcEnvelope(
            {
                "jsonrpc": "2.0",
                "id": 1,
                "method": "initialize",
                "params": {"processId": None, "rootUri": None, "capabilities": {}},
            }
        ),
        RpcEnvelope({"jsonrpc": "2.0", "method": "initialized", "params": {}}),
        RpcEnvelope(
            {
                "jsonrpc": "2.0",
                "method": "textDocument/didOpen",
                "params": {
                    "textDocument": {
                        "uri": doc1_uri,
                        "languageId": "fern",
                        "version": 1,
                        "text": doc1_text,
                    }
                },
            }
        ),
        RpcEnvelope(
            {
                "jsonrpc": "2.0",
                "id": 2,
                "method": "textDocument/completion",
                "params": {"textDocument": {"uri": doc1_uri}, "position": {"line": 4, "character": 6}},
            }
        ),
        RpcEnvelope(
            {
                "jsonrpc": "2.0",
                "id": 3,
                "method": "textDocument/rename",
                "params": {
                    "textDocument": {"uri": doc1_uri},
                    "position": {"line": 4, "character": 5},
                    "newName": "sum",
                },
            }
        ),
        RpcEnvelope(
            {
                "jsonrpc": "2.0",
                "method": "textDocument/didOpen",
                "params": {
                    "textDocument": {
                        "uri": doc2_uri,
                        "languageId": "fern",
                        "version": 1,
                        "text": doc2_text,
                    }
                },
            }
        ),
        RpcEnvelope(
            {
                "jsonrpc": "2.0",
                "id": 4,
                "method": "textDocument/codeAction",
                "params": {
                    "textDocument": {"uri": doc2_uri},
                    "range": {"start": {"line": 1, "character": 4}, "end": {"line": 1, "character": 8}},
                    "context": {"diagnostics": []},
                },
            }
        ),
        RpcEnvelope({"jsonrpc": "2.0", "id": 5, "method": "shutdown", "params": {}}),
        RpcEnvelope({"jsonrpc": "2.0", "method": "exit", "params": {}}),
    ]

    payload = b"".join(env.encode() for env in envelopes)

    proc = subprocess.Popen(
        [str(FERN_BIN), "lsp"],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    stdout, stderr = proc.communicate(payload, timeout=15)

    if proc.returncode != 0:
        print("ERROR: fern lsp exited non-zero")
        if stderr:
            print(stderr.decode("utf-8", errors="replace"))
        return 1

    messages = parse_lsp_output(stdout)
    init = find_response(messages, 1)
    caps = init.get("result", {}).get("capabilities", {})
    assert_true("completionProvider" in caps, "missing completionProvider capability")
    assert_true(caps.get("renameProvider") is True, "missing renameProvider capability")
    assert_true(caps.get("codeActionProvider") is True, "missing codeActionProvider capability")

    completion = find_response(messages, 2).get("result", [])
    labels = [item.get("label") for item in completion if isinstance(item, dict)]
    assert_true("add" in labels, "completion response missing function symbol 'add'")

    rename_result = find_response(messages, 3).get("result", {})
    rename_json = json.dumps(rename_result)
    assert_true("sum" in rename_json, "rename response missing replacement text 'sum'")

    code_actions = find_response(messages, 4).get("result", [])
    actions_json = json.dumps(code_actions)
    assert_true("Result" in actions_json, "code action response missing Result-focused quickfix")

    shutdown = find_response(messages, 5).get("result")
    assert_true(shutdown is None, "shutdown response should be null")

    print("LSP RPC smoke checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
