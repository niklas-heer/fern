#!/usr/bin/env python3
"""Publish a reproducible benchmark and case-study report for Fern."""

from __future__ import annotations

import argparse
import platform
import statistics
import subprocess
import time
from dataclasses import dataclass
from datetime import UTC, datetime
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
FERN_BIN = ROOT / "bin" / "fern"
DEFAULT_EXAMPLES = (
    ROOT / "examples" / "tiny_cli.fn",
    ROOT / "examples" / "http_api.fn",
    ROOT / "examples" / "actor_app.fn",
)


@dataclass(frozen=True)
class LatencyStats:
    """Simple latency summary in milliseconds."""

    median_ms: float
    p95_ms: float
    max_ms: float


@dataclass(frozen=True)
class CompilerBaseline:
    """Compiler-level benchmark snapshot."""

    release_build_seconds: float | None
    binary_size_bytes: int
    startup: LatencyStats


@dataclass(frozen=True)
class CaseStudyResult:
    """Per-example benchmark result."""

    name: str
    source: Path
    build_seconds: float
    binary_size_bytes: int
    run: LatencyStats


def run(cmd: list[str], *, cwd: Path = ROOT, check: bool = True) -> subprocess.CompletedProcess[str]:
    """Run a subprocess command."""

    proc = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, check=False)
    if check and proc.returncode != 0:
        details = [
            f"command failed: {' '.join(cmd)}",
            f"exit={proc.returncode}",
            "--- stdout ---",
            proc.stdout,
            "--- stderr ---",
            proc.stderr,
        ]
        raise RuntimeError("\n".join(details))
    return proc


def fail(message: str) -> int:
    """Print a terminal-friendly failure line."""

    print(f"ERROR: {message}")
    return 1


def ensure_release_build() -> float:
    """Build a clean release and return build duration in seconds."""

    start = time.perf_counter()
    run(["make", "clean"])
    run(["make", "release"])
    return time.perf_counter() - start


def measure_latency(cmd: list[str], *, runs: int, cwd: Path = ROOT) -> LatencyStats:
    """Measure command latency and return median/p95/max."""

    if runs < 3:
        raise ValueError("runs must be >= 3 for stable p95 measurement")

    run(cmd, cwd=cwd)
    samples: list[float] = []
    for _ in range(runs):
        start = time.perf_counter()
        run(cmd, cwd=cwd)
        samples.append((time.perf_counter() - start) * 1000.0)

    ordered = sorted(samples)
    p95_index = max(0, int(round(0.95 * (len(ordered) - 1))))
    return LatencyStats(
        median_ms=statistics.median(samples),
        p95_ms=ordered[p95_index],
        max_ms=max(samples),
    )


def collect_baseline(startup_runs: int, skip_release_build: bool) -> CompilerBaseline:
    """Collect compiler binary size + startup latency metrics."""

    release_build_seconds = None
    if not skip_release_build:
        release_build_seconds = ensure_release_build()

    if not FERN_BIN.exists():
        raise FileNotFoundError(f"fern compiler not found at {FERN_BIN}")

    startup = measure_latency([str(FERN_BIN), "--version"], runs=startup_runs, cwd=ROOT)
    return CompilerBaseline(
        release_build_seconds=release_build_seconds,
        binary_size_bytes=FERN_BIN.stat().st_size,
        startup=startup,
    )


def run_case_study(source: Path, case_runs: int) -> CaseStudyResult:
    """Build and benchmark one canonical example."""

    if not source.exists():
        raise FileNotFoundError(f"example not found: {source}")

    output_bin = ROOT / "dist" / "bench" / source.stem
    output_bin.parent.mkdir(parents=True, exist_ok=True)

    start = time.perf_counter()
    run([str(FERN_BIN), "build", "-o", str(output_bin), str(source)], cwd=ROOT)
    build_seconds = time.perf_counter() - start

    run_stats = measure_latency([str(output_bin)], runs=case_runs, cwd=ROOT)

    return CaseStudyResult(
        name=source.stem,
        source=source,
        build_seconds=build_seconds,
        binary_size_bytes=output_bin.stat().st_size,
        run=run_stats,
    )


def collect_environment() -> dict[str, str]:
    """Collect concise environment metadata for report reproducibility."""

    clang = run(["clang", "--version"], check=False)
    clang_version = clang.stdout.splitlines()[0] if clang.returncode == 0 and clang.stdout else "unavailable"

    git_sha = run(["git", "rev-parse", "HEAD"], check=False)
    git_head = git_sha.stdout.strip() if git_sha.returncode == 0 and git_sha.stdout.strip() else "unknown"

    return {
        "timestamp_utc": datetime.now(UTC).strftime("%Y-%m-%d %H:%M:%S UTC"),
        "platform": platform.platform(),
        "machine": platform.machine(),
        "python": platform.python_version(),
        "clang": clang_version,
        "git_head": git_head,
    }


