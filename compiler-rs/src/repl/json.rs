//! Immutable JSON semantics with native-profile and aggregate interactive budgets.
use super::*;
mod convert;
mod parse;
mod value;

const INPUT: usize = 1_048_576;
const OUTPUT: usize = 16_777_216;
const ALLOC: usize = 33_554_432;
const NODES: usize = 100_000;
const DEPTH: usize = 128;
pub(super) type Json = Rc<Node>;
#[derive(Debug)]
pub(super) struct Node {
    kind: Kind,
    offset: usize,
    height: usize,
    nodes: usize,
    encoded: usize,
}
#[derive(Debug)]
enum Kind {
    Null,
    Bool(bool),
    Number(String),
    String(String),
    Array(Vec<Json>),
    Object(Vec<(Json, Json)>, Vec<usize>),
}
/// Opaque values are never compared structurally, even by internal evaluator helpers.
impl PartialEq for Node {
    /// Compare node identities only; opaque JSON never gains structural source equality.
    fn eq(&self, other: &Self) -> bool {
        std::ptr::eq(self, other)
    }
}
#[derive(Clone, Copy, Debug, PartialEq)]
pub(super) struct Error {
    code: u8,
    offset: i64,
}
type Result<T> = std::result::Result<T, Error>;
/// Construct a stable code/byte-offset failure; code zero is reserved for evaluator exhaustion.
fn error(code: u8, offset: i64) -> Error {
    Error { code, offset }
}
/// Return the native NUL-terminated prefix, scanning no farther than the input limit plus one.
fn input(text: &str) -> &str {
    let end = text
        .bytes()
        .take(INPUT + 1)
        .position(|b| b == 0)
        .unwrap_or(text.len());
    &text[..end]
}

#[derive(Debug)]
pub(super) struct Limits {
    work: usize,
    allocated: usize,
}
impl Limits {
    /// Initialize independent work/allocation allowances of `bytes` for one evaluation phase.
    pub(super) fn new(bytes: usize) -> Self {
        Self {
            work: bytes,
            allocated: bytes,
        }
    }
    /// Consume `amount` before work or allocation; exhaustion empties the allowance and returns a fault.
    fn charge(left: &mut usize, amount: usize) -> Result<()> {
        if amount > *left {
            *left = 0;
            return Err(error(0, -1));
        }
        *left -= amount;
        Ok(())
    }
}
struct Budget<'a> {
    limits: &'a mut Limits,
    work: usize,
    allocated: usize,
    nodes: usize,
    at: usize,
}
impl<'a> Budget<'a> {
    /// Reserve bounded input-scan work in a native-profile operation linked to aggregate limits.
    fn new(limits: &'a mut Limits, bytes: usize) -> Result<Self> {
        let mut budget = Self {
            limits,
            work: 8 * bytes + 64 * NODES,
            allocated: 0,
            nodes: 0,
            at: 0,
        };
        budget.work(8 * bytes)?;
        Ok(budget)
    }
    /// Charge aggregate and operation work before executing `units` of bounded processing.
    fn work(&mut self, units: usize) -> Result<()> {
        Limits::charge(&mut self.limits.work, units)?;
        if units > self.work {
            return Err(error(4, self.at as i64));
        }
        self.work -= units;
        Ok(())
    }
    /// Charge logical native bytes before allocation; zero-byte requests retain the native one-byte rule.
    fn allocate(&mut self, bytes: usize) -> Result<()> {
        let bytes = bytes.max(1);
        Limits::charge(&mut self.limits.allocated, bytes)?;
        if bytes > ALLOC - self.allocated {
            return Err(error(4, self.at as i64));
        }
        self.allocated += bytes;
        Ok(())
    }
    /// Reserve one native-profile node plus larger Rust storage before publishing its Rc allocation.
    fn node(&mut self) -> Result<()> {
        if self.nodes == NODES {
            return Err(error(4, self.at as i64));
        }
        self.allocate(72)?;
        let actual = std::mem::size_of::<Node>() + 2 * std::mem::size_of::<usize>();
        Limits::charge(&mut self.limits.allocated, actual.saturating_sub(72))?;
        self.nodes += 1;
        Ok(())
    }
}

