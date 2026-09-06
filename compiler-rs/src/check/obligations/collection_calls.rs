//! Eager map/filter traversal proves every callback while preserving produced and retained families.
use super::*;
impl Engine<'_> {
    /// Evaluate one typed callback summary universally; empty input never executes that callback.
    pub(super) fn collection_transform(
        &mut self,
        builtin: ir::Builtin,
        values: &[Value],
        source: &[ir::Expr],
        result: &Type,
        span: Span,
    ) -> Checked<Value> {
        let [list, callback] = values else {
            return self.unsupported(span);
        };
        let [list_expr, callback_expr] = source else {
            return self.unsupported(span);
        };
        let Type::List(item) = &list_expr.ty else {
            return self.unsupported(span);
        };
        let Type::List(output) = result else {
            return self.unsupported(span);
        };
        let callback_result = if builtin == ir::Builtin::ListFilter {
            &Type::Bool
        } else {
            output.as_ref()
        };
        let summary =
            self.collection_callback(callback, &callback_expr.ty, item, callback_result, span)?;
        self.work = summary.work;
        let nonempty = self.list_nonempty(list, span, 0)?;
        if nonempty == Predicate::FALSE {
            return self.node(
                Region::List {
                    items: vec![],
                    exact: true,
                    nonempty,
                },
                span,
            );
        }
        let parent = self.path;
        self.path = self
            .predicates
            .and(parent, nonempty, &mut self.work, span)?;
        let family = substitute::Substitution::family(self, list, false, span, 0)?;
        let mut substitution = substitute::Substitution::traversal(&summary);
        let output = substitution.apply(self, &[family.clone(), callback.clone()], span)?;
        let output = if builtin == ir::Builtin::ListFilter {
            self.filtered_family(family, output, nonempty, span)?
        } else {
            self.node(
                Region::List {
                    items: vec![output],
                    exact: false,
                    nonempty,
                },
                span,
            )?
        };
        self.path = parent;
        Ok(output)
    }
    /// The callable environment is a formal input so captures preserve actual Boolean/alias facts.
    pub(super) fn collection_callback(
        &mut self,
        callback: &Value,
        callable_type: &Type,
        item: &Type,
        result: &Type,
        span: Span,
    ) -> Checked<Summary> {
        let mut engine = Engine::new(self.program);
        engine.work = self.work;
        engine.mode = self.mode;
        engine.summaries = self.summaries;
        engine.relevance = self.relevance;
        engine.effect_cache = self.effect_cache.clone();
        let input = engine.fresh(item, Some(0), span, 0)?;
        let shape = engine.effect_shape(callback, span, 0)?;
        let callable = engine.fresh_shaped(callable_type, Some(1), Some(&shape), span, 0)?;
        engine.inputs = vec![input.clone(), callable.clone()];
        engine.used_inputs.extend([0, 1]);
        let output = engine.invoke(&callable, &[input], result, span, 0)?;
        engine.exit(&output, span, 0)?;
        let output = engine.output(span)?;
        engine.finish(output)
    }
    /// A filter's retained output covers every input only when its predicate is unconditionally true.
    fn filtered_family(
        &mut self,
        family: Value,
        selected: Value,
        nonempty: Predicate,
        span: Span,
    ) -> Checked<Value> {
        let selected = self.condition(&selected, span)?;
        if selected == Predicate::FALSE {
            return self.node(
                Region::List {
                    items: vec![],
                    exact: true,
                    nonempty: Predicate::FALSE,
                },
                span,
            );
        }
        let (family, nonempty) = if selected == Predicate::TRUE {
            (family, nonempty)
        } else {
            let retained = self.predicates.variable(&mut self.work, span)?;
            let retained = self
                .predicates
                .and(nonempty, retained, &mut self.work, span)?;
            (family.partial(), retained)
        };
        self.node(
            Region::List {
                items: vec![family],
                exact: false,
                nonempty,
            },
            span,
        )
    }
}
