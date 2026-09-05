# Rust native JSON API

The Rust frontend now checks and compiles immutable JSON values through the
validating native runtime. The C frontend retains its legacy string-copy source
API. Interactive REPL evaluation reports that native JSON support is unavailable;
stored functions may refer to the API, but invoking them does not fabricate JSON
values or commit failed bindings. REPL parity is the next checkpoint.

Use `json` or the compatibility spelling `Json`. Qualified annotations
`json.Value`/`json.Error` and `Json.Value`/`Json.Error` identify the same opaque
types. Ordinary user types named `Value`, `Error` or `Json` remain available.
Opaque values work in generic functions, records, collections and closures; they
cannot be constructed as records, inspected through fields, compared, used as
Map keys, printed or interpolated implicitly. Print encoded text or error details.

```fern
json.parse(String) -> Result(json.Value, json.Error)
json.stringify(json.Value) -> Result(String, json.Error)
json.is_null(json.Value) -> Bool
json.get(json.Value, String) -> Result(json.Value, json.Error)
json.at(json.Value, Int) -> Result(json.Value, json.Error)
json.length(json.Value) -> Result(Int, json.Error)
json.as_bool(json.Value) -> Result(Bool, json.Error)
json.as_int(json.Value) -> Result(Int, json.Error)
json.as_float(json.Value) -> Result(Float, json.Error)
json.as_string(json.Value) -> Result(String, json.Error)
json.number_text(json.Value) -> Result(String, json.Error)
json.elements(json.Value) -> Result(List(json.Value), json.Error)
json.members(json.Value) -> Result(List((json.Value, json.Value)), json.Error)
json.error_code(json.Error) -> Int
json.error_offset(json.Error) -> Int
json.error_message(json.Error) -> String

json.null() -> json.Value
json.from_bool(Bool) -> json.Value
json.from_int(Int) -> json.Value
json.from_float(Float) -> Result(json.Value, json.Error)
json.from_string(String) -> Result(json.Value, json.Error)
json.from_number_text(String) -> Result(json.Value, json.Error)
json.from_array(List(json.Value)) -> Result(json.Value, json.Error)
json.from_object(Map(String, json.Value)) -> Result(json.Value, json.Error)
```

This is an explicit unreleased Rust-source migration. `parse` now returns a Value
and an opaque Error, replacing the former String/Int payloads. `stringify("[]")`
is a type error: parse `"[]"` for an array, or use `from_string("[]")` for a JSON
string that encodes as `"\"[]\""`. Both legacy C ABI symbols remain unchanged.
The canonical source API reference labels the C signatures separately.

Parsing preserves exact number spelling, validates Unicode, rejects duplicate
decoded keys, and preserves object insertion order. `as_int` never rounds through
Float. `as_float` performs explicit binary64 rounding, rejecting overflow and
nonzero values rounded to zero. `from_float` rejects NaN/infinity and uses a
private thread-local C-locale `%.17g` conversion, preserving negative zero.
`from_number_text` accepts exactly one JSON number token without whitespace/BOM.

`length` counts arrays or object members. `elements` and `members` copy collection
storage once and retain immutable children. Member keys are JSON String values,
so keys containing escaped NUL remain lossless; `as_string` reports a precise error
for those keys while `stringify` renders the escape. Array indexing, missing keys
and type mismatches return distinct ordinary Result errors. `?`, matching and
function defers work normally. Builder failures use offset `-1`; parse failures
use original byte offsets. See [the stable error table](JSON_NATIVE_CORE.md).

Builders enforce depth 128, expanded-node 100,000, logical allocation 32 MiB and
encoded content 16 MiB bounds before publishing a value. Scalar input is at most
1 MiB; aggregate object key bytes are at most 16 MiB and are also constrained by
encoded/allocation/work limits. Shared children count on every encoded appearance,
so repeatedly doubling an array cannot create an accepted exponential encoding.
A source Map has already applied last-wins/first-position replacement; object
construction preserves its resulting order. Native parallel-list callers get a
duplicate-key error instead of a second replacement policy.

Encoding can exceed the stricter 1 MiB parse-input limit, for example when large
strings contain many escaped controls. The maximum 16 MiB output is therefore not
a promise that every encoded value can be parsed again under the input cap.

The native boundary uses full-width opaque pointers and heap Results. Float builder
arguments use QBE `d`/C `double`. Member records use two native pointer fields;
the compiler checks Result success before converting them into tagged Fern tuples.
The Map argument is evaluated once, then copied in one bounded pass to parallel
native lists. No C object is reinterpreted as an unrelated Fern tuple or Map.
Adapter storage is included in the documented per-operation allocation reservation:
objects reserve `16 * max(count,1) + 2*sizeof(FernList)` bytes; members reserve at
most `56 * max(count,1) + 2*sizeof(FernList)` bytes, including tuple conversion.

The native gate runs ten exact-output programs and twelve semantic rejection
cases, including direct/first-class calls, NUL members, signed 64-bit Int/Float values,
opaque values retained through GC, deferred cleanup, and shared-tree limits.
Eight Rust integration tests additionally cover qualified aliases, formatting,
public-IR signature rejection and atomic explicit REPL refusal. Typed
`json.encode`, `json.decode(User)` and `derive(Json)` remain future codec work.
