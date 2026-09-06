//! Emit validated concrete wire descriptors and one source-ordered runtime operation.
use super::*;
use crate::json_codec::{Direction, Kind, Plan};
impl Emitter<'_> {
    /// Preserve the operand's full payload width; runtime errors remain ordinary heap Results.
    pub(super) fn json_codec(
        &mut self,
        expr: &Expr,
        locals: &mut Locals,
        depth: usize,
    ) -> Lowering<(Type, String)> {
        let ExprKind::JsonCodec {
            direction,
            input,
            plan,
        } = &expr.kind
        else {
            return Err(invalid(expr.span, "expected JSON codec operation"));
        };
        let value = self.expr(input, locals, depth)?;
        let value = self.payload(locals, &input.ty, value);
        let root = self.codec_table(plan, expr.span)?;
        let name = match direction {
            Direction::Encode => "encode",
            Direction::Decode => "decode",
        };
        let value = self.assign(
            locals,
            expr.ty.clone(),
            &format!("call $fern_json_codec_{name}(l {root}, l {value})"),
        );
        Ok((expr.ty.clone(), value))
    }
    /// Bound aggregate descriptor/name output before allocating formatting intermediates.
    fn reserve_codec_data(&mut self, plan: &Plan, span: Span) -> Lowering<()> {
        let mut bytes = 0usize;
        for entry in &plan.entries {
            bytes = bytes.saturating_add(256);
            match &entry.kind {
                Kind::Tuple(ids) => bytes = bytes.saturating_add(ids.len().saturating_mul(64)),
                Kind::Record(fields) => {
                    for field in fields {
                        bytes = bytes
                            .saturating_add(field.name.len().saturating_mul(8).saturating_add(512));
                    }
                }
                _ => bytes = bytes.saturating_add(64),
            }
        }
        self.codec_data_bytes = self.codec_data_bytes.saturating_add(bytes);
        if self.codec_data_bytes > 16 * 1024 * 1024 {
            return Err(invalid(span, "JSON codec descriptor output limit exceeded"));
        }
        Ok(())
    }
    /// All descriptors use four native 64-bit words and validated finite-graph child pointers.
    fn codec_table(&mut self, plan: &std::rc::Rc<Plan>, span: Span) -> Lowering<String> {
        let identity = std::rc::Rc::as_ptr(plan) as usize;
        if let Some(table) = self.codec_tables.get(&identity) {
            return Ok(table.clone());
        }
        self.reserve_codec_data(plan, span)?;
        let unique = self.strings;
        self.strings += 1;
        for (index, entry) in plan.entries.iter().enumerate() {
            let (tag, children) = descriptor(&entry.kind);
            let prefix = format!("$json_codec_{unique}_{index}");
            let child_data = if children.is_empty() {
                "0".into()
            } else {
                let refs = children
                    .iter()
                    .map(|id| format!("l $json_codec_{unique}_{id}"))
                    .collect::<Vec<_>>()
                    .join(", ");
                self.data
                    .push_str(&format!("data {prefix}_children = {{ {refs} }}\n"));
                format!("{prefix}_children")
            };
            let names = if let Kind::Record(fields) = &entry.kind {
                let mut names = Vec::new();
                for field in fields {
                    names.push(format!("l {}", self.string(&field.name, span)?));
                }
                if names.is_empty() {
                    "0".into()
                } else {
                    self.data.push_str(&format!(
                        "data {prefix}_names = {{ {} }}\n",
                        names.join(", ")
                    ));
                    format!("{prefix}_names")
                }
            } else {
                "0".into()
            };
            self.data.push_str(&format!(
                "data {prefix} = {{ l {tag}, l {}, l {child_data}, l {names} }}\n",
                children.len()
            ));
        }
        let table = format!("$json_codec_{unique}_{}", plan.root);
        self.codec_tables.insert(identity, table.clone());
        Ok(table)
    }
}
/// Match the documented runtime ABI without relying on Rust enum discriminant layout.
fn descriptor(kind: &Kind) -> (usize, Vec<usize>) {
    match kind {
        Kind::Int => (0, vec![]),
        Kind::Float => (1, vec![]),
        Kind::Bool => (2, vec![]),
        Kind::String => (3, vec![]),
        Kind::Unit => (4, vec![]),
        Kind::Dynamic => (5, vec![]),
        Kind::Newtype(id) => (11, vec![*id]),
        Kind::List(id) => (6, vec![*id]),
        Kind::Option(id) => (7, vec![*id]),
        Kind::Tuple(ids) => (8, ids.clone()),
        Kind::Map(id) => (9, vec![*id]),
        Kind::Record(fields) => (10, fields.iter().map(|f| f.codec).collect()),
    }
}
