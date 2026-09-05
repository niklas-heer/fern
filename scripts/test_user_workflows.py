#!/usr/bin/env python3
"""Run canonical examples and every runnable tutorial block, checking actual output."""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
FERN = ROOT / "bin/fern"


def run_program(source: Path, expected: str, workdir: Path) -> None:
    result = subprocess.run([str(FERN), "run", str(source)], cwd=workdir,
                            text=True, capture_output=True, timeout=30)
    if result.returncode or result.stdout != expected:
        raise AssertionError(f"{source.name}: exit={result.returncode}\n"
                             f"expected={expected!r}\nactual={result.stdout!r}\n{result.stderr}")


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="fern-workflows-") as directory:
        workdir = Path(directory)
        for name, expected in (("tiny_cli.fn", "hello, fern\n"),
                               ("actor_app.fn", ""),
                               ("http_api.fn", "HTTP errors are explicit\n")):
            run_program(ROOT / "examples" / name, expected, workdir)
        guide = (ROOT / "docs/LANGUAGE_GUIDE.md").read_text()
        pairs = re.findall(r"```fern\n(.*?)```\s*```output\n(.*?)```", guide, re.DOTALL)
        if len(pairs) < 4:
            raise AssertionError("Language guide must contain at least four runnable examples")
        for index, (code, expected) in enumerate(pairs):
            source = workdir / f"tutorial-{index}.fn"
            source.write_text(code)
            run_program(source, expected, workdir)
        print(f"User workflows passed: 3 canonical programs and {len(pairs)} tutorial programs")


if __name__ == "__main__":
    main()
