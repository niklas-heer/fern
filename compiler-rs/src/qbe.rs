//! QBE lowering uses only checked types and resolved symbol identities.
use crate::{
    ast::{BinaryOp, UnaryOp},
    ir::{self, Builtin, CallTarget, Expr, ExprKind, Function, Stmt},
    Diagnostic, Span, Type,
};
use std::collections::{BTreeMap, BTreeSet};

const MAX_DEPTH: usize = 256;
const MAX_NODES: usize = 200_000;
const STRING_RUN: usize = 512;

/// Lower `program` to native-backend IL, rejecting inconsistent public IR.
/// No source-name or AST type inference occurs here. Requires exactly one main.
pub fn emit(program: &ir::Program) -> Result<String, Diagnostic> {
    let mut functions = BTreeMap::new();
    let mut main = None;
    for function in &program.functions {
        if functions.insert(function.id.0, function).is_some() {
            return Err(invalid(function.body.span, "duplicate function identity"));
        }
        if function.name == "main" {
            if main.replace(function).is_some() {
                return Err(invalid(function.body.span, "duplicate main function"));
            }
            if !function.params.is_empty()
                || !matches!(function.return_type, Type::Int | Type::Unit)
            {
                return Err(invalid(
                    function.body.span,
                    "main must take no arguments and return Int or Unit",
                ));
            }
        }
    }
    let main = main.ok_or_else(|| invalid(Span::default(), "missing main function"))?;
    let mut emitter = Emitter {
        functions,
        output: String::new(),
        data: String::new(),
        strings: 0,
        nodes: 0,
    };
    for function in &program.functions {
        emitter.function(function)?;
    }
    emitter.main_wrapper(main);
    emitter.data.push_str(&emitter.output);
    Ok(emitter.data)
}

/// Build an IR-boundary diagnostic at `span`; `message` describes the invariant.
fn invalid(span: Span, message: &str) -> Diagnostic {
    Diagnostic::new(span, format!("invalid typed IR: {message}"))
}

/// Map semantic `ty` to QBE scalar width; pointers and integers remain distinct in IR.
fn width(ty: Type) -> char {
    match ty {
        Type::Int | Type::String => 'l',
        Type::Bool | Type::Unit => 'w',
    }
}

/// Check `actual` against `expected`, retaining the malformed node's source span.
fn expect_type(actual: Type, expected: Type, span: Span) -> Result<(), Diagnostic> {
    if actual != expected {
        return Err(invalid(
            span,
            &format!("expected {expected:?}, found {actual:?}"),
        ));
    }
    Ok(())
}

struct Emitter<'a> {
    functions: BTreeMap<usize, &'a Function>,
    output: String,
    data: String,
    strings: usize,
    nodes: usize,
}

struct Locals {
    values: BTreeMap<usize, (Type, String)>,
    defined: BTreeSet<usize>,
    count: usize,
    temporary: usize,
    label: usize,
    current: String,
}

impl Locals {
    /// Allocate a unique SSA temporary within this function.
    fn temporary(&mut self) -> String {
        let name = format!("%t{}", self.temporary);
        self.temporary += 1;
        name
    }

    /// Reserve a unique block label within this function.
    fn label(&mut self) -> String {
        let name = format!("@b{}", self.label);
        self.label += 1;
        name
    }

    /// Define `id` once within its owning function, with checked type and SSA value.
    fn define(&mut self, id: usize, ty: Type, value: String, span: Span) -> Result<(), Diagnostic> {
        if id >= self.count || !self.defined.insert(id) {
            return Err(invalid(
                span,
                "local identity is out of bounds or defined twice",
            ));
        }
        self.values.insert(id, (ty, value));
        Ok(())
    }
}

