//! Resolve source names and types once, before any QBE lowering.
use std::collections::HashMap;

use crate::{ast, ir, Diagnostic, Span, Type};

const MAX_EXPR_DEPTH: usize = 128;
const MAX_EXPR_COUNT: usize = 100_000;
const MAX_FUNCTIONS: usize = 4096;
const MAX_PARAMETERS: usize = 255;

type Checked<T> = Result<T, Diagnostic>;

struct Signature {
    id: ir::FunctionId,
    params: Vec<Type>,
    result: Type,
}

struct Checker<'a> {
    signatures: &'a HashMap<String, Signature>,
    scopes: Vec<HashMap<String, (ir::LocalId, Type)>>,
    local_count: usize,
    expr_count: usize,
}

/// Resolve `program` into typed IR, or return its first source diagnostic.
/// No preconditions: even manually constructed ASTs are bounded and validated.
pub fn check(program: &ast::Program) -> Checked<ir::Program> {
    let signatures = signatures(program)?;
    let functions = program
        .functions
        .iter()
        .map(|function| {
            let mut checker = Checker {
                signatures: &signatures,
                scopes: vec![HashMap::new()],
                local_count: 0,
                expr_count: 0,
            };
            checker.function(function)
        })
        .collect::<Checked<Vec<_>>>()?;
    Ok(ir::Program { functions })
}

/// Collect `program` signatures before bodies so forward calls and recursion resolve.
/// Returns validated signatures, rejecting missing entry points and unsupported inference.
fn signatures(program: &ast::Program) -> Checked<HashMap<String, Signature>> {
    if program.functions.len() > MAX_FUNCTIONS {
        return Err(Diagnostic::new(
            Span::default(),
            "prototype function limit exceeded",
        ));
    }
    let mut signatures = HashMap::new();
    for (index, function) in program.functions.iter().enumerate() {
        if signatures.contains_key(&function.name) {
            return Err(Diagnostic::new(
                function.span,
                format!(
                    "duplicate function '{}' (function clauses are unsupported in the prototype)",
                    function.name
                ),
            ));
        }
        if builtin(&function.name).is_some() || function.name == "String" {
            return Err(Diagnostic::new(
                function.span,
                format!(
                    "function name '{}' is reserved for a builtin",
                    function.name
                ),
            ));
        }
        let result = function_result(function)?;
        validate_parameters(function)?;
        signatures.insert(
            function.name.clone(),
            Signature {
                id: ir::FunctionId(index),
                params: function.params.iter().map(|param| param.ty).collect(),
                result,
            },
        );
    }
    if !signatures.contains_key("main") {
        return Err(Diagnostic::new(
            Span::default(),
            "program requires a main function",
        ));
    }
    Ok(signatures)
}

/// Validate `function` entry-point rules and return its declared result type.
/// Non-entry functions require an annotation in this explicitly bounded prototype.
fn function_result(function: &ast::Function) -> Checked<Type> {
    if function.name == "main" {
        if !function.params.is_empty() {
            return Err(Diagnostic::new(
                function.span,
                "main must take zero parameters",
            ));
        }
        let ty = function.return_type.unwrap_or(Type::Unit);
        if !matches!(ty, Type::Int | Type::Unit) {
            return Err(Diagnostic::new(
                function.span,
                "prototype main must return Int or Unit",
            ));
        }
        Ok(ty)
    } else {
        function.return_type.ok_or_else(|| Diagnostic::new(function.span,
            "prototype requires a return type annotation for non-main functions; return type inference is unsupported"))
    }
}

/// Check `function` parameter names and limits; return a diagnostic on duplicates.
fn validate_parameters(function: &ast::Function) -> Checked<()> {
    if function.params.len() > MAX_PARAMETERS {
        return Err(Diagnostic::new(
            function.span,
            "prototype parameter limit exceeded (255)",
        ));
    }
    let mut names = HashMap::new();
    for param in &function.params {
        if names.insert(&param.name, ()).is_some() {
            return Err(Diagnostic::new(
                param.span,
                format!("duplicate parameter '{}'", param.name),
            ));
        }
    }
    Ok(())
}

