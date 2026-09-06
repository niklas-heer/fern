//! External parameter identities are separate from lexical bindings and generated IR.
use super::*;

pub(crate) struct LabelSelection {
    pub function: String,
    pub position: usize,
}

impl Index<'_> {
    /// Retain the first source contributor to each stable clause interface position.
    pub(super) fn interfaces(&mut self, program: &ast::Program) -> Option<()> {
        for function in &program.functions {
            self.charge(function.params.len().saturating_add(1), 0)?;
            if function.span == Span::default() {
                continue;
            }
            let slots = self
                .interfaces
                .entry(function.name.clone())
                .or_insert_with(|| vec![None; function.params.len()]);
            for (slot, param) in slots.iter_mut().zip(&function.params) {
                if slot.is_some() {
                    continue;
                }
                *slot = param.label.clone().or_else(|| match &param.pattern.kind {
                    ast::PatternKind::Bind(name) => Some(ast::ArgumentLabel {
                        name: name.clone(),
                        span: param.pattern.span,
                    }),
                    _ => None,
                });
            }
        }
        Some(())
    }

    /// Local roots erase source interfaces; resolved global nodes bypass this lexical lookup.
    pub(super) fn source_callee(&self, name: &str, locals: &Bindings) -> Option<String> {
        if locals.contains_key(name.split('.').next()?) {
            return None;
        }
        let canonical = self.visible_values.get(name)?;
        self.interfaces
            .contains_key(canonical)
            .then(|| canonical.clone())
    }

    /// Mark label syntax even when invalid, preventing accidental local/global fallback.
    pub(super) fn argument_label(&mut self, label: &ast::ArgumentLabel, callee: Option<&str>) {
        if !self.contains(label.span) {
            return;
        }
        self.blocked = true;
        self.target = None;
        self.label = None;
        let selected = callee.and_then(|name| {
            self.interfaces
                .get(name)?
                .iter()
                .enumerate()
                .find_map(|(position, item)| {
                    let item = item.as_ref()?;
                    (item.name == label.name).then_some((name, position, item.span))
                })
        });
        if let Some((function, position, target)) = selected {
            self.target = Some(target);
            self.token = Some(label.span);
            self.label = Some(LabelSelection {
                function: function.into(),
                position,
            });
        } else {
            self.token = None;
        }
    }

    /// Include a removed labeled placeholder in the same interface as remaining pipe arguments.
    pub(super) fn pipe(
        &mut self,
        expression: &ast::Expr,
        locals: &Bindings,
        depth: usize,
    ) -> Option<()> {
        let (value, args, label, callee) = match &expression.kind {
            ast::ExprKind::Pipe {
                value,
                name,
                args,
                label,
                ..
            } => (value, args, label, self.source_callee(name, locals)),
            ast::ExprKind::GlobalPipe {
                value,
                resolved,
                args,
                label,
                ..
            } => (value, args, label, Some(resolved.clone())),
            _ => return None,
        };
        self.pipe_reference(expression, locals);
        self.expression(value, locals, depth)?;
        if let Some(label) = label {
            self.argument_label(label, callee.as_deref());
        }
        self.arguments(args, callee.as_deref(), locals, depth)
    }
}
