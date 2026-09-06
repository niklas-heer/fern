//! Checked access and allocation boundaries route failures through normal cleanup.
use super::*;

/// The entry wrapper handles only documented successful result payloads.
pub(super) fn main_type(ty: &Type) -> bool {
    matches!(ty, Type::Int | Type::Unit) || matches!(ty, Type::Result(ok, _) if **ok == Type::Unit)
}

impl Emitter<'_> {
    /// Read one validated list element and retain every bit of its concrete payload.
    pub(super) fn list_access(
        &mut self,
        args: &[Expr],
        head: bool,
        span: Span,
        locals: &mut Locals,
        depth: usize,
    ) -> Lowering<(Type, String)> {
        let arity = if head { 1 } else { 2 };
        if args.len() != arity {
            return Err(invalid(
                span,
                "list access argument count differs from signature",
            ));
        }
        let Type::List(item) = &args[0].ty else {
            return Err(invalid(span, "list access requires List"));
        };
        if !head {
            expect_type(args[1].ty.clone(), Type::Int, args[1].span)?;
        }
        let list = self.expr(&args[0], locals, depth)?;
        let index = if head {
            "0".into()
        } else {
            self.expr(&args[1], locals, depth)?
        };
        self.list_access_used = true;
        let raw = self.assign(
            locals,
            Type::Int,
            &format!(
                "call $fern_rs_list_access(l %fault, l {list}, l {index}, w {})",
                u8::from(head)
            ),
        );
        self.guard_fault(locals);
        let value = self.unpack(locals, item, raw);
        Ok((*item.clone(), value))
    }

    /// Evaluate arguments once before the guarded native repetition adapter.
    pub(super) fn repeat_string(
        &mut self,
        args: &[Expr],
        span: Span,
        locals: &mut Locals,
        depth: usize,
    ) -> Lowering<(Type, String)> {
        let [source, count] = args else {
            return Err(invalid(span, "String.repeat requires two arguments"));
        };
        expect_type(source.ty.clone(), Type::String, source.span)?;
        expect_type(count.ty.clone(), Type::Int, count.span)?;
        let source = self.expr(source, locals, depth)?;
        let count = self.expr(count, locals, depth)?;
        self.repeat_used = true;
        let value = self.assign(
            locals,
            Type::String,
            &format!("call $fern_rs_string_repeat(l %fault, l {source}, l {count})"),
        );
        self.guard_fault(locals);
        Ok((Type::String, value))
    }

    /// Report an application Result only after all deferred cleanup and fault checks.
    pub(super) fn result_main_exit(&mut self) {
        const MESSAGE: &str = "fern: main returned Err";
        let bytes = MESSAGE.len() + 1;
        self.data.push_str(&format!(
            "data $fern_rs_main_error = {{ b \"{MESSAGE}\", b 10 }}\n"
        ));
        self.output.push_str(&format!("    %ok =w call $fern_result_is_ok(l %exit)\n    jnz %ok, @ok, @err\n@ok\n    ret 0\n@err\n    call $write(w 2, l $fern_rs_main_error, l {bytes})\n    ret 1\n}}\n"));
    }
}

impl Emitter<'_> {
    /// Validate UTF-8 byte endpoints without allocating or re-evaluating source arguments.
    pub(super) fn slice_string(
        &mut self,
        args: &[Expr],
        span: Span,
        locals: &mut Locals,
        depth: usize,
    ) -> Lowering<(Type, String)> {
        let [source, start, end] = args else {
            return Err(invalid(span, "String.slice requires three arguments"));
        };
        expect_type(source.ty.clone(), Type::String, source.span)?;
        expect_type(start.ty.clone(), Type::Int, start.span)?;
        expect_type(end.ty.clone(), Type::Int, end.span)?;
        let source = self.expr(source, locals, depth)?;
        let start = self.expr(start, locals, depth)?;
        let end = self.expr(end, locals, depth)?;
        self.slice_used = true;
        let value = self.assign(
            locals,
            Type::String,
            &format!("call $fern_rs_string_slice(l %fault, l {source}, l {start}, l {end})"),
        );
        self.guard_fault(locals);
        Ok((Type::String, value))
    }
}

impl Emitter<'_> {
    /// Reject invalid external UTF-8 before splitting can allocate or fail inside C.
    pub(super) fn split_guard(&mut self, arguments: &[String], locals: &mut Locals) {
        let valid = self.assign(
            locals,
            Type::Int,
            &format!("call $fern_str_split_is_valid({})", arguments.join(", ")),
        );
        let resume = locals.label();
        let invalid = locals.label();
        self.output
            .push_str(&format!("    jnz {valid}, {resume}, {invalid}\n"));
        self.start_block(locals, &invalid);
        self.output
            .push_str("    storel 7, %fault\n    storel 0, %return_slot\n    jmp @return\n");
        self.start_block(locals, &resume);
    }
}

impl Emitter<'_> {
    /// Bound decimal text before native classification so a size fault drains Fern defers.
    pub(super) fn decimal_guard(&mut self, arguments: &[String], locals: &mut Locals) {
        let valid = self.assign(
            locals,
            Type::Int,
            &format!(
                "call $fern_str_decimal_size_is_valid({})",
                arguments.join(", ")
            ),
        );
        let resume = locals.label();
        let invalid = locals.label();
        self.output
            .push_str(&format!("    jnz {valid}, {resume}, {invalid}\n"));
        self.start_block(locals, &invalid);
        self.output
            .push_str("    storel 5, %fault\n    storel 0, %return_slot\n    jmp @return\n");
        self.start_block(locals, &resume);
    }
}
