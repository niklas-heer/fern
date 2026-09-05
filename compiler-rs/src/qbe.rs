//! QBE lowering uses only checked types and resolved symbol identities.
use crate::{
    ast::{BinaryOp, UnaryOp},
    ir::{self, Builtin, CallTarget, Expr, ExprKind, Function, MatchArm, Pattern, Stmt},
    Constructor, Diagnostic, Span, Type,
};
use std::collections::{BTreeMap, BTreeSet, HashMap};
#[path = "qbe/closures.rs"]
mod closures;
#[path = "qbe/control.rs"]
mod control;
#[path = "qbe/higher_order.rs"]
mod higher_order;
#[path = "qbe/iteration.rs"]
mod iteration;
#[path = "qbe/maps.rs"]
mod maps;
#[path = "qbe/nominal.rs"]
mod nominal;
#[path = "qbe/runtime_calls.rs"]
mod runtime_calls;
#[path = "qbe/with.rs"]
mod with;

const MAX_DEPTH: usize = 128;
const MAX_NODES: usize = 200_000;
const STRING_RUN: usize = 512;

/// Lower `program` to native-backend IL, rejecting inconsistent public IR.
/// No source-name or AST type inference occurs here. Requires exactly one main.
pub fn emit(program: &ir::Program) -> Result<String, Diagnostic> {
    emit_inner(program).map_err(|exit| match exit {
        Exit::Diagnostic(error) => error,
        Exit::Terminated => Diagnostic::new(
            Span::default(),
            "invalid typed IR: unhandled control termination",
        ),
    })
}

/// Termination travels separately from diagnostics and never creates a usable operand.
#[derive(Debug)]
enum Exit {
    Diagnostic(Diagnostic),
    Terminated,
}
type Lowering<T> = Result<T, Exit>;
impl From<Diagnostic> for Exit {
    fn from(error: Diagnostic) -> Self {
        Self::Diagnostic(error)
    }
}

/// Validate signatures and lower complete function bodies through their exit handlers.
fn emit_inner(program: &ir::Program) -> Lowering<String> {
    let layouts = nominal::layouts(&program.types)?;
    let mut functions = BTreeMap::new();
    let mut main = None;
    for function in &program.functions {
        if function.params.len() > MAX_NODES
            || function.captures.len() > MAX_NODES.saturating_sub(function.params.len())
        {
            return Err(invalid(
                function.body.span,
                "function signature limit exceeded",
            ));
        }
        nominal::resolved(&function.return_type, &layouts, function.body.span, 0)?;
        for param in function.params.iter().chain(&function.captures) {
            nominal::resolved(&param.ty, &layouts, function.body.span, 0)?;
        }
        if functions.insert(function.id.0, function).is_some() {
            return Err(invalid(function.body.span, "duplicate function identity"));
        }
        if function.name == "main" {
            if main.replace(function).is_some() {
                return Err(invalid(function.body.span, "duplicate main function"));
            }
            if !function.captures.is_empty()
                || !function.params.is_empty()
                || !matches!(function.return_type.clone(), Type::Int | Type::Unit)
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
        layouts,
        output: String::new(),
        data: String::new(),
        strings: 0,
        nodes: 0,
        maps_used: false,
        enumerate_used: false,
    };
    for function in &program.functions {
        emitter.function(function)?;
    }
    emitter.output.push_str(include_str!("qbe/control.ssa"));
    if emitter.enumerate_used {
        emitter.output.push_str(include_str!("qbe/iteration.ssa"));
    }
    emitter.main_wrapper(main);
    emitter.float_print_helpers();
    if emitter.maps_used {
        emitter.output.push_str(include_str!("qbe/maps.ssa"));
    }
    emitter.data.push_str(&emitter.output);
    Ok(emitter.data)
}

/// Build an IR-boundary diagnostic at `span`; `message` describes the invariant.
fn invalid(span: Span, message: &str) -> Exit {
    Exit::Diagnostic(Diagnostic::new(
        span,
        format!("invalid typed IR: {message}"),
    ))
}

/// Map semantic `ty` to QBE scalar width; pointers and integers remain distinct in IR.
fn width(ty: Type) -> char {
    match ty {
        Type::Range
        | Type::Int
        | Type::String
        | Type::Map(_, _)
        | Type::List(_)
        | Type::Option(_)
        | Type::Result(_, _)
        | Type::Tuple(_)
        | Type::Native(_)
        | Type::Named(_, _)
        | Type::Function(_, _) => 'l',
        Type::Bool | Type::Unit => 'w',
        Type::Float => 'd',
        Type::Never | Type::Infer(_) | Type::Generic(_) => {
            unreachable!("concrete types validated at IR boundary")
        }
    }
}

/// Check `actual` against `expected`, retaining the malformed node's source span.
fn expect_type(actual: Type, expected: Type, span: Span) -> Lowering<()> {
    if actual != Type::Never && actual != expected {
        return Err(invalid(
            span,
            &format!("expected {expected:?}, found {actual:?}"),
        ));
    }
    Ok(())
}

struct Emitter<'a> {
    functions: BTreeMap<usize, &'a Function>,
    layouts: HashMap<Type, &'a ir::TypeLayout>,
    output: String,
    data: String,
    strings: usize,
    nodes: usize,
    maps_used: bool,
    enumerate_used: bool,
}

