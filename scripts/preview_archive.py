"""Strict, bounded deterministic preview tar/gzip dialect; no archive extraction."""
import binascii
import struct
import zlib

BLOCK = 512
CHUNK = 65536
GZIP_HEADER = bytes.fromhex("1f8b08000000000002ff")


class BoundedReader:
    """Cap actual compressed bytes even if an opened input grows after fstat."""
    def __init__(self, source, limit):
        """Retain one descriptor stream and its already validated byte allowance."""
        self.source, self.remaining = source, limit

    def read(self, size):
        """Read a bounded chunk and reject the first byte beyond the allowance."""
        if size < 0:
            raise ValueError("unbounded archive read is forbidden")
        data = self.source.read(min(size, self.remaining + 1))
        if len(data) > self.remaining:
            raise ValueError("compressed archive exceeds byte limit")
        self.remaining -= len(data)
        return data


def header(name, size, mode):
    """Encode a canonical regular USTAR header from validated ASCII metadata."""
    encoded = name.encode("ascii")
    if len(encoded) > 100 or size < 0 or size >= 8 ** 11:
        raise ValueError("archive header bounds exceeded")
    raw = bytearray(BLOCK)
    raw[:len(encoded)] = encoded
    for start, width, value in [(100, 8, mode), (108, 8, 0), (116, 8, 0),
                                (124, 12, size), (136, 12, 0)]:
        raw[start:start + width] = (f"{value:0{width - 1}o}" + "\0").encode("ascii")
    raw[148:156] = b"        "
    raw[156] = ord("0")
    raw[257:265] = b"ustar\0" + b"00"
    raw[148:156] = (f"{sum(raw):06o}\0 ").encode("ascii")
    return bytes(raw)


def read_header(stream):
    """Reject noncanonical metadata before using a member's advertised size."""
    raw = stream.read(BLOCK)
    if len(raw) != BLOCK:
        raise ValueError("truncated archive header")
    try:
        name = raw[:100].split(b"\0", 1)[0].decode("ascii")
        size = int(raw[124:135], 8)
        mode = int(raw[100:107], 8)
    except (ValueError, UnicodeError) as error:
        raise ValueError("invalid archive header") from error
    if raw != header(name, size, mode):
        raise ValueError("noncanonical archive header")
    return name, size, mode


def pad(size):
    """Return the exact zero padding length for a bounded member."""
    return (-size) % BLOCK


def write_gzip(source, output):
    """Compress a prepared bounded tar stream with a stable header and trailer."""
    codec = zlib.compressobj(9, zlib.DEFLATED, -15)
    checksum = total = 0
    output.write(GZIP_HEADER)
    while chunk := source.read(CHUNK):
        total += len(chunk)
        checksum = binascii.crc32(chunk, checksum)
        output.write(codec.compress(chunk))
    output.write(codec.flush())
    output.write(struct.pack("<II", checksum, total & 0xffffffff))


def inflate(source, output, limit):
    """Validate one fixed gzip member, bounding decompressed work before writes."""
    if source.read(10) != GZIP_HEADER:
        raise ValueError("unsupported gzip header")
    codec = zlib.decompressobj(-15)
    checksum = total = 0
    while not codec.eof:
        data = source.read(CHUNK)
        if not data:
            raise ValueError("truncated gzip stream")
        try:
            chunk = codec.decompress(data, min(CHUNK, limit - total + 1))
            while True:
                total += len(chunk)
                if total > limit:
                    raise ValueError("decompressed archive exceeds byte limit")
                checksum = binascii.crc32(chunk, checksum)
                output.write(chunk)
                if not codec.unconsumed_tail or codec.eof:
                    break
                chunk = codec.decompress(codec.unconsumed_tail, min(CHUNK, limit - total + 1))
        except zlib.error as error:
            raise ValueError("invalid gzip stream") from error
    trailer = codec.unused_data + source.read(9)
    if len(trailer) != 8 or trailer != struct.pack("<II", checksum, total & 0xffffffff):
        raise ValueError("invalid gzip trailer or trailing data")
    output.seek(0)
