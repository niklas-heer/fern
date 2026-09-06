# Native build task arguments

The build helpers read authored flag records from `scripts/build_config` through
`build-arguments`. Runtime pkg-config include flags use the same decoder. The
native-only style bootstrap retains its existing C metadata decoder and needs
neither this helper, Python, mise, nor a recursively built compiler to decode flags.

A record is a literal argument list: spaces/tabs separate words, single and double
quotes group them, and a backslash escapes the next byte outside single quotes.
Adjacent quoted and unquoted spans join one word; `""` is one empty argument.
Dollar signs, backticks, semicolons, wildcards and other metacharacters remain
literal bytes. There is no variable, command, pathname or arithmetic expansion.
This matches the bootstrap's lexical flag contract, not every shell quoting rule.

One optional final newline terminates the record. Embedded NUL/CR/newline,
unclosed quotes and trailing escapes reject the whole record before compiler
execution. Decoding reads at most 65,537 bytes, accepts at most 65,536 bytes per
record (including its final newline), 16,384 bytes per word and 4,096 words.
Plain spans copy in 256-byte chunks to avoid slow long-word assembly in Bash3.2.
A failed decoder never publishes partial or stale arguments.

Configuration and pkg-config are trusted build producers, not sandboxed programs.
Their output is first captured in an invocation-owned temporary file so command
status and NUL bytes are retained; success and failure remove that file. The
reader bounds allocation and decoding work, not the external producer's runtime
or disk writes. The native bootstrap's separate process/budget controls remain
unchanged.

Source inventories retain their existing deliberate glob expansion, including
matched filenames containing spaces. Flag records never use that expansion.
The `${array[@]+"${array[@]}"}` spelling preserves empty arrays under macOS
Bash3.2 with `set -u`; it is intentional, not shell evaluation.

Run `python3 scripts/test_build_flags.py` for quoted/escaped/empty/malformed,
size-limit, Unicode, injection-literal and seeded roundtrip tests. All five native
helpers are exercised with mock compiler/archive tools for debug/release mode;
these tests neither compile C nor touch checkout build artifacts.
