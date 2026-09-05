//! First-class ranges and bounded collection iteration with lexical loop exits.
use super::*;

pub(super) struct LoopTargets {
    next: String,
    done: String,
}

struct Iteration {
    slot: String,
    head: String,
    body: String,
    next: String,
    increment: String,
    done: String,
    end: String,
    inclusive: String,
}

impl Emitter<'_> {
    /// Capture both endpoints once; range iteration remains lazy even at i64 limits.
    pub(super) fn range(
        &mut self,
        start: &Expr,
        end: &Expr,
        inclusive: bool,
        locals: &mut Locals,
        depth: usize,
    ) -> Lowering<(Type, String)> {
        expect_type(start.ty.clone(), Type::Int, start.span)?;
        expect_type(end.ty.clone(), Type::Int, end.span)?;
        let start = self.expr(start, locals, depth)?;
        let end = self.expr(end, locals, depth)?;
        let value = self.assign(locals, Type::Range, "call $fern_alloc(l 24)");
        self.store_field(&value, 0, &start, locals);
        self.store_field(&value, 8, &end, locals);
        self.store_field(&value, 16, &u8::from(inclusive).to_string(), locals);
        Ok((Type::Range, value))
    }

    /// Write a full-width internal field without inventing a source-level field access.
    pub(super) fn store_field(
        &mut self,
        value: &str,
        offset: usize,
        raw: &str,
        locals: &mut Locals,
    ) {
        let address = self.assign(locals, Type::Int, &format!("add {value}, {offset}"));
        self.output
            .push_str(&format!("    storel {raw}, {address}\n"));
    }

    /// Read an audited internal layout field as raw bits for later typed unpacking.
    pub(super) fn raw_field(&mut self, value: &str, offset: usize, locals: &mut Locals) -> String {
        let address = self.assign(locals, Type::Int, &format!("add {value}, {offset}"));
        self.assign(locals, Type::Int, &format!("loadl {address}"))
    }

    /// Jump to the innermost loop without draining this function's deferred callbacks.
    pub(super) fn loop_exit(
        &mut self,
        next: bool,
        span: Span,
        locals: &Locals,
    ) -> Lowering<(Type, String)> {
        let targets = locals
            .loops
            .last()
            .ok_or_else(|| invalid(span, "loop control outside loop"))?;
        let target = if next { &targets.next } else { &targets.done };
        self.output.push_str(&format!("    jmp {target}\n"));
        Err(Exit::Terminated)
    }

    /// Iterate a captured collection, restoring the outer lexical scope on every exit.
    pub(super) fn iteration(
        &mut self,
        pattern: &Pattern,
        iterable: &Expr,
        body: &Expr,
        locals: &mut Locals,
        depth: usize,
    ) -> Lowering<(Type, String)> {
        let item = match &iterable.ty {
            Type::Range => Type::Int,
            Type::List(item) => *item.clone(),
            Type::Map(key, value) => Type::Tuple(vec![*key.clone(), *value.clone()]),
            _ => return Err(invalid(iterable.span, "for requires List, Map, or Range")),
        };
        let collection = self.expr(iterable, locals, depth)?;
        let flow = self.iteration_setup(&collection, &iterable.ty, locals);
        let outer = locals.values.clone();
        let value = self.iteration_item(&flow, &collection, &iterable.ty, &item, locals);
        self.bind_irrefutable(pattern, &item, &value, iterable.span, locals, depth)?;
        locals.loops.push(LoopTargets {
            next: flow.next.clone(),
            done: flow.done.clone(),
        });
        let outcome = self.expr(body, locals, depth);
        locals.loops.pop();
        locals.values = outer;
        match outcome {
            Ok(_) => self.output.push_str(&format!("    jmp {}\n", flow.next)),
            Err(Exit::Terminated) => {}
            Err(error) => return Err(error),
        }
        self.iteration_increment(&flow, locals);
        self.start_block(locals, &flow.done);
        Ok((Type::Unit, "0".into()))
    }

    /// Guard the current induction value before any indexed load or body evaluation.
    fn iteration_setup(&mut self, collection: &str, ty: &Type, locals: &mut Locals) -> Iteration {
        let (start, end, inclusive) = if *ty == Type::Range {
            (
                self.raw_field(collection, 0, locals),
                self.raw_field(collection, 8, locals),
                self.raw_field(collection, 16, locals),
            )
        } else {
            (
                "0".into(),
                self.assign(
                    locals,
                    Type::Int,
                    &format!("call $fern_list_len(l {collection})"),
                ),
                "0".into(),
            )
        };
        let flow = Iteration {
            slot: locals.stack_slot(),
            head: locals.label(),
            body: locals.label(),
            next: locals.label(),
            increment: locals.label(),
            done: locals.label(),
            end,
            inclusive,
        };
        self.output.push_str(&format!(
            "    storel {start}, {}\n    jmp {}\n",
            flow.slot, flow.head
        ));
        self.start_block(locals, &flow.head);
        let index = self.assign(locals, Type::Int, &format!("loadl {}", flow.slot));
        let before = self.assign(locals, Type::Bool, &format!("csltl {index}, {}", flow.end));
        let within = self.assign(locals, Type::Bool, &format!("cslel {index}, {}", flow.end));
        let inclusive = self.assign(
            locals,
            Type::Bool,
            &format!("and {within}, {}", flow.inclusive),
        );
        let available = self.assign(locals, Type::Bool, &format!("or {before}, {inclusive}"));
        self.output.push_str(&format!(
            "    jnz {available}, {}, {}\n",
            flow.body, flow.done
        ));
        self.start_block(locals, &flow.body);
        flow
    }

    /// Maps expose fresh structural tuples while lists preserve their typed payloads.
    fn iteration_item(
        &mut self,
        flow: &Iteration,
        collection: &str,
        ty: &Type,
        item: &Type,
        locals: &mut Locals,
    ) -> String {
        let index = self.assign(locals, Type::Int, &format!("loadl {}", flow.slot));
        if *ty == Type::Range {
            return index;
        }
        let raw = self.assign(
            locals,
            Type::Int,
            &format!("call $fern_list_get(l {collection}, l {index})"),
        );
        if matches!(ty, Type::Map(_, _)) {
            let key = self.raw_field(&raw, 0, locals);
            let value = self.raw_field(&raw, 8, locals);
            let tuple = self.assign(locals, item.clone(), "call $fern_alloc(l 24)");
            self.store_field(&tuple, 0, "0", locals);
            self.store_field(&tuple, 8, &key, locals);
            self.store_field(&tuple, 16, &value, locals);
            tuple
        } else {
            self.unpack(locals, item, raw)
        }
    }

    /// Test the endpoint before incrementing, so inclusive i64::MAX cannot overflow.
    fn iteration_increment(&mut self, flow: &Iteration, locals: &mut Locals) {
        self.start_block(locals, &flow.next);
        let index = self.assign(locals, Type::Int, &format!("loadl {}", flow.slot));
        let at_end = self.assign(locals, Type::Bool, &format!("ceql {index}, {}", flow.end));
        self.output.push_str(&format!(
            "    jnz {at_end}, {}, {}\n",
            flow.done, flow.increment
        ));
        self.start_block(locals, &flow.increment);
        let next = self.assign(locals, Type::Int, &format!("add {index}, 1"));
        self.output.push_str(&format!(
            "    storel {next}, {}\n    jmp {}\n",
            flow.slot, flow.head
        ));
    }

    /// Materialize index/item tuples using raw payload bits for every element type.
    pub(super) fn enumerate(
        &mut self,
        args: &[Expr],
        span: Span,
        locals: &mut Locals,
        depth: usize,
    ) -> Lowering<(Type, String)> {
        let [arg] = args else {
            return Err(invalid(span, "List.enumerate requires one argument"));
        };
        let Type::List(item) = &arg.ty else {
            return Err(invalid(span, "List.enumerate requires List"));
        };
        let result = Type::List(Box::new(Type::Tuple(vec![Type::Int, *item.clone()])));
        let value = self.expr(arg, locals, depth)?;
        self.enumerate_used = true;
        let value = self.assign(
            locals,
            result.clone(),
            &format!("call $fern_rs_list_enumerate(l {value})"),
        );
        Ok((result, value))
    }
}
