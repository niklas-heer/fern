//! Heap closures use a code pointer followed by full-width lexical captures.
use super::*;

impl Emitter<'_> {
    /// Bind checked capture identities from the uniform environment parameter.
    pub(super) fn load_captures(
        &mut self,
        function: &Function,
        locals: &mut Locals,
    ) -> Lowering<()> {
        for (index, capture) in function.captures.iter().enumerate() {
            let address = self.assign(locals, Type::Int, &format!("add %env, {}", 8 * (index + 1)));
            let raw = self.assign(locals, Type::Int, &format!("loadl {address}"));
            let value = self.unpack(locals, &capture.ty, raw);
            locals.define(capture.id.0, capture.ty.clone(), value, function.body.span)?;
        }
        Ok(())
    }

    /// Validate capture arity/types before allocating an escaping lexical environment.
    pub(super) fn closure(
        &mut self,
        id: ir::FunctionId,
        captures: &[Expr],
        span: Span,
        locals: &mut Locals,
        depth: usize,
    ) -> Lowering<(Type, String)> {
        let function = self
            .functions
            .get(&id.0)
            .ok_or_else(|| invalid(span, "unknown closure function identity"))?;
        if captures.len() != function.captures.len() || captures.len() > MAX_NODES {
            return Err(invalid(
                span,
                "closure capture count differs from lifted function",
            ));
        }
        let ty = Type::Function(
            function.params.iter().map(|p| p.ty.clone()).collect(),
            Box::new(function.return_type.clone()),
        );
        let expected: Vec<_> = function.captures.iter().map(|p| p.ty.clone()).collect();
        let mut values = Vec::new();
        for (capture, expected) in captures.iter().zip(expected) {
            expect_type(capture.ty.clone(), expected, capture.span)?;
            let value = self.expr(capture, locals, depth)?;
            values.push(self.payload(locals, &capture.ty, value));
        }
        let object = self.assign(
            locals,
            ty.clone(),
            &format!("call $fern_alloc(l {})", 8 * (values.len() + 1)),
        );
        self.output
            .push_str(&format!("    storel $f{}, {object}\n", id.0));
        for (index, value) in values.iter().enumerate() {
            let address = self.assign(
                locals,
                Type::Int,
                &format!("add {object}, {}", 8 * (index + 1)),
            );
            self.output
                .push_str(&format!("    storel {value}, {address}\n"));
        }
        Ok((ty, object))
    }

    /// Evaluate the callee first and each argument once before indirect invocation.
    pub(super) fn invoke(
        &mut self,
        callee: &Expr,
        args: &[Expr],
        span: Span,
        locals: &mut Locals,
        depth: usize,
    ) -> Lowering<(Type, String)> {
        let Type::Function(params, result) = &callee.ty else {
            return Err(invalid(span, "invocation requires function value"));
        };
        if params.len() != args.len() {
            return Err(invalid(
                span,
                "invocation argument count differs from signature",
            ));
        }
        let closure = self.expr(callee, locals, depth)?;
        let mut values = Vec::new();
        for (arg, param) in args.iter().zip(params) {
            expect_type(arg.ty.clone(), param.clone(), arg.span)?;
            values.push((param.clone(), self.expr(arg, locals, depth)?));
        }
        let value = self.invoke_values(&closure, &values, result, locals);
        Ok((*result.clone(), value))
    }

    /// Invoke an already evaluated, signature-checked callback through its stored code.
    pub(super) fn invoke_values(
        &mut self,
        closure: &str,
        args: &[(Type, String)],
        result: &Type,
        locals: &mut Locals,
    ) -> String {
        let code = self.assign(locals, Type::Int, &format!("loadl {closure}"));
        let mut arguments = vec![format!("l {closure}"), "l %fault".into()];
        arguments.extend(
            args.iter()
                .map(|(ty, value)| format!("{} {value}", self.width(ty.clone()))),
        );
        let instruction = format!("call {code}({})", arguments.join(", "));
        let value = if *result == Type::Unit {
            self.output.push_str(&format!("    {instruction}\n"));
            "0".into()
        } else {
            self.assign(locals, result.clone(), &instruction)
        };
        self.guard_fault(locals);
        value
    }
}