impl Checker<'_> {
    /// Check `function` using pre-collected signatures and return a resolved body.
    /// The checker must be newly created for this function.
    fn function(&mut self, function: &ast::Function) -> Checked<ir::Function> {
        let signature = &self.signatures[&function.name];
        let id = signature.id;
        let return_type = signature.result;
        let params = function
            .params
            .iter()
            .map(|param| ir::Param {
                id: self.bind(&param.name, param.ty),
                ty: param.ty,
            })
            .collect();
        let body = self.expression(&function.body, 0)?;
        if !(function.name == "main" && return_type == Type::Unit) {
            expect_type(body.ty, return_type, body.span, "function return")?;
        }
        Ok(ir::Function {
            id,
            name: function.name.clone(),
            params,
            return_type,
            body,
            local_count: self.local_count,
        })
    }

    /// Bind `name` with `ty` in the current scope and return its unique local ID.
    /// The checker always contains at least its function scope.
    fn bind(&mut self, name: &str, ty: Type) -> ir::LocalId {
        let id = ir::LocalId(self.local_count);
        self.local_count += 1;
        self.scopes
            .last_mut()
            .expect("function scope exists")
            .insert(name.into(), (id, ty));
        id
    }

    /// Find the innermost binding for `name`; return none if it is unresolved.
    fn local(&self, name: &str) -> Option<(ir::LocalId, Type)> {
        self.scopes
            .iter()
            .rev()
            .find_map(|scope| scope.get(name).copied())
    }

    /// Resolve `expr` at bounded `depth` into a node retaining its semantic type.
    /// Returns an error before exceeding the prototype work or recursion budget.
    fn expression(&mut self, expr: &ast::Expr, depth: usize) -> Checked<ir::Expr> {
        if depth >= MAX_EXPR_DEPTH {
            return Err(Diagnostic::new(
                expr.span,
                "prototype expression nesting limit exceeded (128)",
            ));
        }
        self.expr_count += 1;
        if self.expr_count > MAX_EXPR_COUNT {
            return Err(Diagnostic::new(
                expr.span,
                "prototype expression count limit exceeded",
            ));
        }
        let (kind, ty) = match &expr.kind {
            ast::ExprKind::Int(n) => (ir::ExprKind::Int(*n), Type::Int),
            ast::ExprKind::Bool(b) => (ir::ExprKind::Bool(*b), Type::Bool),
            ast::ExprKind::String(s) => (ir::ExprKind::String(s.clone()), Type::String),
            ast::ExprKind::Name(name) => {
                let (id, ty) = self.local(name).ok_or_else(|| Diagnostic::new(expr.span, format!("unknown name '{name}' (function values are unsupported in the prototype)")))?;
                (ir::ExprKind::Local(id), ty)
            }
            ast::ExprKind::Unary { op, value } => self.unary(*op, value, depth + 1)?,
            ast::ExprKind::Binary { op, left, right } => {
                self.binary(*op, left, right, depth + 1)?
            }
            ast::ExprKind::Call { name, args } => self.call(name, args, expr.span, depth + 1)?,
            ast::ExprKind::If {
                condition,
                then_branch,
                else_branch,
            } => self.conditional(condition, then_branch, else_branch.as_deref(), depth + 1)?,
            ast::ExprKind::Block(stmts) => self.block(stmts, depth + 1)?,
        };
        Ok(ir::Expr {
            kind,
            ty,
            span: expr.span,
        })
    }

    /// Resolve `stmts` in a new lexical scope at `depth`, returning its final type.
    /// Let initializers resolve before the new binding is introduced.
    fn block(&mut self, stmts: &[ast::Stmt], depth: usize) -> Checked<(ir::ExprKind, Type)> {
        self.scopes.push(HashMap::new());
        let mut checked = Vec::new();
        let mut ty = Type::Unit;
        for stmt in stmts {
            match stmt {
                ast::Stmt::Let {
                    name,
                    annotation,
                    value,
                    span,
                } => {
                    let value = self.expression(value, depth)?;
                    if let Some(expected) = annotation {
                        expect_type(value.ty, *expected, *span, "let annotation")?;
                    }
                    let id = self.bind(name, value.ty);
                    checked.push(ir::Stmt::Let { id, value });
                    ty = Type::Unit;
                }
                ast::Stmt::Expr(value) => {
                    let value = self.expression(value, depth)?;
                    ty = value.ty;
                    checked.push(ir::Stmt::Expr(value));
                }
            }
        }
        self.scopes.pop();
        Ok((ir::ExprKind::Block(checked), ty))
    }

    /// Check `op` and `value` at `depth`; return a correctly typed unary node.
    fn unary(
        &mut self,
        op: ast::UnaryOp,
        value: &ast::Expr,
        depth: usize,
    ) -> Checked<(ir::ExprKind, Type)> {
        let value = self.expression(value, depth)?;
        let ty = match op {
            ast::UnaryOp::Negate => Type::Int,
            ast::UnaryOp::Not => Type::Bool,
        };
        expect_type(value.ty, ty, value.span, "unary operator")?;
        Ok((
            ir::ExprKind::Unary {
                op,
                value: Box::new(value),
            },
            ty,
        ))
    }

    /// Check `op` against `left` and `right` at `depth`, returning typed operands.
    fn binary(
        &mut self,
        op: ast::BinaryOp,
        left: &ast::Expr,
        right: &ast::Expr,
        depth: usize,
    ) -> Checked<(ir::ExprKind, Type)> {
        let left = self.expression(left, depth)?;
        let right = self.expression(right, depth)?;
        let ty = binary_result(op, left.ty, right.ty).ok_or_else(|| {
            Diagnostic::new(
                right.span,
                format!(
                    "binary operator {op:?} does not accept {:?} and {:?}",
                    left.ty, right.ty
                ),
            )
        })?;
        Ok((
            ir::ExprKind::Binary {
                op,
                left: Box::new(left),
                right: Box::new(right),
            },
            ty,
        ))
    }

    /// Validate Boolean `condition` and branch agreement at `depth`.
    /// A missing `else_branch` makes the conditional Unit and discards its then value.
    fn conditional(
        &mut self,
        condition: &ast::Expr,
        then_branch: &ast::Expr,
        else_branch: Option<&ast::Expr>,
        depth: usize,
    ) -> Checked<(ir::ExprKind, Type)> {
        let condition = self.expression(condition, depth)?;
        expect_type(condition.ty, Type::Bool, condition.span, "if condition")?;
        let then_branch = self.expression(then_branch, depth)?;
        let else_branch = else_branch
            .map(|branch| self.expression(branch, depth))
            .transpose()?;
        let ty = if let Some(other) = &else_branch {
            expect_type(other.ty, then_branch.ty, other.span, "if branch")?;
            then_branch.ty
        } else {
            Type::Unit
        };
        Ok((
            ir::ExprKind::If {
                condition: Box::new(condition),
                then_branch: Box::new(then_branch),
                else_branch: else_branch.map(Box::new),
            },
            ty,
        ))
    }

    /// Resolve `name` at `span` to a callable signature, rejecting local shadowing.
    fn resolve_callable(
        &self,
        name: &str,
        span: Span,
    ) -> Checked<(ir::CallTarget, Vec<Type>, Type)> {
        if self.local(name).is_some() {
            return Err(Diagnostic::new(
                span,
                format!("local '{name}' is not callable"),
            ));
        }
        if let Some((module, _)) = name.split_once('.') {
            if self.local(module).is_some() {
                return Err(Diagnostic::new(
                    span,
                    format!("local '{module}' shadows the builtin module"),
                ));
            }
        }
        if let Some(builtin) = builtin(name) {
            let (params, result) = builtin_signature(builtin);
            Ok((ir::CallTarget::Builtin(builtin), params, result))
        } else if let Some(signature) = self.signatures.get(name) {
            Ok((
                ir::CallTarget::Function(signature.id),
                signature.params.clone(),
                signature.result,
            ))
        } else {
            Err(Diagnostic::new(span, format!("unknown function '{name}'")))
        }
    }

    /// Resolve callable `name`, then validate `args` at `depth` and report errors at `span`.
    /// Local shadowing is honored: non-function locals cannot silently call a builtin.
    fn call(
        &mut self,
        name: &str,
        args: &[ast::Expr],
        span: Span,
        depth: usize,
    ) -> Checked<(ir::ExprKind, Type)> {
        let (target, expected, result) = self.resolve_callable(name, span)?;
        if args.len() != expected.len() {
            return Err(Diagnostic::new(
                span,
                format!(
                    "'{name}' expects {} argument(s), found {}",
                    expected.len(),
                    args.len()
                ),
            ));
        }
        let args = args
            .iter()
            .map(|arg| self.expression(arg, depth))
            .collect::<Checked<Vec<_>>>()?;
        for (arg, expected) in args.iter().zip(expected) {
            if matches!(
                target,
                ir::CallTarget::Builtin(ir::Builtin::Print | ir::Builtin::Println)
            ) {
                if arg.ty == Type::Unit {
                    return Err(Diagnostic::new(
                        arg.span,
                        "print argument must be Int, Bool, or String",
                    ));
                }
            } else {
                expect_type(arg.ty, expected, arg.span, "call argument")?;
            }
        }
        Ok((ir::ExprKind::Call { target, args }, result))
    }
}

