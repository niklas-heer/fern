# Source call labels

The Rust frontend accepts labels on direct source function calls. Labels name
parameter positions; expressions still execute once, in the order written.

```fern
fn subtract(left: Int, right: Int) -> Int:
    left - right

fn main() -> Int:
    subtract(right: 2, left: 9)
```

This returns 7. Positional arguments may form a prefix, followed by labels in any
order. Unknown, duplicate, missing or already supplied positions are errors.
In this initial checkpoint wholly positional source calls remain accepted.
Decision7's mandatory Bool and repeated-type policy is the next enforcement phase.

A simple parameter binding supplies its external name. Pattern parameters can
declare one explicitly, for example `fn choose(enabled true: Bool) -> Int: 1`.
All clauses expose a stable agreed interface; literal/wildcard clauses may inherit
it from another clause. Conflicting names need consistent explicit external names.
External labels and local pattern bindings occupy distinct source roles.

A pipe evaluates its input before the written call arguments. A labeled placeholder
such as `9 |> subtract(right: 2, left: _)` chooses its parameter explicitly.
Reordering does not move side effects, returns or Result propagation across other
arguments. Imported functions and reexports retain their original interfaces.

Labels erase when a function becomes a structural function value. Those values,
lambdas, runtime/compiler builtins and constructors use positional calls and reject
labels. The C frontend is not an oracle for source label semantics.

Formatting roundtrips labels, external pattern names and labeled pipe holes.
LSP label tokens deliberately have no navigation target in this checkpoint;
ordinary local pattern-binding navigation remains available. Full label navigation
and Tree-sitter grammar support remain open.

`scripts/test_rust_labels.py` executes four native programs and checks four invalid
programs preserve an existing output's bytes, permissions and modification time.
Rust tests cover modules, contextual callback types, source spans, formatting,
presentation and hostile AST budgets.
