#!/usr/bin/env python3
"""Stage and verify opt-in Rust previews from explicit immutable native inputs."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import tempfile

import preview_archive as archive

MANIFEST = "fern-rust-preview.json"
EXECUTABLES = ("fern-rs", "fern-qbe", "fern-test-supervisor")
PAYLOAD_NAMES = tuple(sorted((*EXECUTABLES, "libfern_runtime.a", "LICENSE", "PREVIEW.md")))
FILE_LIMITS = {name: 128 * 1024 * 1024 for name in PAYLOAD_NAMES}
FILE_LIMITS.update({"LICENSE": 65536, "PREVIEW.md": 65536, MANIFEST: 65536})
TOTAL_LIMIT = 256 * 1024 * 1024
TAR_LIMIT = TOTAL_LIMIT + 16384
ARCHIVE_LIMIT = TAR_LIMIT + 1024 * 1024
PREVIEW = """# Fern Rust preview

This opt-in preview does not replace the shipping C compiler. Keep these files
beside fern-rs when moving this directory. No Cargo, Python or separate QBE
installation is required to use it. Native build/run/test require a host C
compiler, pkg-config and compatible BDW GC, SQLite and OpenSSL development
libraries. This bundle is for its declared OS/architecture, not a static or
cross-platform executable. See the source project's Rust migration status.

Examples: ./fern-rs check app.fn; ./fern-rs emit app.fn;
./fern-rs build app.fn -o app; ./fern-rs run app.fn;
./fern-rs test tests.fn; ./fern-rs doc app.fn --html -o docs.html.
No browser opens unless explicitly requested with doc --open.

fern-rust-preview.json disables implicit development-checkout fallback. Keep it
with the bundle. Explicit FERN_QBE, FERN_RUNTIME_LIB and FERN_TEST_SUPERVISOR
component paths still override bundled files. Missing helpers are errors.