def render_report(
    env: dict[str, str],
    baseline: CompilerBaseline,
    cases: list[CaseStudyResult],
    startup_runs: int,
    case_runs: int,
    skip_release_build: bool,
) -> str:
    """Render markdown benchmark report."""

    lines: list[str] = []
    lines.append("# Fern Benchmark Report")
    lines.append("")
    lines.append("## Environment")
    lines.append(f"- timestamp: `{env['timestamp_utc']}`")
    lines.append(f"- platform: `{env['platform']}`")
    lines.append(f"- machine: `{env['machine']}`")
    lines.append(f"- python: `{env['python']}`")
    lines.append(f"- clang: `{env['clang']}`")
    lines.append(f"- git_head: `{env['git_head']}`")
    lines.append("")
    lines.append("## Compiler Baseline")
    if baseline.release_build_seconds is None:
        lines.append("- release build: skipped (`--skip-release-build`)")
    else:
        lines.append(f"- release build (clean): `{baseline.release_build_seconds:.2f}s`")
    lines.append(f"- `bin/fern` size: `{baseline.binary_size_bytes}` bytes")
    lines.append(
        f"- `fern --version` startup ({startup_runs} runs): "
        f"median `{baseline.startup.median_ms:.2f}ms`, "
        f"p95 `{baseline.startup.p95_ms:.2f}ms`, "
        f"max `{baseline.startup.max_ms:.2f}ms`"
    )
    lines.append("")
    lines.append("## Case Studies")
    for case in cases:
        rel_source = case.source.relative_to(ROOT)
        lines.append(f"### {case.name}")
        lines.append(f"- source: `{rel_source}`")
        lines.append(f"- build time: `{case.build_seconds:.2f}s`")
        lines.append(f"- executable size: `{case.binary_size_bytes}` bytes")
        lines.append(
            f"- run latency ({case_runs} runs): "
            f"median `{case.run.median_ms:.2f}ms`, "
            f"p95 `{case.run.p95_ms:.2f}ms`, "
            f"max `{case.run.max_ms:.2f}ms`"
        )
        lines.append("")

    lines.append("## Reproduce")
    lines.append("```bash")
    lines.append("make release")
    lines.append(
        "python3 scripts/publish_benchmarks.py "
        f"--startup-runs {startup_runs} --case-runs {case_runs}"
    )
    if skip_release_build:
        lines.append("# For faster local iteration:")
        lines.append(
            "python3 scripts/publish_benchmarks.py "
            f"--skip-release-build --startup-runs {startup_runs} --case-runs {case_runs}"
        )
    lines.append("```")
    lines.append("")
    lines.append("## Related Report")
    lines.append("- `docs/reports/memory-path-comparison-2026-02-06.md`")
    lines.append("")
    return "\n".join(lines)


def parse_args() -> argparse.Namespace:
    """Parse CLI arguments."""

    today = datetime.now(UTC).strftime("%Y-%m-%d")
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output",
        default=f"docs/reports/benchmark-case-studies-{today}.md",
        help="Output markdown path",
    )
    parser.add_argument(
        "--startup-runs",
        type=int,
        default=20,
        help="Number of startup probes for `fern --version`",
    )
    parser.add_argument(
        "--case-runs",
        type=int,
        default=10,
        help="Number of run probes per case-study executable",
    )
    parser.add_argument(
        "--skip-release-build",
        action="store_true",
        help="Skip `make clean && make release` before measuring",
    )
    parser.add_argument(
        "--example",
        dest="examples",
        action="append",
        default=[],
        help="Example file path (repeatable). Defaults to the three canonical examples.",
    )
    return parser.parse_args()


def main() -> int:
    """Program entry point."""

    args = parse_args()
    if args.startup_runs < 3:
        return fail("startup-runs must be >= 3")
    if args.case_runs < 3:
        return fail("case-runs must be >= 3")

    examples = [Path(item) for item in args.examples] if args.examples else list(DEFAULT_EXAMPLES)

    try:
        env = collect_environment()
        baseline = collect_baseline(args.startup_runs, args.skip_release_build)
        case_results = [run_case_study(item, args.case_runs) for item in examples]
        report = render_report(
            env=env,
            baseline=baseline,
            cases=case_results,
            startup_runs=args.startup_runs,
            case_runs=args.case_runs,
            skip_release_build=args.skip_release_build,
        )
        out_path = ROOT / args.output
        out_path.parent.mkdir(parents=True, exist_ok=True)
        out_path.write_text(report, encoding="utf-8")
    except (FileNotFoundError, RuntimeError, OSError, ValueError) as exc:
        return fail(str(exc))

    print(f"Wrote benchmark report: {out_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