impl Emitter<'_> {
    /// Emit `function` using only its resolved signature and typed body.
    fn function(&mut self, function: &Function) -> Result<(), Diagnostic> {
        let mut locals = Locals {
            values: BTreeMap::new(),
            defined: BTreeSet::new(),
            count: function.local_count,
            temporary: 0,
            label: 0,
            current: "@start".into(),
        };
        let mut params = Vec::new();
        for param in &function.params {
            let value = format!("%v{}", param.id.0);
            locals.define(param.id.0, param.ty, value.clone(), function.body.span)?;
            params.push(format!("{} {value}", width(param.ty)));
        }
        self.output.push_str(&format!(
            "function {} $f{}({}) {{\n@start\n",
            width(function.return_type),
            function.id.0,
            params.join(", ")
        ));
        let value = self.expr(&function.body, &mut locals, 0)?;
        if function.return_type != Type::Unit || function.name != "main" {
            expect_type(function.body.ty, function.return_type, function.body.span)?;
        }
        let result = if function.return_type == Type::Unit {
            "0"
        } else {
            &value
        };
        self.output.push_str(&format!("    ret {result}\n}}\n\n"));
        Ok(())
    }

    /// Bridge the C runtime's 32-bit entry ABI to the resolved Fern main signature.
    fn main_wrapper(&mut self, main: &Function) {
        self.output
            .push_str("export function w $fern_main() {\n@start\n");
        self.output.push_str(&format!(
            "    %exit ={} call $f{}()\n",
            width(main.return_type),
            main.id.0
        ));
        if main.return_type == Type::Int {
            self.output
                .push_str("    %status =w copy %exit\n    ret %status\n}\n");
        } else {
            self.output.push_str("    ret 0\n}\n");
        }
    }

    /// Emit `expr` within lexical `locals`, with bounded recursive descent.
    fn expr(
        &mut self,
        expr: &Expr,
        locals: &mut Locals,
        depth: usize,
    ) -> Result<String, Diagnostic> {
        self.nodes += 1;
        if depth > MAX_DEPTH || self.nodes > MAX_NODES {
            return Err(invalid(expr.span, "lowering complexity limit exceeded"));
        }
        let (actual, value) = match &expr.kind {
            ExprKind::Int(value) => (Type::Int, value.to_string()),
            ExprKind::Bool(value) => (Type::Bool, u8::from(*value).to_string()),
            ExprKind::String(value) => (Type::String, self.string(value, expr.span)?),
            ExprKind::Local(id) => locals
                .values
                .get(&id.0)
                .cloned()
                .ok_or_else(|| invalid(expr.span, "unknown or out-of-scope local identity"))?,
            ExprKind::Unary { op, value } => self.unary(*op, value, locals, depth + 1)?,
            ExprKind::Binary { op, left, right } => {
                self.binary(*op, left, right, locals, depth + 1)?
            }
            ExprKind::Call { target, args } => {
                self.call(*target, args, expr.span, locals, depth + 1)?
            }
            ExprKind::If {
                condition,
                then_branch,
                else_branch,
            } => self.conditional(
                condition,
                then_branch,
                else_branch.as_deref(),
                locals,
                depth + 1,
            )?,
            ExprKind::Block(stmts) => self.block(stmts, locals, depth + 1)?,
        };
        expect_type(actual, expr.ty, expr.span)?;
        Ok(value)
    }

    /// Emit UTF-8 `value` as bounded ASCII runs and exact numeric bytes.
    fn string(&mut self, value: &str, span: Span) -> Result<String, Diagnostic> {
        if value.as_bytes().contains(&0) {
            return Err(invalid(
                span,
                "embedded NUL is unsupported by the C string runtime",
            ));
        }
        let name = format!("$str{}", self.strings);
        self.strings += 1;
        self.data.push_str(&format!("data {name} = {{ "));
        let bytes = value.as_bytes();
        let mut index = 0;
        while index < bytes.len() {
            let start = index;
            while index < bytes.len()
                && index - start < STRING_RUN
                && (32..=126).contains(&bytes[index])
                && !matches!(bytes[index], b'"' | b'\\')
            {
                index += 1;
            }
            if index > start {
                self.data
                    .push_str(&format!("b \"{}\", ", &value[start..index]));
            } else {
                self.data.push_str(&format!("b {}, ", bytes[index]));
                index += 1;
            }
        }
        self.data.push_str("b 0 }\n");
        Ok(name)
    }

    /// Emit a typed unary operation on `value`, returning its semantic result type.
    fn unary(
        &mut self,
        op: UnaryOp,
        value: &Expr,
        locals: &mut Locals,
        depth: usize,
    ) -> Result<(Type, String), Diagnostic> {
        let expected = match op {
            UnaryOp::Negate => Type::Int,
            UnaryOp::Not => Type::Bool,
        };
        expect_type(value.ty, expected, value.span)?;
        let value = self.expr(value, locals, depth)?;
        let instruction = match op {
            UnaryOp::Negate => format!("sub 0, {value}"),
            UnaryOp::Not => format!("ceqw {value}, 0"),
        };
        Ok((expected, self.assign(locals, expected, &instruction)))
    }

    /// Emit a scalar instruction `instruction`, assigning a fresh SSA value of `ty`.
    fn assign(&mut self, locals: &mut Locals, ty: Type, instruction: &str) -> String {
        let result = locals.temporary();
        self.output
            .push_str(&format!("    {result} ={} {instruction}\n", width(ty)));
        result
    }

    /// Emit the block `label` and record its actual predecessor identity.
    fn start_block(&mut self, locals: &mut Locals, label: &str) {
        self.output.push_str(&format!("{label}\n"));
        locals.current = label.to_owned();
    }

    /// Emit a final-value block and remove its local bindings at lexical scope exit.
    fn block(
        &mut self,
        stmts: &[Stmt],
        locals: &mut Locals,
        depth: usize,
    ) -> Result<(Type, String), Diagnostic> {
        let mut introduced = Vec::new();
        let mut result = (Type::Unit, "0".into());
        for stmt in stmts {
            match stmt {
                Stmt::Let { id, value } => {
                    let lowered = self.expr(value, locals, depth)?;
                    locals.define(id.0, value.ty, lowered, value.span)?;
                    introduced.push(id.0);
                    result = (Type::Unit, "0".into());
                }
                Stmt::Expr(value) => result = (value.ty, self.expr(value, locals, depth)?),
            }
        }
        for id in introduced {
            locals.values.remove(&id);
        }
        Ok(result)
    }

    /// Emit a resolved call after checking the signature and each argument's type.
    fn call(
        &mut self,
        target: CallTarget,
        args: &[Expr],
        span: Span,
        locals: &mut Locals,
        depth: usize,
    ) -> Result<(Type, String), Diagnostic> {
        let (symbol, params, result) = self.signature(target, args, span)?;
        if args.len() != params.len() {
            return Err(invalid(
                span,
                "call argument count differs from resolved signature",
            ));
        }
        let mut arguments = Vec::new();
        for (arg, expected) in args.iter().zip(params) {
            expect_type(arg.ty, expected, arg.span)?;
            let mut value = self.expr(arg, locals, depth)?;
            let mut abi = width(expected);
            if matches!(
                target,
                CallTarget::Builtin(Builtin::Print | Builtin::Println)
            ) && expected == Type::Bool
            {
                value = self.assign(locals, Type::Int, &format!("extuw {value}"));
                abi = 'l';
            }
            arguments.push(format!("{abi} {value}"));
        }
        let instruction = format!("call ${symbol}({})", arguments.join(", "));
        let value = if result == Type::Unit {
            self.output.push_str(&format!("    {instruction}\n"));
            "0".into()
        } else {
            self.assign(locals, result, &instruction)
        };
        Ok((result, value))
    }

    /// Resolve backend symbols from `target` IDs or builtin identities, never source text.
    fn signature(
        &self,
        target: CallTarget,
        args: &[Expr],
        span: Span,
    ) -> Result<(String, Vec<Type>, Type), Diagnostic> {
        let builtin = match target {
            CallTarget::Function(id) => {
                let function = self
                    .functions
                    .get(&id.0)
                    .ok_or_else(|| invalid(span, "unknown function identity"))?;
                return Ok((
                    format!("f{}", id.0),
                    function.params.iter().map(|p| p.ty).collect(),
                    function.return_type,
                ));
            }
            CallTarget::Builtin(builtin) => builtin,
        };
        let (symbol, params, result) = match builtin {
            Builtin::StringConcat => (
                "fern_str_concat".into(),
                vec![Type::String, Type::String],
                Type::String,
            ),
            Builtin::StringEq => (
                "fern_str_eq".into(),
                vec![Type::String, Type::String],
                Type::Bool,
            ),
            Builtin::StringLen => ("fern_str_len".into(), vec![Type::String], Type::Int),
            Builtin::Print | Builtin::Println => {
                let arg = args
                    .first()
                    .ok_or_else(|| invalid(span, "print requires one argument"))?;
                let suffix = match arg.ty {
                    Type::Int => "int",
                    Type::Bool => "bool",
                    Type::String => "str",
                    Type::Unit => return Err(invalid(arg.span, "cannot print Unit")),
                };
                let name = if builtin == Builtin::Print {
                    "print"
                } else {
                    "println"
                };
                (format!("fern_{name}_{suffix}"), vec![arg.ty], Type::Unit)
            }
        };
        Ok((symbol, params, result))
    }

    /// Emit typed arithmetic/comparisons; logical operators require control flow.
    fn binary(
        &mut self,
        op: BinaryOp,
        left: &Expr,
        right: &Expr,
        locals: &mut Locals,
        depth: usize,
    ) -> Result<(Type, String), Diagnostic> {
        if matches!(op, BinaryOp::And | BinaryOp::Or) {
            return self.logical(op, left, right, locals, depth);
        }
        expect_type(right.ty, left.ty, right.span)?;
        let result_type = match op {
            BinaryOp::Add if left.ty == Type::String => Type::String,
            BinaryOp::Add
            | BinaryOp::Subtract
            | BinaryOp::Multiply
            | BinaryOp::Divide
            | BinaryOp::Remainder => {
                expect_type(left.ty, Type::Int, left.span)?;
                Type::Int
            }
            BinaryOp::Lt | BinaryOp::Le | BinaryOp::Gt | BinaryOp::Ge => {
                expect_type(left.ty, Type::Int, left.span)?;
                Type::Bool
            }
            BinaryOp::Eq | BinaryOp::Ne => {
                if left.ty == Type::Unit {
                    return Err(invalid(left.span, "Unit comparison is unsupported"));
                }
                Type::Bool
            }
            BinaryOp::And | BinaryOp::Or => unreachable!("logical operators handled above"),
        };
        let lhs = self.expr(left, locals, depth)?;
        let rhs = self.expr(right, locals, depth)?;
        if left.ty == Type::String {
            if op == BinaryOp::Add {
                let result = self.assign(
                    locals,
                    Type::String,
                    &format!("call $fern_str_concat(l {lhs}, l {rhs})"),
                );
                return Ok((Type::String, result));
            }
            let result = self.assign(
                locals,
                Type::Bool,
                &format!("call $fern_str_eq(l {lhs}, l {rhs})"),
            );
            return Ok((
                Type::Bool,
                if op == BinaryOp::Ne {
                    self.assign(locals, Type::Bool, &format!("ceqw {result}, 0"))
                } else {
                    result
                },
            ));
        }
        let instruction = binary_instruction(op, left.ty);
        let value = self.assign(locals, result_type, &format!("{instruction} {lhs}, {rhs}"));
        Ok((result_type, value))
    }

    /// Emit true short-circuit evaluation, evaluating `right` only in its RHS block.
    fn logical(
        &mut self,
        op: BinaryOp,
        left: &Expr,
        right: &Expr,
        locals: &mut Locals,
        depth: usize,
    ) -> Result<(Type, String), Diagnostic> {
        expect_type(left.ty, Type::Bool, left.span)?;
        expect_type(right.ty, Type::Bool, right.span)?;
        let lhs = self.expr(left, locals, depth)?;
        let before = locals.current.clone();
        let rhs_label = locals.label();
        let merge = locals.label();
        let (on_true, on_false, shortcut) = if op == BinaryOp::And {
            (&rhs_label, &merge, 0)
        } else {
            (&merge, &rhs_label, 1)
        };
        self.output
            .push_str(&format!("    jnz {lhs}, {on_true}, {on_false}\n"));
        self.start_block(locals, &rhs_label);
        let rhs = self.expr(right, locals, depth)?;
        let rhs_end = locals.current.clone();
        self.output.push_str(&format!("    jmp {merge}\n"));
        self.start_block(locals, &merge);
        let value = self.assign(
            locals,
            Type::Bool,
            &format!("phi {before} {shortcut}, {rhs_end} {rhs}"),
        );
        Ok((Type::Bool, value))
    }

    /// Emit `if` with phi incoming labels captured after nested branch expressions.
    fn conditional(
        &mut self,
        condition: &Expr,
        then_branch: &Expr,
        else_branch: Option<&Expr>,
        locals: &mut Locals,
        depth: usize,
    ) -> Result<(Type, String), Diagnostic> {
        expect_type(condition.ty, Type::Bool, condition.span)?;
        let result_type = if let Some(other) = else_branch {
            expect_type(other.ty, then_branch.ty, other.span)?;
            then_branch.ty
        } else {
            Type::Unit
        };
        let test = self.expr(condition, locals, depth)?;
        let then_label = locals.label();
        let else_label = locals.label();
        let merge = locals.label();
        self.output
            .push_str(&format!("    jnz {test}, {then_label}, {else_label}\n"));
        self.start_block(locals, &then_label);
        let then_value = self.expr(then_branch, locals, depth)?;
        let then_end = locals.current.clone();
        self.output.push_str(&format!("    jmp {merge}\n"));
        self.start_block(locals, &else_label);
        let else_value = if let Some(other) = else_branch {
            self.expr(other, locals, depth)?
        } else {
            "0".into()
        };
        let else_end = locals.current.clone();
        self.output.push_str(&format!("    jmp {merge}\n"));
        self.start_block(locals, &merge);
        let value = if result_type == Type::Unit {
            "0".into()
        } else {
            self.assign(
                locals,
                result_type,
                &format!("phi {then_end} {then_value}, {else_end} {else_value}"),
            )
        };
        Ok((result_type, value))
    }
}

/// Select the scalar opcode from a checked operator and operand type.
fn binary_instruction(op: BinaryOp, operand: Type) -> String {
    let base = match op {
        BinaryOp::Add => "add",
        BinaryOp::Subtract => "sub",
        BinaryOp::Multiply => "mul",
        BinaryOp::Divide => "div",
        BinaryOp::Remainder => "rem",
        BinaryOp::Eq => "ceq",
        BinaryOp::Ne => "cne",
        BinaryOp::Lt => "cslt",
        BinaryOp::Le => "csle",
        BinaryOp::Gt => "csgt",
        BinaryOp::Ge => "csge",
        BinaryOp::And | BinaryOp::Or => unreachable!("logical operators use branch lowering"),
    };
    if matches!(
        op,
        BinaryOp::Eq | BinaryOp::Ne | BinaryOp::Lt | BinaryOp::Le | BinaryOp::Gt | BinaryOp::Ge
    ) {
        format!("{base}{}", width(operand))
    } else {
        base.into()
    }
}