/// Return the builtin identity for `name`, or none for user-defined names.
fn builtin(name: &str) -> Option<ir::Builtin> {
    match name {
        "print" => Some(ir::Builtin::Print),
        "println" => Some(ir::Builtin::Println),
        "String.concat" => Some(ir::Builtin::StringConcat),
        "String.eq" => Some(ir::Builtin::StringEq),
        "String.len" => Some(ir::Builtin::StringLen),
        _ => None,
    }
}

/// Return `builtin` parameter and result types; print uses its separate polymorphic rule.
fn builtin_signature(builtin: ir::Builtin) -> (Vec<Type>, Type) {
    match builtin {
        ir::Builtin::Print | ir::Builtin::Println => (vec![Type::Int], Type::Unit),
        ir::Builtin::StringConcat => (vec![Type::String, Type::String], Type::String),
        ir::Builtin::StringEq => (vec![Type::String, Type::String], Type::Bool),
        ir::Builtin::StringLen => (vec![Type::String], Type::Int),
    }
}

/// Determine the result of `op` on `left` and `right`; reject unsupported operand pairs.
fn binary_result(op: ast::BinaryOp, left: Type, right: Type) -> Option<Type> {
    use ast::BinaryOp::*;
    if left != right {
        return None;
    }
    match op {
        Add if left == Type::String => Some(Type::String),
        Add | Subtract | Multiply | Divide | Remainder if left == Type::Int => Some(Type::Int),
        Eq | Ne if matches!(left, Type::Int | Type::Bool | Type::String) => Some(Type::Bool),
        Lt | Le | Gt | Ge if left == Type::Int => Some(Type::Bool),
        And | Or if left == Type::Bool => Some(Type::Bool),
        _ => None,
    }
}

/// Compare `actual` and `expected`, returning a diagnostic at `span` with `context` on mismatch.
fn expect_type(actual: Type, expected: Type, span: Span, context: &str) -> Checked<()> {
    if actual == expected {
        Ok(())
    } else {
        Err(Diagnostic::new(
            span,
            format!("{context}: expected {expected:?}, found {actual:?}"),
        ))
    }
}
