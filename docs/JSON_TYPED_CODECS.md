# Typed JSON codecs in the Rust compiler

`json.encode(value)` returns `Result(String, json.Error)`.
`json.decode(text, TargetType)` returns `Result(TargetType, json.Error)`.
Both operations require handling their Result. `Json.encode` and `Json.decode`
are aliases. The existing dynamic JSON API is unchanged.

Records opt in explicitly:

```fern
type User derive(Json):
    age: Int
    name: Option(String)

fn main() -> Result(Unit, json.Error):
    let user = json.decode("\{\"age\":42\}", User)?
    println(user.age)
    println(Option.is_none(user.name))
    println(json.encode(user)?)
    Ok(())
```

The decoder's second argument is a compile-time type reference. It uses the type
namespace, including imported types and transparent aliases; it never evaluates a
same-spelled function or value. Arbitrary expressions are not type targets.
Input pipes work, for example `text |> json.decode(User)`.

Supported concrete wire types are Int, Float, Bool, String, Unit, `json.Value`,
tuples, Lists, `Map(String, value)`, nullable-safe Options, and regular recursive
records and transparent newtypes marked `derive(Json)`. Concrete instantiations
of generic records and newtypes are supported.
Records retain declaration order; maps retain their existing entry order.
Unknown record fields fail strictly. A missing Option field becomes None; other
missing fields fail. None and Unit encode as JSON null. Option payloads must not
themselves accept null, so `Option(Unit)`, `Option(json.Value)`, and nested Options
are rejected. Int conversion preserves the full signed 64-bit range; Float and
text conversion reuse the dynamic API's existing exact adapters.

This checkpoint does not implement general Json traits, user codec implementations,
generic codec function constraints, sums, or unions.
Unsupported derivations are diagnosed even when unused. Result-bearing values,
including nested fields or containers, cannot be serialized: converting a Result
to opaque JSON does not acknowledge its error obligation.

## Errors and paths

Existing error codes, messages, numeric conversion rules and parse offsets remain
unchanged. Code 12 means `unknown JSON object field`. `json.error_path(error)`
returns a JSON Pointer: root is empty, array indices are decimal, `~` becomes
`~0`, and `/` becomes `~1`. A key `a/b~c` containing a failing item at index 1
therefore reports `/a~1b~0c/1`.

Conversion errors have offset -1; parse errors preserve the original byte offset
and have an empty path. Unknown fields report their key path. JSON keys containing
NUL report code 10 at the containing object. Missing required fields report code 6
at the field path. Path storage is reserved before descent; if path growth exceeds
a codec limit, code 4 reports the last successfully entered parent path. Reporting
a path never replaces an already selected error's code or offset.

## Limits and verification

Each codec operation shares one allowance across parsing, conversion, primitive
adapters, paths and output: 1 MiB input, 16 MiB output, 32 MiB logical allocation,
100,000 expanded nodes, depth 128 and 64 MiB work units. These are deterministic
logical accounting limits, not a guarantee against physical allocator failure.
Interactive execution additionally retains its existing per-entry, cleanup and
stored-value allowances. It may report an interactive resource fault before a
native codec limit.

Concrete compiler plans contain at most 4,096 entries. Plan validation shares
400,000 work units across an executable program; repeated immutable plan identities
are reused. QBE descriptor output has a separate aggregate 16 MiB limit. These
bounds also apply to independently supplied executable IR, including inactive
function bodies. Source type targets never enter executable IR.

`test_rust_json_codecs.py` verifies native source behavior and atomic rejection;
`test_runtime_json_codecs.py` checks the ABI, shared sibling allowance, exact path
boundary and preservation of original errors in debug, release and ASan/UBSan.
The same source corpus is exercised in the REPL. Existing dynamic JSON runtime
and numeric-oracle gates remain required.

## Regular recursive records

A derived record may refer back to itself or to another derived record through
List, Map or a nullable-safe Option. Concrete generic instances retain separate
schema identities. The compiler closes a finite indexed graph rather than
expanding a recursive record into an infinite tree. Finite generic permutations
can close too; type-changing recursion that exceeds the existing type/plan/work
bounds is rejected.

Every declared codec must admit a finite value. `Node { children: List(Node) }`
has the empty-list base case. A strict cycle such as `Loop { next: Loop }` has no
finite value and is rejected as a codec, even when unused. Other fields on a
recursive record are still validated, so a cycle never hides a Result or an
unsupported function payload.

Schema reuse does not replenish runtime allowances. Each executed visit spends
the existing depth/work/node budgets. A repeated record/list chain at depths
127 and 128 preserves the same success/error boundary in native and interactive
execution; a hostile native cyclic value terminates with code 4. The path and
original-error preservation rules above continue to apply.

## Transparent newtypes

Newtypes opt in with an explicit header, for example
`newtype UserId derive(Json) = UserId(Int)` or
`newtype Box(a) derive(Json) = Packed(a)`. A type without this opt-in has no codec.
Encoding and decoding use the payload's wire form while preserving distinct
source type identity. Native constructors, accessors and codec adaptation add no
wrapper allocation, including for nested Float payloads. Existing numeric/text
conversion rules are unchanged; Float JSON uses the established 17-digit format.
The REPL also erases the wrapper representation, retaining its checked source type.

Nullability follows the payload. A newtype wrapping Option(Int) accepts explicit
JSON null. It is still a **required** record field: missing-field defaults apply
only to actual Option fields. Conversely, Option of a newtype that accepts null
is rejected as ambiguous. Newtype identity does not grant implicit Json support
to Map keys: they remain exactly String.

Regular recursive newtypes such as a wrapper over List of itself share the same
finite-graph and runtime-budget rules as recursive records. Strict unboxed cycles
remain invalid. Every transparent wrapper layer spends work and depth, even
though it adds no native allocation. Result-bearing payloads remain unsupported.