impl Machine {
    /// Route the checked registry through safe semantic values; failures remain Results.
    pub(super) fn json(&mut self, symbol: &str, args: &[Value]) -> Option<Eval<Value>> {
        let operation = symbol.strip_prefix("fern_json_value_")?;
        let limits = if self.cleanup_depth == 0 {
            &mut self.json_limits
        } else {
            &mut self.json_cleanup
        };
        Some(match dispatch(operation, args, limits) {
            Ok(value) => Ok(value),
            Err(Error { code: 0, .. }) => Err(fault("interactive evaluation limit exceeded")),
            Err(failure) => Ok(Value::Sum(1, Rc::new(vec![Value::JsonError(failure)]))),
        })
    }
}
/// Wrap the success payload only for APIs whose checked contract returns Result.
fn dispatch(operation: &str, args: &[Value], limits: &mut Limits) -> Result<Value> {
    for argument in args {
        if let Value::String(text) = argument {
            Limits::charge(&mut limits.work, text.len().min(INPUT + 1))?;
        }
    }
    let value = operation_value(operation, args, limits)?;
    if matches!(
        operation,
        "null"
            | "from_bool"
            | "from_int"
            | "is_null"
            | "error_code"
            | "error_offset"
            | "error_message"
    ) {
        Ok(value)
    } else {
        Ok(Value::Sum(0, Rc::new(vec![value])))
    }
}
/// Evaluate checked JSON arguments under `limits`, returning a payload or an ordinary JSON error.
fn operation_value(operation: &str, args: &[Value], limits: &mut Limits) -> Result<Value> {
    match (operation, args) {
        ("parse", [Value::String(text)]) => parse::document(input(text), limits).map(Value::Json),
        ("error_code", [Value::JsonError(e)]) => Ok(Value::Int(e.code.into())),
        ("error_offset", [Value::JsonError(e)]) => Ok(Value::Int(e.offset)),
        ("error_message", [Value::JsonError(e)]) => {
            Ok(Value::String(Rc::new(message(e.code).into())))
        }
        ("is_null", [Value::Json(value)]) => Ok(Value::Bool(matches!(value.kind, Kind::Null))),
        (name, [Value::Json(value), rest @ ..]) => value::access(name, value, rest, limits),
        _ => value::build(operation, args, limits),
    }
}
/// Return the static message for a stable error code without embedding input text.
fn message(code: u8) -> &'static str {
    [
        "",
        "invalid JSON syntax",
        "invalid JSON Unicode",
        "duplicate JSON object key",
        "JSON resource limit exceeded",
        "JSON value has wrong type",
        "JSON object key not found",
        "JSON array index out of bounds",
        "JSON number out of range",
        "JSON number is not an integer",
        "JSON string contains NUL",
        "JSON number is not finite",
    ]
    .get(code as usize)
    .copied()
    .unwrap_or("invalid JSON error")
}

/// Count actual retained Rc identities rather than cached expanded subtree sizes.
#[derive(Default)]
pub(super) struct Storage {
    seen: std::collections::HashSet<usize>,
}
impl Storage {
    /// Visit unique JSON nodes reachable from `root`, updating storage counters or rejecting the retained graph.
    pub(super) fn add(
        &mut self,
        root: &Json,
        bytes: &mut usize,
        count: &mut usize,
    ) -> std::result::Result<(), String> {
        let mut pending = vec![root];
        while let Some(node) = pending.pop() {
            if !self.seen.insert(Rc::as_ptr(node) as usize) {
                continue;
            }
            *count = count.saturating_add(1);
            *bytes = bytes
                .saturating_add(std::mem::size_of::<Node>() + 2 * std::mem::size_of::<usize>());
            match &node.kind {
                Kind::Number(text) | Kind::String(text) => {
                    *bytes = bytes.saturating_add(text.capacity())
                }
                Kind::Array(values) => {
                    *bytes = bytes.saturating_add(values.capacity() * std::mem::size_of::<Json>());
                    pending.extend(values);
                }
                Kind::Object(values, index) => {
                    *bytes = bytes.saturating_add(
                        values.capacity() * std::mem::size_of::<(Json, Json)>()
                            + index.capacity() * std::mem::size_of::<usize>(),
                    );
                    pending.extend(values.iter().flat_map(|(k, v)| [k, v]));
                }
                _ => {}
            }
            if *bytes > 16 * 1024 * 1024 || count.saturating_add(pending.len()) > 200_000 {
                return Err("interactive value storage limit exceeded".into());
            }
        }
        Ok(())
    }
}

#[cfg(test)]
mod tests;
