# File text IO

`File.read(path) -> Result(String, Int)` publishes complete UTF-8 text without
interior NUL bytes, up to 16 MiB. Empty files succeed. Invalid encoding, NUL,
oversized files, incomplete reads and read/close errors return an error without
publishing partial text. Reads retain the existing seekable-file requirement.

`File.write(path, text)` and `File.append(path, text)` return `Result(Int, Int)`;
success contains the complete byte count. Text is validated before opening the
target. Native buffered writes must complete both the write and close/flush
successfully before returning Ok. Oversized or invalid text leaves an existing
target unchanged. An IO failure after opening may leave a created/truncated file
or an appended prefix: these are not atomic or durable writes.

| Code | Meaning |
| --- | --- |
| 1 | Opening a file for reading failed |
| 2 | Opening a file for writing/appending failed |
| 3 | IO, invalid text or size limit failure |
| 4 | Native allocation failed |

Native write arguments are C strings; they cannot represent bytes after an
interior NUL terminator. This is a text API, not a binary-file API. A future Bytes
API remains separate. Earlier failures survive cleanup, and each owned stream
is closed exactly once. There is no hard deadline or fsync guarantee.

The Rust REPL enforces the same text policy and retains its stricter interactive
String/storage budgets. Explicit reads and unbuffered writes are checked. Safe
Rust file-drop cleanup does not report a late OS close error; this is distinct from
the native buffered close/flush check and is not a durability guarantee.

This deliberately changes prior behavior: File.read no longer reports success
with bytes that silently truncate at NUL, and write/append no longer report
success after a buffered flush failure. Existing native malformed-String guard
tests inject bytes directly instead of relying on invalid File.read success.