struct Locals {
    values: BTreeMap<usize, (Type, String)>,
    defined: BTreeSet<usize>,
    count: usize,
    return_type: Type,
    loops: Vec<iteration::LoopTargets>,
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
    fn define(&mut self, id: usize, ty: Type, value: String, span: Span) -> Lowering<()> {
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
    fn function(&mut self, function: &Function) -> Lowering<()> {
        let mut locals = Locals {
            values: BTreeMap::new(),
            defined: BTreeSet::new(),
            count: function.local_count,
            return_type: function.return_type.clone(),
            loops: vec![],
            temporary: 0,
            label: 0,
            current: "@start".into(),
        };
        let mut params = vec!["l %env".to_owned()];
        for param in &function.params {
            let value = format!("%v{}", param.id.0);
            locals.define(
                param.id.0,
                param.ty.clone(),
                value.clone(),
                function.body.span,
            )?;
            params.push(format!("{} {value}", width(param.ty.clone())));
        }
        self.output.push_str(&format!(
            "function {} $f{}({}) {{\n@start\n",
            width(function.return_type.clone()),
            function.id.0,
            params.join(", ")
        ));
        self.load_captures(function, &mut locals)?;
        self.output.push_str("    %return_slot =l alloc8 8\n    %defer_head =l alloc8 8\n    storel 0, %defer_head\n");
        if function.body.ty != Type::Never
            && (function.return_type != Type::Unit || function.name != "main")
        {
            expect_type(
                function.body.ty.clone(),
                function.return_type.clone(),
                function.body.span,
            )?;
        }
        match self.expr(&function.body, &mut locals, 0) {
            Ok(value) => self.save_return(&value, &mut locals),
            Err(Exit::Terminated) => {}
            Err(error) => return Err(error),
        }
        self.finish_function(&mut locals);
        Ok(())
    }

    /// Bridge the C runtime's 32-bit entry ABI to the resolved Fern main signature.
    fn main_wrapper(&mut self, main: &Function) {
        self.output
            .push_str("export function w $fern_main() {\n@start\n");
        self.output.push_str(&format!(
            "    %exit ={} call $f{}(l 0)\n",
            width(main.return_type.clone()),
            main.id.0
        ));
        if main.return_type.clone() == Type::Int {
            self.output
                .push_str("    %status =w copy %exit\n    ret %status\n}\n");
        } else {
            self.output.push_str("    ret 0\n}\n");
        }
    }

    /// Emit `expr` within lexical `locals`, with bounded recursive descent.
    fn expr(&mut self, expr: &Expr, locals: &mut Locals, depth: usize) -> Lowering<String> {
        self.validate_expr(expr, depth)?;
        self.strict_termination(expr, locals, depth + 1)?;
        let (actual, value) = match &expr.kind {
            ExprKind::With { .. }
            | ExprKind::For { .. }
            | ExprKind::Range { .. }
            | ExprKind::Break
            | ExprKind::Continue
            | ExprKind::Return(_)
            | ExprKind::Defer(_)
            | ExprKind::Match { .. }
            | ExprKind::If { .. } => self.flow_expr(expr, locals, depth + 1)?,
            ExprKind::Lambda { .. } | ExprKind::FunctionValue { .. } => {
                return Err(invalid(expr.span, "unlifted callable expression"));
            }
            ExprKind::Closure { function, captures } => {
                self.closure(*function, captures, expr.span, locals, depth + 1)?
            }
            ExprKind::Invoke { callee, args } => {
                self.invoke(callee, args, expr.span, locals, depth + 1)?
            }
            ExprKind::CustomConstruct { tag, fields } => {
                self.custom_construct(*tag, fields, &expr.ty, expr.span, locals, depth + 1)?
            }
            ExprKind::Field { value, index } => {
                self.record_field(value, *index, locals, depth + 1)?
            }
            ExprKind::Try(value) => self.attempt(value, locals, depth + 1)?,
            ExprKind::Interpolate(parts) => self.interpolate(parts, locals, depth + 1)?,
            ExprKind::Unit => (Type::Unit, "0".into()),
            ExprKind::Tuple(items) => self.tuple(items, &expr.ty, expr.span, locals, depth + 1)?,
            ExprKind::Map(entries) => {
                self.map_literal(entries, &expr.ty, expr.span, locals, depth + 1)?
            }
            ExprKind::List(items) => self.list(items, &expr.ty, expr.span, locals, depth + 1)?,
            ExprKind::Construct { constructor, value } => self.construct(
                *constructor,
                value.as_deref(),
                &expr.ty,
                expr.span,
                locals,
                depth + 1,
            )?,
            ExprKind::Int(value) => (Type::Int, value.to_string()),
            ExprKind::Float(value) => self.float_literal(*value, locals),
            ExprKind::Bool(value) => (Type::Bool, u8::from(*value).to_string()),
            ExprKind::String(value) => (Type::String, self.string(value, expr.span)?),
            ExprKind::Local(id) => Self::local(*id, expr.span, locals)?,
            ExprKind::Unary { op, value } => self.unary(*op, value, locals, depth + 1)?,
            ExprKind::Binary { op, left, right } => {
                self.binary(*op, left, right, locals, depth + 1)?
            }
            ExprKind::Call { target, args } => {
                self.call(*target, args, &expr.ty, expr.span, locals, depth + 1)?
            }
            ExprKind::Block(stmts) => self.block(stmts, locals, depth + 1)?,
        };
        expect_type(actual, expr.ty.clone(), expr.span)?;
        Ok(value)
    }

    /// Lower structured control independently from scalar and aggregate value dispatch.
    fn flow_expr(
        &mut self,
        expr: &Expr,
        locals: &mut Locals,
        depth: usize,
    ) -> Lowering<(Type, String)> {
        match &expr.kind {
            ExprKind::With {
                steps,
                body,
                handlers,
            } => self.with(steps, body, handlers, locals, depth),
            ExprKind::For {
                pattern,
                iterable,
                body,
            } => self.iteration(pattern, iterable, body, locals, depth),
            ExprKind::Range {
                start,
                end,
                inclusive,
            } => self.range(start, end, *inclusive, locals, depth),
            ExprKind::Break => self.loop_exit(false, expr.span, locals),
            ExprKind::Continue => self.loop_exit(true, expr.span, locals),
            ExprKind::Return(value) => self.returned(value, locals, depth),
            ExprKind::Defer(value) => self.defer(value, locals, depth),
            ExprKind::Match { value, arms } => self.matching(value, arms, locals, depth),
            ExprKind::If {
                condition,
                then_branch,
                else_branch,
            } => self.conditional(
                condition,
                then_branch,
                else_branch.as_deref(),
                locals,
                depth,
            ),
            _ => Err(invalid(expr.span, "expected control expression")),
        }
    }

    /// Validate a node's concrete type and bound recursive lowering before emitting it.
    fn validate_expr(&mut self, expr: &Expr, depth: usize) -> Lowering<()> {
        if matches!(
            expr.kind,
            ExprKind::Return(_) | ExprKind::Break | ExprKind::Continue
        ) && expr.ty != Type::Never
        {
            return Err(invalid(
                expr.span,
                "control exit expression requires Never type",
            ));
        }
        if expr.ty != Type::Never {
            nominal::resolved(&expr.ty, &self.layouts, expr.span, 0)?;
        }
        self.nodes += 1;
        if depth > MAX_DEPTH || self.nodes > MAX_NODES {
            return Err(invalid(expr.span, "lowering complexity limit exceeded"));
        }
        Ok(())
    }

    /// Resolve a lexical identity without reconstructing source names or types.
    fn local(id: ir::LocalId, span: Span, locals: &Locals) -> Lowering<(Type, String)> {
        locals
            .values
            .get(&id.0)
            .cloned()
            .ok_or_else(|| invalid(span, "unknown or out-of-scope local identity"))
    }

    /// Validate structural tuple shape before allocating its tag and typed fields.
    fn tuple(
        &mut self,
        items: &[Expr],
        ty: &Type,
        span: Span,
        locals: &mut Locals,
        depth: usize,
    ) -> Lowering<(Type, String)> {
        if !matches!(ty, Type::Tuple(fields) if !fields.is_empty()) {
            return Err(invalid(
                span,
                "tuple expression requires a nonempty structural tuple type",
            ));
        }
        self.custom_construct(0, items, ty, span, locals, depth)
    }

    /// Preserve every literal Float bit without a locale-dependent decimal conversion.
    fn float_literal(&mut self, value: f64, locals: &mut Locals) -> (Type, String) {
        (
            Type::Float,
            self.assign(
                locals,
                Type::Float,
                &format!("cast {}", value.to_bits() as i64),
            ),
        )
    }

    /// Emit UTF-8 `value` as bounded ASCII runs and exact numeric bytes.
    fn string(&mut self, value: &str, span: Span) -> Lowering<String> {
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
    ) -> Lowering<(Type, String)> {
        let expected = match op {
            UnaryOp::Negate if value.ty == Type::Float => Type::Float,
            UnaryOp::Negate => Type::Int,
            UnaryOp::Not => Type::Bool,
        };
        expect_type(value.ty.clone(), expected.clone(), value.span)?;
        let value = self.expr(value, locals, depth)?;
        let instruction = match op {
            UnaryOp::Negate if expected == Type::Float => format!("neg {value}"),
            UnaryOp::Negate => format!("sub 0, {value}"),
            UnaryOp::Not => format!("ceqw {value}, 0"),
        };
        Ok((
            expected.clone(),
            self.assign(locals, expected, &instruction),
        ))
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
    ) -> Lowering<(Type, String)> {
        let outer = locals.values.clone();
        let result = self.block_statements(stmts, locals, depth);
        locals.values = outer;
        result
    }

    /// Evaluate a block's statements until normal completion or a propagated exit.
    fn block_statements(
        &mut self,
        stmts: &[Stmt],
        locals: &mut Locals,
        depth: usize,
    ) -> Lowering<(Type, String)> {
        let mut result = (Type::Unit, "0".into());
        for stmt in stmts {
            match stmt {
                Stmt::Let { id, value } => {
                    let lowered = self.expr(value, locals, depth)?;
                    locals.define(id.0, value.ty.clone(), lowered, value.span)?;
                    result = (Type::Unit, "0".into());
                }
                Stmt::LetElse {
                    pattern,
                    value,
                    else_branch,
                } => {
                    self.let_else(pattern, value, else_branch, locals, depth)?;
                    result = (Type::Unit, "0".into());
                }
                Stmt::Expr(value) => result = (value.ty.clone(), self.expr(value, locals, depth)?),
            }
        }
        Ok(result)
    }

    /// Emit a resolved call after checking the signature and each argument's type.
    fn call(
        &mut self,
        target: CallTarget,
        args: &[Expr],
        result: &Type,
        span: Span,
        locals: &mut Locals,
        depth: usize,
    ) -> Lowering<(Type, String)> {
        if target == CallTarget::Builtin(Builtin::ListEnumerate) {
            return self.enumerate(args, span, locals, depth);
        }
        if let CallTarget::Runtime(id) = target {
            return self.runtime_call(id, args, span, locals, depth);
        }
        if let CallTarget::Builtin(builtin) = target {
            if maps::is_map(builtin) {
                return self.map_call(builtin, args, result, span, locals, depth);
            }
            if higher_order::is_higher_order(builtin) {
                return self.higher_order(builtin, args, span, locals, depth);
            }
        }
        if compound_builtin(target) {
            return self.compound_call(target, args, span, locals, depth);
        }
        let (symbol, params, result) = self.signature(target, args, span)?;
        if args.len() != params.len() {
            return Err(invalid(
                span,
                "call argument count differs from resolved signature",
            ));
        }
        let mut arguments = if matches!(target, CallTarget::Function(_)) {
            vec!["l 0".into()]
        } else {
            Vec::new()
        };
        for (arg, expected) in args.iter().zip(params) {
            expect_type(arg.ty.clone(), expected.clone(), arg.span)?;
            let mut value = self.expr(arg, locals, depth)?;
            let mut abi = width(expected.clone());
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
            self.assign(locals, result.clone(), &instruction)
        };
        Ok((result, value))
    }

    /// Resolve backend symbols from `target` IDs or builtin identities, never source text.
    fn signature(
        &self,
        target: CallTarget,
        args: &[Expr],
        span: Span,
    ) -> Lowering<(String, Vec<Type>, Type)> {
        let builtin = match target {
            CallTarget::Runtime(_) => {
                return Err(invalid(span, "runtime call requires registry lowering"))
            }
            CallTarget::Function(id) => {
                let function = self
                    .functions
                    .get(&id.0)
                    .ok_or_else(|| invalid(span, "unknown function identity"))?;
                if !function.captures.is_empty() {
                    return Err(invalid(
                        span,
                        "direct call cannot supply captured environment",
                    ));
                }
                return Ok((
                    format!("f{}", id.0),
                    function.params.iter().map(|p| p.ty.clone()).collect(),
                    function.return_type.clone(),
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
                let suffix = match arg.ty.clone() {
                    Type::Int => "int",
                    Type::Float => "float",
                    Type::Bool => "bool",
                    Type::String => "str",
                    _ => return Err(invalid(arg.span, "cannot print compound or Unit value")),
                };
                let name = if builtin == Builtin::Print {
                    "print"
                } else {
                    "println"
                };
                (
                    format!("fern_{name}_{suffix}"),
                    vec![arg.ty.clone()],
                    Type::Unit,
                )
            }
            _ => return compound_signature(builtin, args, span),
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
    ) -> Lowering<(Type, String)> {
        if matches!(op, BinaryOp::And | BinaryOp::Or) {
            return self.logical(op, left, right, locals, depth);
        }
        expect_type(right.ty.clone(), left.ty.clone(), right.span)?;
        let result_type = match op {
            BinaryOp::Add if left.ty.clone() == Type::String => Type::String,
            BinaryOp::Add
            | BinaryOp::Subtract
            | BinaryOp::Multiply
            | BinaryOp::Divide
            | BinaryOp::Remainder => {
                if !matches!(left.ty, Type::Int | Type::Float)
                    || (op == BinaryOp::Remainder && left.ty == Type::Float)
                {
                    return Err(invalid(
                        left.span,
                        "numeric operator requires matching numeric types",
                    ));
                }
                left.ty.clone()
            }
            BinaryOp::Lt | BinaryOp::Le | BinaryOp::Gt | BinaryOp::Ge => {
                if !matches!(left.ty, Type::Int | Type::Float) {
                    return Err(invalid(
                        left.span,
                        "ordered comparison requires numeric types",
                    ));
                }
                Type::Bool
            }
            BinaryOp::Eq | BinaryOp::Ne => {
                if !matches!(left.ty, Type::Int | Type::Float | Type::Bool | Type::String) {
                    return Err(invalid(
                        left.span,
                        "comparison requires Int, Bool, or String",
                    ));
                }
                Type::Bool
            }
            BinaryOp::And | BinaryOp::Or => unreachable!("logical operators handled above"),
        };
        let lhs = self.expr(left, locals, depth)?;
        let rhs = self.expr(right, locals, depth)?;
        if left.ty == Type::String {
            return Ok(self.string_binary(op, &lhs, &rhs, locals));
        }
        let instruction = binary_instruction(op, left.ty.clone());
        let value = self.assign(
            locals,
            result_type.clone(),
            &format!("{instruction} {lhs}, {rhs}"),
        );
        Ok((result_type, value))
    }

    /// Emit the three checked String operators through content-aware runtime helpers.
    fn string_binary(
        &mut self,
        op: BinaryOp,
        lhs: &str,
        rhs: &str,
        locals: &mut Locals,
    ) -> (Type, String) {
        if op == BinaryOp::Add {
            let value = self.assign(
                locals,
                Type::String,
                &format!("call $fern_str_concat(l {lhs}, l {rhs})"),
            );
            return (Type::String, value);
        }
        let value = self.assign(
            locals,
            Type::Bool,
            &format!("call $fern_str_eq(l {lhs}, l {rhs})"),
        );
        let value = if op == BinaryOp::Ne {
            self.assign(locals, Type::Bool, &format!("ceqw {value}, 0"))
        } else {
            value
        };
        (Type::Bool, value)
    }

    /// Emit true short-circuit evaluation, evaluating `right` only in its RHS block.
    fn logical(
        &mut self,
        op: BinaryOp,
        left: &Expr,
        right: &Expr,
        locals: &mut Locals,
        depth: usize,
    ) -> Lowering<(Type, String)> {
        expect_type(left.ty.clone(), Type::Bool, left.span)?;
        expect_type(right.ty.clone(), Type::Bool, right.span)?;
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
        let mut incoming = vec![(before, shortcut.to_string())];
        let rhs = self.expr(right, locals, depth);
        self.incoming(rhs, &mut incoming, &merge, locals)?;
        self.join(Type::Bool, incoming, &merge, locals)
    }

    /// Emit `if` with phi incoming labels captured after nested branch expressions.
    fn conditional(
        &mut self,
        condition: &Expr,
        then_branch: &Expr,
        else_branch: Option<&Expr>,
        locals: &mut Locals,
        depth: usize,
    ) -> Lowering<(Type, String)> {
        expect_type(condition.ty.clone(), Type::Bool, condition.span)?;
        let result_type = if let Some(other) = else_branch {
            control::joined(&then_branch.ty, &other.ty, other.span)?
        } else {
            Type::Unit
        };
        let test = self.expr(condition, locals, depth)?;
        let then_label = locals.label();
        let else_label = locals.label();
        let merge = locals.label();
        let mut incoming = Vec::new();
        self.output
            .push_str(&format!("    jnz {test}, {then_label}, {else_label}\n"));
        self.start_block(locals, &then_label);
        let then_value = self.expr(then_branch, locals, depth);
        self.incoming(then_value, &mut incoming, &merge, locals)?;
        self.start_block(locals, &else_label);
        let else_value = if let Some(other) = else_branch {
            self.expr(other, locals, depth)
        } else {
            Ok("0".into())
        };
        self.incoming(else_value, &mut incoming, &merge, locals)?;
        self.join(result_type, incoming, &merge, locals)
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
    let base = if operand == Type::Float {
        base.replace("cs", "c")
    } else {
        base.to_owned()
    };
    if matches!(
        op,
        BinaryOp::Eq | BinaryOp::Ne | BinaryOp::Lt | BinaryOp::Le | BinaryOp::Gt | BinaryOp::Ge
    ) {
        format!("{base}{}", width(operand))
    } else {
        base
    }
}

/// Reject unresolved or excessively nested types before choosing any ABI layout.
fn concrete(ty: &Type, span: Span, depth: usize) -> Lowering<()> {
    if depth > MAX_DEPTH {
        return Err(invalid(span, "type nesting limit exceeded"));
    }
    match ty {
        Type::Never | Type::Infer(_) | Type::Generic(_) => Err(invalid(
            span,
            "unresolved inference variable or generic type",
        )),
        Type::Function(args, result) => {
            for arg in args {
                concrete(arg, span, depth + 1)?;
            }
            concrete(result, span, depth + 1)
        }
        Type::Tuple(args) | Type::Named(_, args) => {
            for arg in args {
                concrete(arg, span, depth + 1)?;
            }
            Ok(())
        }
        Type::List(item) | Type::Option(item) => concrete(item, span, depth + 1),
        Type::Result(ok, err) | Type::Map(ok, err) => {
            concrete(ok, span, depth + 1)?;
            concrete(err, span, depth + 1)
        }
        Type::Range
        | Type::Native(_)
        | Type::Int
        | Type::Float
        | Type::Bool
        | Type::String
        | Type::Unit => Ok(()),
    }
}

/// Determine the payload type for a constructor, rejecting tags from another sum type.
fn payload_type(constructor: Constructor, ty: &Type, span: Span) -> Lowering<Option<&Type>> {
    match (constructor, ty) {
        (Constructor::Some, Type::Option(item)) => Ok(Some(item)),
        (Constructor::None, Type::Option(_)) => Ok(None),
        (Constructor::Ok, Type::Result(ok, _)) => Ok(Some(ok)),
        (Constructor::Err, Type::Result(_, err)) => Ok(Some(err)),
        _ => Err(invalid(
            span,
            "constructor does not belong to the checked sum type",
        )),
    }
}

/// Identify builtins whose C runtime ABI transports every scalar as a full word.
fn compound_builtin(target: CallTarget) -> bool {
    matches!(target, CallTarget::Builtin(builtin) if !matches!(builtin,
        Builtin::Print | Builtin::Println | Builtin::StringConcat | Builtin::StringEq | Builtin::StringLen))
}

/// Resolve generic runtime operations against the first argument's concrete type.
fn compound_signature(
    builtin: Builtin,
    args: &[Expr],
    span: Span,
) -> Lowering<(String, Vec<Type>, Type)> {
    let first = args
        .first()
        .ok_or_else(|| invalid(span, "builtin requires an argument"))?;
    match builtin {
        Builtin::ListLen
        | Builtin::ListGet
        | Builtin::ListHead
        | Builtin::ListTail
        | Builtin::ListIsEmpty
        | Builtin::ListPush
        | Builtin::ListReverse
        | Builtin::ListConcat
        | Builtin::ListContains => list_signature(builtin, &first.ty, span),
        Builtin::OptionIsSome | Builtin::OptionIsNone | Builtin::OptionUnwrapOr => {
            let Type::Option(item) = &first.ty else {
                return Err(invalid(span, "Option builtin requires Option"));
            };
            sum_signature(builtin, &first.ty, item)
        }
        Builtin::ResultIsOk | Builtin::ResultIsErr | Builtin::ResultUnwrapOr => {
            let Type::Result(ok, _) = &first.ty else {
                return Err(invalid(span, "Result builtin requires Result"));
            };
            sum_signature(builtin, &first.ty, ok)
        }
        _ => Err(invalid(span, "expected a compound builtin identity")),
    }
}

/// Derive list ABI symbols and semantic signatures, avoiding compound pointer equality.
fn list_signature(builtin: Builtin, ty: &Type, span: Span) -> Lowering<(String, Vec<Type>, Type)> {
    let Type::List(item) = ty else {
        return Err(invalid(span, "List builtin requires List"));
    };
    let mut params = vec![ty.clone()];
    let (symbol, result) = match builtin {
        Builtin::ListLen => ("fern_list_len", Type::Int),
        Builtin::ListIsEmpty => ("fern_list_is_empty", Type::Bool),
        Builtin::ListHead => ("fern_list_head", *item.clone()),
        Builtin::ListGet => {
            params.push(Type::Int);
            ("fern_list_get", *item.clone())
        }
        Builtin::ListTail => ("fern_list_tail", ty.clone()),
        Builtin::ListReverse => ("fern_list_reverse", ty.clone()),
        Builtin::ListPush => {
            params.push(*item.clone());
            ("fern_list_push", ty.clone())
        }
        Builtin::ListConcat => {
            params.push(ty.clone());
            ("fern_list_concat", ty.clone())
        }
        Builtin::ListContains => {
            if !matches!(**item, Type::Int | Type::Bool | Type::String) {
                return Err(invalid(
                    span,
                    "List contains requires Int, Bool, or String elements",
                ));
            }
            params.push(*item.clone());
            (
                if **item == Type::String {
                    "fern_list_contains_str"
                } else {
                    "fern_list_contains"
                },
                Type::Bool,
            )
        }
        _ => return Err(invalid(span, "expected a List builtin identity")),
    };
    Ok((symbol.into(), params, result))
}

/// Use heap Result helpers for both built-in sum types, with eager default arguments.
fn sum_signature(
    builtin: Builtin,
    ty: &Type,
    payload: &Type,
) -> Lowering<(String, Vec<Type>, Type)> {
    if matches!(builtin, Builtin::OptionUnwrapOr | Builtin::ResultUnwrapOr) {
        Ok((
            "fern_result_unwrap_or".into(),
            vec![ty.clone(), payload.clone()],
            payload.clone(),
        ))
    } else {
        Ok(("fern_result_is_ok".into(), vec![ty.clone()], Type::Bool))
    }
}

impl Emitter<'_> {
    /// Widen Boolean/Unit values before storing them in runtime payload slots.
    fn payload(&mut self, locals: &mut Locals, ty: &Type, value: String) -> String {
        if matches!(ty, Type::Bool | Type::Unit) {
            self.assign(locals, Type::Int, &format!("extuw {value}"))
        } else if *ty == Type::Float {
            self.assign(locals, Type::Int, &format!("cast {value}"))
        } else {
            value
        }
    }

    /// Narrow payload loads to the checked scalar width without changing pointer values.
    fn unpack(&mut self, locals: &mut Locals, ty: &Type, value: String) -> String {
        if matches!(ty, Type::Bool | Type::Unit) {
            self.assign(locals, ty.clone(), &format!("copy {value}"))
        } else if *ty == Type::Float {
            self.assign(locals, Type::Float, &format!("cast {value}"))
        } else {
            value
        }
    }

    /// Allocate a private list and populate it once before exposing its immutable value.
    fn list(
        &mut self,
        items: &[Expr],
        ty: &Type,
        span: Span,
        locals: &mut Locals,
        depth: usize,
    ) -> Lowering<(Type, String)> {
        let Type::List(item_type) = ty else {
            return Err(invalid(span, "list literal requires List type"));
        };
        let list = self.assign(
            locals,
            ty.clone(),
            &format!("call $fern_list_with_capacity(l {})", items.len().max(1)),
        );
        for item in items {
            expect_type(item.ty.clone(), *item_type.clone(), item.span)?;
            let value = self.expr(item, locals, depth)?;
            let value = self.payload(locals, &item.ty, value);
            self.output.push_str(&format!(
                "    call $fern_list_push_mut(l {list}, l {value})\n"
            ));
        }
        Ok((ty.clone(), list))
    }

    /// Construct heap-backed Options/Results using the checked payload, including full i64s.
    fn construct(
        &mut self,
        constructor: Constructor,
        value: Option<&Expr>,
        ty: &Type,
        span: Span,
        locals: &mut Locals,
        depth: usize,
    ) -> Lowering<(Type, String)> {
        let expected = payload_type(constructor, ty, span)?;
        let payload = match (value, expected) {
            (Some(value), Some(expected)) => {
                expect_type(value.ty.clone(), expected.clone(), value.span)?;
                let lowered = self.expr(value, locals, depth)?;
                self.payload(locals, expected, lowered)
            }
            (None, None) => "0".into(),
            _ => {
                return Err(invalid(
                    span,
                    "constructor payload presence differs from its signature",
                ))
            }
        };
        let symbol = if matches!(constructor, Constructor::Some | Constructor::Ok) {
            "fern_result_ok"
        } else {
            "fern_result_err"
        };
        Ok((
            ty.clone(),
            self.assign(locals, ty.clone(), &format!("call ${symbol}(l {payload})")),
        ))
    }

    /// Emit a runtime call with explicit full-width transport and typed result conversion.
    fn compound_call(
        &mut self,
        target: CallTarget,
        args: &[Expr],
        span: Span,
        locals: &mut Locals,
        depth: usize,
    ) -> Lowering<(Type, String)> {
        let (symbol, params, result) = self.signature(target, args, span)?;
        if args.len() != params.len() {
            return Err(invalid(
                span,
                "builtin argument count differs from signature",
            ));
        }
        let mut arguments = Vec::new();
        for (arg, expected) in args.iter().zip(params) {
            expect_type(arg.ty.clone(), expected.clone(), arg.span)?;
            let value = self.expr(arg, locals, depth)?;
            let payload = self.payload(locals, &arg.ty, value);
            arguments.push(format!("l {payload}"));
        }
        let raw = self.assign(
            locals,
            Type::Int,
            &format!("call ${symbol}({})", arguments.join(", ")),
        );
        let mut value = self.unpack(locals, &result, raw);
        if matches!(
            target,
            CallTarget::Builtin(Builtin::OptionIsNone | Builtin::ResultIsErr)
        ) {
            value = self.assign(locals, Type::Bool, &format!("ceqw {value}, 0"));
        }
        Ok((result, value))
    }
}

impl Emitter<'_> {
    /// Propagate Err unchanged, then expose an Ok payload in the continuing block.
    fn attempt(
        &mut self,
        value: &Expr,
        locals: &mut Locals,
        depth: usize,
    ) -> Lowering<(Type, String)> {
        let Type::Result(ok, error) = &value.ty else {
            return Err(invalid(value.span, "? requires Result operand"));
        };
        let Type::Result(_, returned_error) = &locals.return_type else {
            return Err(invalid(
                value.span,
                "? requires enclosing Result return type",
            ));
        };
        expect_type(*error.clone(), *returned_error.clone(), value.span)?;
        let result = self.expr(value, locals, depth)?;
        let tag = self.assign(
            locals,
            Type::Bool,
            &format!("call $fern_result_is_ok(l {result})"),
        );
        let success = locals.label();
        let failure = locals.label();
        self.output
            .push_str(&format!("    jnz {tag}, {success}, {failure}\n"));
        self.start_block(locals, &failure);
        self.save_return(&result, locals);
        self.start_block(locals, &success);
        let payload = self.assign(
            locals,
            Type::Int,
            &format!("call $fern_result_unwrap(l {result})"),
        );
        let value = self.unpack(locals, ok, payload);
        Ok((*ok.clone(), value))
    }
}

impl Emitter<'_> {
    /// Print doubles with round-trip precision through the system variadic C ABI.
    fn float_print_helpers(&mut self) {
        if !self.output.contains("call $fern_print_float")
            && !self.output.contains("call $fern_println_float")
        {
            return;
        }
        self.data
            .push_str("data $fern_rs_float_fmt = { b \"%.17g\", b 0 }\n");
        self.data
            .push_str("data $fern_rs_float_line_fmt = { b \"%.17g\", b 10, b 0 }\n");
        for (name, format) in [
            ("fern_print_float", "fern_rs_float_fmt"),
            ("fern_println_float", "fern_rs_float_line_fmt"),
        ] {
            self.output.push_str(&format!("function ${name}(d %x) {{\n@start\n    call $printf(l ${format}, ..., d %x)\n    ret\n}}\n"));
        }
    }
}

impl Emitter<'_> {
    /// Convert scalar parts once and concatenate in source order using the runtime ABI.
    fn interpolate(
        &mut self,
        parts: &[Expr],
        locals: &mut Locals,
        depth: usize,
    ) -> Lowering<(Type, String)> {
        let mut text = self.string("", Span::default())?;
        for part in parts {
            if !matches!(part.ty, Type::Int | Type::Float | Type::Bool | Type::String) {
                return Err(invalid(part.span, "interpolation requires scalar values"));
            }
            let value = self.expr(part, locals, depth)?;
            let value = match part.ty {
                Type::String => value,
                Type::Int => self.assign(
                    locals,
                    Type::String,
                    &format!("call $fern_int_to_str(l {value})"),
                ),
                Type::Bool => {
                    let value = self.assign(locals, Type::Int, &format!("extuw {value}"));
                    self.assign(
                        locals,
                        Type::String,
                        &format!("call $fern_bool_to_str(l {value})"),
                    )
                }
                Type::Float => self.float_string(value, locals, part.span)?,
                _ => return Err(invalid(part.span, "interpolation requires scalar values")),
            };
            text = self.assign(
                locals,
                Type::String,
                &format!("call $fern_str_concat(l {text}, l {value})"),
            );
        }
        Ok((Type::String, text))
    }

    /// Format an IEEE double into a bounded GC allocation with round-trip precision.
    fn float_string(&mut self, value: String, locals: &mut Locals, span: Span) -> Lowering<String> {
        let format = self.string("%.17g", span)?;
        let buffer = self.assign(locals, Type::String, "call $fern_alloc(l 32)");
        self.output.push_str(&format!(
            "    call $snprintf(l {buffer}, l 32, l {format}, ..., d {value})\n"
        ));
        Ok(buffer)
    }
}
