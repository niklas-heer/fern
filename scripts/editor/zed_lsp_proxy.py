"""Test-only transparent LSP relay with bounded transcripts and explicit owned process cleanup."""
import json
import os
from pathlib import Path
import selectors
import subprocess
import sys


def main():
    root = Path(__file__).resolve().parent
    settings = json.loads((root / "proxy.json").read_text())
    (root / "invocation.json").write_text(json.dumps(sys.argv))
    process = subprocess.Popen([settings["rust"], *sys.argv[1:]], stdin=subprocess.PIPE,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    selector = selectors.DefaultSelector()
    logs = [(root / name).open("wb") for name in ["requests.bin", "responses.bin", "lsp.stderr"]]
    totals = [0, 0, 0]
    streams = [(sys.stdin.fileno(), process.stdin.fileno(), 0),
               (process.stdout.fileno(), sys.stdout.fileno(), 1), (process.stderr.fileno(), None, 2)]
    for source, destination, index in streams:
        selector.register(source, selectors.EVENT_READ, (destination, index))
    try:
        while process.poll() is None:
            for key, _ in selector.select(0.1):
                data = os.read(key.fd, 65536)
                destination, index = key.data
                if not data:
                    selector.unregister(key.fd)
                    if index == 0:
                        process.stdin.close()
                    continue
                totals[index] += len(data)
                if totals[index] > 1048576:
                    raise ValueError("LSP smoke transcript exceeds 1 MiB")
                logs[index].write(data)
                logs[index].flush()
                if destination is not None:
                    offset = 0
                    while offset < len(data):
                        offset += os.write(destination, data[offset:])
    finally:
        selector.close()
        for log in logs:
            log.close()
        if process.poll() is None:
            process.terminate()
        process.wait(timeout=5)
    return process.returncode


if __name__ == "__main__":
    sys.exit(main())