The manifest authenticates no publisher: hashes detect accidental modification,
not a malicious replacement of both payload and manifest. Archive reproducibility
covers identical input bytes and metadata with the same packaging/zlib toolchain;
it does not claim reproducible compilation, signing or release publication.
"""


def mode(name):
    """Choose the one accepted Unix payload mode independently of source umask."""
    return 0o755 if name in EXECUTABLES else 0o644


def metadata(version, system, arch):
    """Validate bounded portable archive identity before constructing any path."""
    if not isinstance(version, str) or len(version) > 40 or not re.fullmatch(
        r"[0-9]+\.[0-9]+\.[0-9]+(?:-[A-Za-z0-9]+(?:[.-][A-Za-z0-9]+)*)?", version
    ):
        raise ValueError("invalid bounded preview version")
    if system not in ("macos", "linux") or arch not in ("arm64", "x86_64"):
        raise ValueError("unsupported preview OS or architecture")
    return f"fern-rust-preview-{version}-{system}-{arch}"


def canonical(value):
    """Encode deterministic small JSON metadata; consumers reject alternate forms."""
    return (json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=True) + "\n").encode()


def regular(path, limit, executable=False):
    """Open one bounded regular non-symlink input and compare the selected inode."""
    info = path.lstat()
    if not stat.S_ISREG(info.st_mode) or info.st_size < 1 or info.st_size > limit:
        raise ValueError(f"invalid regular file or byte limit: {path}")
    if executable and not info.st_mode & 0o111:
        raise ValueError(f"component is not executable: {path}")
    descriptor = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    stream = os.fdopen(descriptor, "rb")
    opened = os.fstat(stream.fileno())
    if (opened.st_dev, opened.st_ino, opened.st_size, opened.st_mtime_ns) != (
        info.st_dev, info.st_ino, info.st_size, info.st_mtime_ns
    ):
        stream.close()
        raise ValueError("input changed while opening")
    return stream, info


def fingerprint(info):
    """Compare immutable file identity/content metadata without read-side atime changes."""
    return (info.st_dev, info.st_ino, info.st_size, info.st_mtime_ns, info.st_ctime_ns, info.st_mode)


def transfer(path, name, output=None, expected_size=None):
    """Hash/copy one file under its precharged cap and reject concurrent changes."""
    limit = FILE_LIMITS[name] if expected_size is None else min(FILE_LIMITS[name], expected_size)
    stream, before = regular(path, limit, name in EXECUTABLES)
    digest = hashlib.sha256()
    size = 0
    with stream:
        for _ in range(FILE_LIMITS[name] // archive.CHUNK + 2):
            data = stream.read(archive.CHUNK)
            if not data:
                break
            size += len(data)
            if size > before.st_size:
                raise ValueError("input grew during copy")
            digest.update(data)
            if output is not None:
                output.write(data)
        after = os.fstat(stream.fileno())
    if size != before.st_size or fingerprint(after) != fingerprint(before) or fingerprint(path.lstat()) != fingerprint(before):
        raise ValueError("input changed during copy")
    return {"size": size, "mode": mode(name), "sha256": digest.hexdigest()}


def fresh(path):
    """Reject every existing destination, including dangling symbolic links."""
    if os.path.lexists(path):
        raise ValueError(f"stage destination already exists: {path}")
    if not path.parent.is_dir():
        raise ValueError("output parent must already exist")


def disjoint(output, inputs):
    """Reject canonical overlap or hard-linked aliases before output preparation."""
    target = output.resolve()
    for source in inputs:
        value = source.resolve()
        if target == value or target in value.parents or value in target.parents:
            raise ValueError("output overlaps an input")
        if output.exists() and source.exists() and os.path.samefile(output, source):
            raise ValueError("output aliases an input")


def stage(inputs, output, version, system, arch):
    """Publish a complete seven-file directory only at a previously absent path."""
    metadata(version, system, arch)
    if set(inputs) != set(PAYLOAD_NAMES):
        raise ValueError("stage requires exactly six explicit input files")
    fresh(output)
    disjoint(output, inputs.values())
    sizes = {name: path.lstat().st_size for name, path in inputs.items()}
    total = sum(sizes.values())
    if total > TOTAL_LIMIT:
        raise ValueError("aggregate payload byte limit exceeded")
    with tempfile.TemporaryDirectory(prefix=".fern-preview-", dir=output.parent) as temporary:
        prepared = Path(temporary) / "payload"
        prepared.mkdir(mode=0o755)
        files = {}
        for name in PAYLOAD_NAMES:
            with (prepared / name).open("xb") as stream:
                files[name] = transfer(inputs[name], name, stream, expected_size=sizes[name])
            (prepared / name).chmod(mode(name))
        manifest = {"format": 1, "version": version, "os": system, "arch": arch, "files": files}
        (prepared / MANIFEST).write_bytes(canonical(manifest))
        (prepared / MANIFEST).chmod(0o644)
        validate_stage(prepared)
        fresh(output)
        prepared.rename(output)
    return manifest


def unique_pairs(pairs):
    """Reject duplicate JSON object keys instead of silently taking the last one."""
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("duplicate manifest key")
        result[key] = value
    return result


def parse_manifest(data):
    """Validate the complete closed manifest shape within a 64 KiB input cap."""
    if len(data) > FILE_LIMITS[MANIFEST]:
        raise ValueError("manifest byte limit exceeded")
    try:
        value = json.loads(data, object_pairs_hook=unique_pairs)
    except (UnicodeError, RecursionError, json.JSONDecodeError) as error:
        raise ValueError("invalid manifest JSON") from error
    if not isinstance(value, dict) or set(value) != {"format", "version", "os", "arch", "files"}:
        raise ValueError("invalid manifest fields")
    if type(value["format"]) is not int or value["format"] != 1:
        raise ValueError("unsupported manifest format")
    metadata(value["version"], value["os"], value["arch"])
    files = value["files"]
    if not isinstance(files, dict) or set(files) != set(PAYLOAD_NAMES):
        raise ValueError("invalid payload membership")
    total = 0
    for name, entry in files.items():
        if not isinstance(entry, dict) or set(entry) != {"size", "mode", "sha256"}:
            raise ValueError("invalid payload record")
        if type(entry["size"]) is not int or not 0 < entry["size"] <= FILE_LIMITS[name]:
            raise ValueError("payload byte limit exceeded")
        if type(entry["mode"]) is not int or entry["mode"] != mode(name):
            raise ValueError("invalid payload mode")
        if not isinstance(entry["sha256"], str) or not re.fullmatch(r"[0-9a-f]{64}", entry["sha256"]):
            raise ValueError("invalid payload digest")
        total += entry["size"]
    if total > TOTAL_LIMIT or canonical(value) != data:
        raise ValueError("noncanonical or oversized manifest")
    return value


def validate_stage(directory):
    """Require exact immediate membership, regular files, modes and payload hashes."""
    if directory.is_symlink() or not directory.is_dir():
        raise ValueError("staging must be a real directory")
    with os.scandir(directory) as entries:
        names = [entry.name for _, entry in zip(range(9), entries)]
    if set(names) != set((*PAYLOAD_NAMES, MANIFEST)) or len(names) != 7:
        raise ValueError("unexpected stage membership")
    stream, _ = regular(directory / MANIFEST, FILE_LIMITS[MANIFEST])
    with stream:
        manifest = parse_manifest(stream.read(FILE_LIMITS[MANIFEST] + 1))
    for name in (*PAYLOAD_NAMES, MANIFEST):
        if stat.S_IMODE((directory / name).lstat().st_mode) != mode(name):
            raise ValueError("noncanonical stage mode")
        if name != MANIFEST and transfer(directory / name, name) != manifest["files"][name]:
            raise ValueError(f"payload digest mismatch: {name}")
    return manifest


def member(stream, name, size, expected_mode, output=None):
    """Read one already bounded member and validate bytes and canonical padding."""
    if archive.read_header(stream) != (name, size, expected_mode):
        raise ValueError("unexpected archive member, size or mode")
    digest = hashlib.sha256()
    for offset in range(0, size, archive.CHUNK):
        data = stream.read(min(archive.CHUNK, size - offset))
        if len(data) != min(archive.CHUNK, size - offset):
            raise ValueError("truncated archive payload")
        digest.update(data)
        if output is not None:
            output.write(data)
    if stream.read(archive.pad(size)) != bytes(archive.pad(size)):
        raise ValueError("invalid member padding")
    return digest.hexdigest()


def verify_tar(stream, destination=None):
    """Validate exactly seven ordered regular members; optionally copy to private storage."""
    name, size, file_mode = archive.read_header(stream)
    if size < 1 or size > FILE_LIMITS[MANIFEST] or file_mode != 0o644:
        raise ValueError("invalid manifest header")
    data = stream.read(size)
    manifest = parse_manifest(data)
    root = metadata(manifest["version"], manifest["os"], manifest["arch"])
    if name != f"{root}/{MANIFEST}" or stream.read(archive.pad(size)) != bytes(archive.pad(size)):
        raise ValueError("invalid archive root or manifest padding")
    if destination is not None:
        (destination / MANIFEST).write_bytes(data)
        (destination / MANIFEST).chmod(0o644)
    for leaf in PAYLOAD_NAMES:
        entry = manifest["files"][leaf]
        if destination is None:
            digest = member(stream, f"{root}/{leaf}", entry["size"], mode(leaf))
        else:
            with (destination / leaf).open("xb") as output:
                digest = member(stream, f"{root}/{leaf}", entry["size"], mode(leaf), output)
            (destination / leaf).chmod(mode(leaf))
        if digest != entry["sha256"]:
            raise ValueError(f"archive payload digest mismatch: {leaf}")
    if stream.read(1025) != bytes(1024):
        raise ValueError("invalid archive terminator or extra members")
    return manifest


def verified_tar(path, callback):
    """Bound compressed/decompressed bytes and expose only one validated gzip stream."""
    source, info = regular(path, ARCHIVE_LIMIT)
    with source, tempfile.TemporaryFile() as tar:
        archive.inflate(archive.BoundedReader(source, info.st_size), tar, TAR_LIMIT)
        return callback(tar)


def verify(path):
    """Verify a hostile archive without extracting any pathname from its metadata."""
    return verified_tar(path, verify_tar)


def package(directory, output):
    """Atomically replace one archive only after its staged bytes verify completely."""
    manifest = validate_stage(directory)
    disjoint(output, [directory, *(directory / name for name in (*PAYLOAD_NAMES, MANIFEST))])
    if output.is_symlink() or (output.exists() and not output.is_file()):
        raise ValueError("archive output must be a regular file or absent")
    root = metadata(manifest["version"], manifest["os"], manifest["arch"])
    with tempfile.TemporaryDirectory(prefix=".fern-preview-", dir=output.parent) as temporary:
        target = Path(temporary) / "archive.tar.gz"
        with tempfile.TemporaryFile() as tar:
            for name in (MANIFEST, *PAYLOAD_NAMES):
                path = directory / name
                size = len(canonical(manifest)) if name == MANIFEST else manifest["files"][name]["size"]
                if path.lstat().st_size != size:
                    raise ValueError("payload changed after aggregate precharge")
                tar.write(archive.header(f"{root}/{name}", size, mode(name)))
                transfer(path, name, tar, expected_size=size)
                tar.write(bytes(archive.pad(size)))
            tar.write(bytes(1024)); tar.seek(0)
            with target.open("xb") as stream:
                archive.write_gzip(tar, stream)
        target.chmod(0o644)
        if verify(target) != manifest or validate_stage(directory) != manifest:
            raise ValueError("staging changed during packaging")
        os.replace(target, output)
    return manifest


def extract(path, output):
    """Verify before extraction, then publish fixed filenames to a new-only directory."""
    fresh(output)
    disjoint(output, [path])
    with tempfile.TemporaryDirectory(prefix=".fern-preview-", dir=output.parent) as temporary:
        prepared = Path(temporary) / "payload"
        prepared.mkdir(mode=0o755)
        def checked(tar):
            manifest = verify_tar(tar)
            tar.seek(0)
            if verify_tar(tar, prepared) != manifest:
                raise ValueError("archive changed during extraction")
            return manifest
        manifest = verified_tar(path, checked)
        validate_stage(prepared)
        fresh(output); prepared.rename(output)
    return manifest


def main():
    """Dispatch developer-only literal path commands; never build or install inputs."""
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    staging = commands.add_parser("stage")
    for option in ("compiler", "qbe", "supervisor", "runtime", "license", "output"):
        staging.add_argument("--" + option, required=True, type=Path)
    for option in ("version", "os", "arch"):
        staging.add_argument("--" + option, required=True)
    packing = commands.add_parser("package")
    packing.add_argument("--staging", required=True, type=Path)
    packing.add_argument("--output", required=True, type=Path)
    checking = commands.add_parser("verify")
    checking.add_argument("archive", type=Path)
    args = parser.parse_args()
    try:
        if args.command == "stage":
            with tempfile.TemporaryDirectory(prefix="fern-preview-readme-") as temporary:
                readme = Path(temporary) / "PREVIEW.md"; readme.write_text(PREVIEW, encoding="utf-8")
                inputs = dict(zip(EXECUTABLES, (args.compiler, args.qbe, args.supervisor)))
                inputs.update({"libfern_runtime.a": args.runtime, "LICENSE": args.license, "PREVIEW.md": readme})
                result = stage(inputs, args.output, args.version, args.os, args.arch)
        elif args.command == "package":
            result = package(args.staging, args.output)
        else:
            result = verify(args.archive)
    except (OSError, ValueError) as error:
        parser.exit(1, f"preview packaging: {error}\n")
    print(canonical(result).decode(), end="")


if __name__ == "__main__":
    main()
