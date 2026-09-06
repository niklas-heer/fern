//! Immutable subtree metadata, indexed objects and bounded semantic projections.
use super::*;
mod build;
pub(super) use build::build;

/// Seal an already allocated decoded string with its exact escaped byte count and original offset.
pub(super) fn text_node(text: String, offset: usize) -> Json {
    let encoded = 2 + text.bytes().map(escape_size).sum::<usize>();
    Rc::new(Node {
        kind: Kind::String(text),
        offset,
        height: 1,
        nodes: 1,
        encoded,
    })
}
/// Return the native encoded byte count for one decoded UTF-8 byte, including JSON controls.
fn escape_size(byte: u8) -> usize {
    match byte {
        b'"' | b'\\' | b'\x08' | b'\x0c' | b'\n' | b'\r' | b'\t' => 2,
        0..=31 => 6,
        _ => 1,
    }
}
/// Validate expanded child metadata, preserving order and building a checked object index before publication.
pub(super) fn seal(
    children: Vec<Json>,
    object: bool,
    offset: usize,
    budget: &mut Budget<'_>,
) -> Result<Json> {
    let mut node = Node {
        kind: Kind::Null,
        offset,
        height: 1,
        nodes: 1,
        encoded: 2 + children.len().saturating_sub(1),
    };
    for child in &children {
        if child.encoded > OUTPUT - node.encoded
            || child.nodes > NODES - node.nodes
            || child.height >= DEPTH
        {
            return Err(error(4, budget.at as i64));
        }
        node.encoded += child.encoded;
        node.nodes += child.nodes;
        node.height = node.height.max(child.height + 1);
    }
    node.kind = if object {
        Limits::charge(
            &mut budget.limits.allocated,
            children.len() * std::mem::size_of::<Json>(),
        )?;
        let mut members = Vec::with_capacity(children.len() / 2);
        let mut children = children.into_iter();
        while let (Some(key), Some(value)) = (children.next(), children.next()) {
            members.push((key, value));
        }
        let index = sorted(&members, budget)?;
        Kind::Object(members, index)
    } else {
        Kind::Array(children)
    };
    Ok(Rc::new(node))
}
/// Read a validated object key String; non-key internal nodes produce an empty fallback.
fn text(node: &Node) -> &str {
    match &node.kind {
        Kind::String(text) => text,
        _ => "",
    }
}
/// Compare decoded key bytes lexically, charging each examined byte before comparison.
fn compare(a: &str, b: &str, budget: &mut Budget<'_>) -> Result<std::cmp::Ordering> {
    for (a, b) in a.bytes().zip(b.bytes()) {
        budget.work(1)?;
        if a != b {
            return Ok(a.cmp(&b));
        }
    }
    Ok(a.len().cmp(&b.len()))
}
/// Build a stable bounded index and reject the later key in the first sorted duplicate group.
fn sorted(members: &[(Json, Json)], budget: &mut Budget<'_>) -> Result<Vec<usize>> {
    let count = members.len();
    if count == 0 {
        return Ok(Vec::new());
    }
    budget.allocate(count * 8)?;
    budget.allocate(count * 8)?;
    let mut index: Vec<_> = (0..count).collect();
    let mut scratch = vec![0; count];
    let mut width = 1;
    while width < count {
        for base in (0..count).step_by(width * 2) {
            merge(members, &index, &mut scratch, base, width, budget)?;
        }
        std::mem::swap(&mut index, &mut scratch);
        width *= 2;
    }
    for pair in index.windows(2) {
        let a = &members[pair[0]].0;
        let b = &members[pair[1]].0;
        if compare(text(a), text(b), budget)?.is_eq() {
            return Err(error(3, b.offset as i64));
        }
    }
    Ok(index)
}
/// Merge two bounded index runs into scratch storage, charging comparisons and steps before writes.
fn merge(
    members: &[(Json, Json)],
    index: &[usize],
    scratch: &mut [usize],
    base: usize,
    width: usize,
    budget: &mut Budget<'_>,
) -> Result<()> {
    let middle = (base + width).min(index.len());
    let end = (middle + width).min(index.len());
    let (mut a, mut b) = (base, middle);
    for slot in &mut scratch[base..end] {
        budget.work(1)?;
        let left = b == end
            || (a < middle
                && !compare(
                    text(&members[index[a]].0),
                    text(&members[index[b]].0),
                    budget,
                )?
                .is_gt());
        *slot = index[if left {
            let i = a;
            a += 1;
            i
        } else {
            let i = b;
            b += 1;
            i
        }];
    }
    Ok(())
}

