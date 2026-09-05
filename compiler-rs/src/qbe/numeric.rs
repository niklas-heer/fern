//! Numeric operators preserve signed64 wrapping and IEEE double value semantics.
use super::*;

/// Compute an operator result from concrete operand types, independently of source checking.
pub(super) fn binary_type(op: BinaryOp, operand: &Type, span: Span) -> Lowering<Type> {
    use BinaryOp::*;
    let valid = match op {
        Add if *operand == Type::String => return Ok(Type::String),
        Add | Subtract | Multiply | Divide | Power => matches!(operand, Type::Int | Type::Float),
        Remainder | BitAnd | BitOr | BitXor | ShiftLeft | ShiftRight => *operand == Type::Int,
        Lt | Le | Gt | Ge => matches!(operand, Type::Int | Type::Float),
        Eq | Ne => matches!(operand, Type::Int | Type::Float | Type::Bool | Type::String),
        And | Or => {
            return Err(invalid(
                span,
                "logical operation requires short-circuit lowering",
            ))
        }
    };
    if !valid {
        return Err(invalid(span, "operator has incompatible operand type"));
    }
    Ok(if matches!(op, Eq | Ne | Lt | Le | Gt | Ge) {
        Type::Bool
    } else {
        operand.clone()
    })
}

impl Emitter<'_> {
    /// Guard integer domains and normalize shift counts before target-specific instructions.
    pub(super) fn numeric_value(
        &mut self,
        op: BinaryOp,
        operand: &Type,
        result: &Type,
        lhs: &str,
        rhs: &str,
        locals: &mut Locals,
    ) -> String {
        if *operand == Type::Int
            && matches!(op, BinaryOp::Divide | BinaryOp::Remainder | BinaryOp::Power)
        {
            self.numeric_used = true;
            let instruction = if op == BinaryOp::Power {
                format!("call $fern_rs_int_pow(l %fault, l {lhs}, l {rhs})")
            } else {
                format!(
                    "call $fern_rs_int_div(l %fault, l {lhs}, l {rhs}, w {})",
                    u8::from(op == BinaryOp::Remainder)
                )
            };
            let value = self.assign(locals, Type::Int, &instruction);
            self.guard_fault(locals);
            return value;
        }
        if op == BinaryOp::Power {
            return self.assign(locals, Type::Float, &format!("call $pow(d {lhs}, d {rhs})"));
        }
        let rhs = if matches!(op, BinaryOp::ShiftLeft | BinaryOp::ShiftRight) {
            self.assign(locals, Type::Int, &format!("and {rhs}, 63"))
        } else {
            rhs.into()
        };
        let instruction = binary_instruction(op, operand.clone());
        self.assign(
            locals,
            result.clone(),
            &format!("{instruction} {lhs}, {rhs}"),
        )
    }

    /// Share one value-aware contains path between intrinsic and registry call identities.
    pub(super) fn float_contains(
        &mut self,
        args: &[Expr],
        span: Span,
        locals: &mut Locals,
        depth: usize,
    ) -> Lowering<(Type, String)> {
        let [list, value] = args else {
            return Err(invalid(span, "List.contains requires two arguments"));
        };
        expect_type(
            list.ty.clone(),
            Type::List(Box::new(Type::Float)),
            list.span,
        )?;
        expect_type(value.ty.clone(), Type::Float, value.span)?;
        let list = self.expr(list, locals, depth)?;
        let value = self.expr(value, locals, depth)?;
        self.float_contains_used = true;
        let found = self.assign(
            locals,
            Type::Bool,
            &format!("call $fern_rs_list_contains_float(l {list}, d {value})"),
        );
        Ok((Type::Bool, found))
    }
}

/// Detect a Float list only for selecting its otherwise fully checked special ABI.
pub(super) fn float_list(args: &[Expr]) -> bool {
    matches!(args.first().map(|arg| &arg.ty), Some(Type::List(item)) if **item == Type::Float)
}
