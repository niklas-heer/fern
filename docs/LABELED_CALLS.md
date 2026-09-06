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
Exact Bool parameters and parameters sharing an identical declared scheme type
require labels. The interface is fixed after declaration inference and alias/union
normalization, before call-site specialization. Distinct generic variables and
distinct newtypes remain distinct; a union merely containing Bool is not exact
Bool. Repeated instances of the same generic parameter do require labels.

For example, both parameters of `subtract` above require labels. A unique String
parameter can still form a positional prefix before labeled Int/Bool parameters.
The rule applies to direct source functions; erased and builtin interfaces are
described below.

A simple parameter binding supplies its external name. Pattern parameters can
declare one explicitly, for example `fn choose(enabled true: Bool) -> Int: 1`.
All clauses expose a stable agreed interface; literal/wildcard clauses may inherit
it from another clause. Conflicting names need consistent explicit external names.
Required positions without an agreed name must declare an explicit external label.
External labels and local pattern bindings occupy distinct source roles.

A pipe evaluates its input before the written call arguments. A labeled placeholder
such as `9 |> subtract(right: 2, left: _)` chooses its parameter explicitly.
When the selected parameter requires a label, the pipe must use a labeled hole.
Reordering does not move side effects, returns or Result propagation across other
arguments. Imported functions and reexports retain their original interfaces.

Labels erase when a function becomes a structural function value. Those values,
lambdas, runtime/compiler builtins and constructors use positional calls and reject
labels. The C frontend is not an oracle for source label semantics.

Formatting roundtrips labels, external pattern names and labeled pipe holes.
LSP label tokens deliberately have no navigation target in this checkpoint;
ordinary local pattern-binding navigation remains available. Full label navigation
and Tree-sitter grammar support remain open.

`scripts/test_rust_labels.py` executes five native programs and checks eight invalid
programs preserve an existing output's bytes, permissions and modification time.
Rust tests cover modules, contextual callback types, source spans, formatting,
presentation and hostile AST budgets.
Public AST labels share the parser's identifier/keyword rules and reject reversed
spans. Finalized scheme classification has a separate 400,000-unit work ceiling.