/// Evaluate a checked projection/conversion, retaining opaque children and returning precise domain errors.
pub(super) fn access(
    operation: &str,
    node: &Json,
    rest: &[Value],
    limits: &mut Limits,
) -> Result<Value> {
    match (operation, &node.kind, rest) {
        ("stringify", _, []) => stringify(node, limits).map(|s| Value::String(Rc::new(s))),
        ("get", Kind::Object(members, index), [Value::String(key)]) => {
            get(members, index, input(key), limits).map(Value::Json)
        }
        ("at", Kind::Array(values), [Value::Int(index)]) => usize::try_from(*index)
            .ok()
            .and_then(|n| values.get(n))
            .cloned()
            .map(Value::Json)
            .ok_or(error(7, -1)),
        ("length", Kind::Array(values), []) => Ok(Value::Int(values.len() as i64)),
        ("length", Kind::Object(values, _), []) => Ok(Value::Int(values.len() as i64)),
        ("as_bool", Kind::Bool(value), []) => Ok(Value::Bool(*value)),
        ("as_int", Kind::Number(text), []) => {
            Limits::charge(&mut limits.work, text.len() * 2)?;
            convert::integer(text).map(Value::Int)
        }
        ("as_float", Kind::Number(text), []) => {
            Limits::charge(&mut limits.work, text.len())?;
            convert::float(text).map(Value::Float)
        }
        ("number_text", Kind::Number(text), []) => copied(text, limits),
        ("as_string", Kind::String(text), []) if !text.contains('\0') => copied(text, limits),
        ("as_string", Kind::String(_), []) => Err(error(10, -1)),
        ("elements", Kind::Array(values), []) => elements(values, limits),
        ("members", Kind::Object(values, _), []) => members(values, limits),
        _ => Err(error(5, -1)),
    }
}
/// Charge work and output allocation before copying representable text into a Fern String payload.
fn copied(text: &str, limits: &mut Limits) -> Result<Value> {
    Limits::charge(&mut limits.work, text.len())?;
    Limits::charge(&mut limits.allocated, text.len() + 1)?;
    Ok(Value::String(Rc::new(text.into())))
}
/// Compute actual Rc/Vec/value storage for a bounded semantic collection of `count` entries.
fn collection_bytes(count: usize) -> usize {
    count * std::mem::size_of::<Value>()
        + std::mem::size_of::<Vec<Value>>()
        + 2 * std::mem::size_of::<usize>()
}
/// Copy array references into fresh semantic list storage after charging logical and actual allocation.
fn elements(values: &[Json], limits: &mut Limits) -> Result<Value> {
    Limits::charge(&mut limits.work, values.len())?;
    let actual = collection_bytes(values.len());
    Limits::charge(
        &mut limits.allocated,
        actual.max(values.len().max(1) * 8 + 24),
    )?;
    Ok(Value::List(Rc::new(
        values.iter().cloned().map(Value::Json).collect(),
    )))
}
/// Copy ordered key/value references into tagged semantic tuples under the native adapter and aggregate caps.
fn members(values: &[(Json, Json)], limits: &mut Limits) -> Result<Value> {
    let bytes = values.len().max(1) * 56 + 48;
    Limits::charge(&mut limits.work, values.len())?;
    let actual = collection_bytes(values.len()) + values.len() * collection_bytes(2);
    Limits::charge(&mut limits.allocated, bytes.max(actual))?;
    if bytes > ALLOC {
        return Err(error(4, -1));
    }
    Ok(Value::List(Rc::new(
        values
            .iter()
            .map(|(k, v)| {
                Value::Sum(
                    0,
                    Rc::new(vec![Value::Json(k.clone()), Value::Json(v.clone())]),
                )
            })
            .collect(),
    )))
}
/// Binary-search a bounded decoded key using the stable index, distinguishing absence from profile failure.
fn get(members: &[(Json, Json)], index: &[usize], key: &str, limits: &mut Limits) -> Result<Json> {
    if key.len() > INPUT {
        return Err(error(4, -1));
    }
    let (mut low, mut high) = (0, index.len());
    while low < high {
        let middle = low + (high - low) / 2;
        let slot = index[middle];
        Limits::charge(
            &mut limits.work,
            key.len().min(text(&members[slot].0).len()),
        )?;
        match key.as_bytes().cmp(text(&members[slot].0).as_bytes()) {
            std::cmp::Ordering::Equal => return Ok(members[slot].1.clone()),
            std::cmp::Ordering::Less => high = middle,
            std::cmp::Ordering::Greater => low = middle + 1,
        }
    }
    Err(error(6, -1))
}
/// Reserve validated output size and traversal work before encoding one immutable subtree.
fn stringify(node: &Node, limits: &mut Limits) -> Result<String> {
    if node.encoded > OUTPUT {
        return Err(error(4, -1));
    }
    Limits::charge(&mut limits.work, node.encoded + node.nodes)?;
    Limits::charge(&mut limits.allocated, node.encoded + 1)?;
    let mut out = String::with_capacity(node.encoded);
    encode(node, &mut out);
    debug_assert_eq!(out.len(), node.encoded);
    Ok(out)
}
/// Append a validated subtree into reserved output; sealed depth and expanded-node metadata bound recursion.
pub(super) fn encode(node: &Node, out: &mut String) {
    match &node.kind {
        Kind::Null => out.push_str("null"),
        Kind::Bool(v) => out.push_str(if *v { "true" } else { "false" }),
        Kind::Number(text) => out.push_str(text),
        Kind::String(text) => encode_string(text, out),
        Kind::Array(values) => {
            out.push('[');
            for (i, value) in values.iter().enumerate() {
                if i != 0 {
                    out.push(',');
                }
                encode(value, out);
            }
            out.push(']');
        }
        Kind::Object(values, _) => {
            out.push('{');
            for (i, (key, value)) in values.iter().enumerate() {
                if i != 0 {
                    out.push(',');
                }
                encode_string(text(key), out);
                out.push(':');
                encode(value, out);
            }
            out.push('}');
        }
    }
}
/// Append exact JSON escapes for decoded scalar text, preserving non-ASCII bytes and escaped NUL.
fn encode_string(text: &str, out: &mut String) {
    out.push('"');
    for ch in text.chars() {
        match ch {
            '"' => out.push_str("\\\""),
            '\\' => out.push_str("\\\\"),
            '\u{8}' => out.push_str("\\b"),
            '\u{c}' => out.push_str("\\f"),
            '\n' => out.push_str("\\n"),
            '\r' => out.push_str("\\r"),
            '\t' => out.push_str("\\t"),
            '\0'..='\u{1f}' => {
                out.push_str("\\u00");
                out.push(char::from_digit((ch as u32) >> 4, 16).unwrap());
                out.push(char::from_digit((ch as u32) & 15, 16).unwrap());
            }
            _ => out.push(ch),
        }
    }
    out.push('"');
}
