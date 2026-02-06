#!/usr/bin/env python3
"""Create and verify Fern single-binary release bundles."""

from __future__ import annotations

import argparse
import hashlib
import platform
import shutil
import tarfile
from dataclasses import dataclass
from pathlib import Path


REQUIRED_STAGING_FILES = ("fern", "libfern_runtime.a", "LICENSE")
OPTIONAL_STAGING_FILES = ("README.md", "docs/COMPATIBILITY_POLICY.md")


@dataclass(frozen=True)
class BundleSpec:
    """Bundle naming metadata."""

    version: str
    os_name: str
    arch: str

    @property
    def stem(self) -> str:
        """Return stable bundle stem."""
        return f"fern-{self.version}-{self.os_name}-{self.arch}"


def fail(message: str) -> int:
    """Print an error and return failure."""
    print(f"ERROR: {message}")
    return 1


def detect_os() -> str:
    """Map host OS into normalized artifact token."""
    system = platform.system().lower()
    if system == "darwin":
        return "macos"
    if system == "linux":
        return "linux"
    return system


def detect_arch() -> str:
    """Map host arch into normalized artifact token."""
    machine = platform.machine().lower()
    aliases = {
        "x86_64": "x86_64",
        "amd64": "x86_64",
        "aarch64": "arm64",
        "arm64": "arm64",
    }
    return aliases.get(machine, machine)


def sha256_file(path: Path) -> str:
    """Compute sha256 for a file."""
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(65536), b""):
            digest.update(chunk)
    return digest.hexdigest()


def ensure_staging_layout(staging: Path) -> None:
    """Ensure required release inputs are present."""
    if not staging.exists() or not staging.is_dir():
        raise FileNotFoundError(f"staging directory not found: {staging}")

    missing: list[str] = []
    for rel in REQUIRED_STAGING_FILES:
        path = staging / rel
        if not path.exists():
            missing.append(rel)

    fern_path = staging / "fern"
    if fern_path.exists() and not fern_path.is_file():
        missing.append("fern (must be a file)")
    if fern_path.exists() and not fern_path.stat().st_mode & 0o111:
        missing.append("fern (must be executable)")

    if missing:
        joined = ", ".join(missing)
        raise FileNotFoundError(f"staging layout invalid; missing/invalid: {joined}")


def make_bundle(staging: Path, out_dir: Path, spec: BundleSpec) -> tuple[Path, Path]:
    """Create tar.gz bundle plus sha256 file."""
    ensure_staging_layout(staging)
    out_dir.mkdir(parents=True, exist_ok=True)

    bundle_root = spec.stem
    archive_path = out_dir / f"{bundle_root}.tar.gz"
    checksum_path = out_dir / f"{bundle_root}.tar.gz.sha256"

    with tarfile.open(archive_path, "w:gz") as tar:
        for rel in REQUIRED_STAGING_FILES:
            src = staging / rel
            tar.add(src, arcname=f"{bundle_root}/{rel}")

        for rel in OPTIONAL_STAGING_FILES:
            src = staging / rel
            if src.exists() and src.is_file():
                tar.add(src, arcname=f"{bundle_root}/{rel}")

    digest = sha256_file(archive_path)
    checksum_path.write_text(f"{digest}  {archive_path.name}\n", encoding="utf-8")
    return archive_path, checksum_path


def verify_archive(archive_path: Path, checksum_path: Path) -> None:
    """Verify archive checksum and required bundle contents."""
    if not archive_path.exists():
        raise FileNotFoundError(f"archive not found: {archive_path}")
    if not checksum_path.exists():
        raise FileNotFoundError(f"checksum not found: {checksum_path}")

    checksum_text = checksum_path.read_text(encoding="utf-8").strip()
    expected_hash = checksum_text.split()[0] if checksum_text else ""
    actual_hash = sha256_file(archive_path)
    if expected_hash != actual_hash:
        raise ValueError("archive checksum mismatch")

    with tarfile.open(archive_path, "r:gz") as tar:
        names = tar.getnames()
    if not names:
        raise ValueError("archive is empty")

    root = names[0].split("/")[0]
    required_members = [f"{root}/{rel}" for rel in REQUIRED_STAGING_FILES]
    missing = [member for member in required_members if member not in names]
    if missing:
        raise ValueError(f"archive missing required members: {', '.join(missing)}")


def parse_args() -> argparse.Namespace:
    """Parse CLI arguments."""
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)

    verify_layout = sub.add_parser("verify-layout", help="Verify staging layout")
    verify_layout.add_argument("--staging", default="bin")

    package = sub.add_parser("package", help="Create release bundle")
    package.add_argument("--staging", default="bin")
    package.add_argument("--out-dir", default="dist")
    package.add_argument("--version", required=True)
    package.add_argument("--os", dest="os_name", default=detect_os())
    package.add_argument("--arch", default=detect_arch())

    verify_archive_cmd = sub.add_parser("verify-archive", help="Verify an existing release bundle")
    verify_archive_cmd.add_argument("--archive", required=True)
    verify_archive_cmd.add_argument("--checksum", required=True)
    return parser.parse_args()


def main() -> int:
    """Program entry point."""
    args = parse_args()

    if args.command == "verify-layout":
        try:
            ensure_staging_layout(Path(args.staging))
        except (FileNotFoundError, ValueError) as exc:
            return fail(str(exc))
        print(f"Staging layout valid: {args.staging}")
        return 0

    if args.command == "package":
        spec = BundleSpec(version=args.version, os_name=args.os_name, arch=args.arch)
        staging = Path(args.staging)
        out_dir = Path(args.out_dir)
        try:
            archive_path, checksum_path = make_bundle(staging, out_dir, spec)
            verify_archive(archive_path, checksum_path)
        except (FileNotFoundError, ValueError, OSError, tarfile.TarError) as exc:
            return fail(str(exc))
        print(f"Created release bundle: {archive_path}")
        print(f"Created checksum: {checksum_path}")
        return 0

    if args.command == "verify-archive":
        try:
            verify_archive(Path(args.archive), Path(args.checksum))
        except (FileNotFoundError, ValueError, OSError, tarfile.TarError) as exc:
            return fail(str(exc))
        print(f"Release bundle verified: {args.archive}")
        return 0

    return fail(f"unknown command: {args.command}")


if __name__ == "__main__":
    raise SystemExit(main())
