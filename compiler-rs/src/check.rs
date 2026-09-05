//! Resolve source names and types once, including local compound-type inference.
use crate::{ast, ir, Constructor, Diagnostic, Span, Type};
use std::collections::{HashMap, HashSet};

const MAX_EXPR_DEPTH: usize = 128;
const MAX_EXPR_COUNT: usize = 100_000;
const MAX_FUNCTIONS: usize = 4096;
const MAX_PARAMETERS: usize = 255;
const MAX_TYPE_DEPTH: usize = 128;
const MAX_TYPE_NODES: usize = 4096;
type Checked<T> = Result<T, Diagnostic>;
type TypedKind = (ir::ExprKind, Type);

struct Signature {
    id: ir::FunctionId,
    params: Vec<Type>,
    result: Type,
}
#[derive(Default)]
struct Inference {
    bindings: Vec<Option<Type>>,
    ranks: Vec<u32>,
}
struct Checker<'a> {
    signatures: &'a HashMap<String, Signature>,
    scopes: Vec<HashMap<String, (ir::LocalId, Type)>>,
    local_count: usize,
    expr_count: usize,
    inference: Inference,
    function_return: Type,
}

/// Resolve `program` into fully concrete IR or its first source diagnostic.
/// No preconditions: caller-created syntax and recursive types are validated too.
pub fn check(program: &ast::Program) -> Checked<ir::Program> {
    let signatures = signatures(program)?;
    let functions = program
        .functions
        .iter()
        .map(|function| {
            Checker {
                signatures: &signatures,
                scopes: vec![HashMap::new()],
                local_count: 0,
                expr_count: 0,
                inference: Inference::default(),
                function_return: Type::Unit,
            }
            .function(function)
        })
        .collect::<Checked<Vec<_>>>()?;
    Ok(ir::Program { functions })
}

/// Collect validated concrete signatures from `program` before checking bodies.
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
        if reserved(&function.name) {
            return Err(Diagnostic::new(
                function.span,
                format!(
                    "function name '{}' is reserved for a builtin",
                    function.name
                ),
            ));
        }
        let result = function_result(function)?;
        validate_type(&result, function.span)?;
        validate_parameters(function)?;
        signatures.insert(
            function.name.clone(),
            Signature {
                id: ir::FunctionId(index),
                params: function
                    .params
                    .iter()
                    .map(|param| param.ty.clone())
                    .collect(),
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

/// Return whether `name` denotes a builtin function, namespace, or sum constructor.
fn reserved(name: &str) -> bool {
    builtin(name).is_some()
        || matches!(
            name,
            "String" | "List" | "Option" | "Result" | "Some" | "None" | "Ok" | "Err"
        )
}

/// Validate `function` entry-point rules and return its annotated result.
fn function_result(function: &ast::Function) -> Checked<Type> {
    if let Some(ty) = &function.return_type {
        validate_type(ty, function.span)?;
    }
    if function.name == "main" {
        if !function.params.is_empty() {
            return Err(Diagnostic::new(
                function.span,
                "main must take zero parameters",
            ));
        }
        let ty = function.return_type.clone().unwrap_or(Type::Unit);
        if !matches!(ty, Type::Int | Type::Unit) {
            return Err(Diagnostic::new(
                function.span,
                "prototype main must return Int or Unit",
            ));
        }
        Ok(ty)
    } else {
        function.return_type.clone().ok_or_else(|| Diagnostic::new(function.span,
            "prototype requires a return type annotation for non-main functions; return type inference is unsupported"))
    }
}

/// Validate `function` parameter names, count and recursive annotations.
fn validate_parameters(function: &ast::Function) -> Checked<()> {
    if function.params.len() > MAX_PARAMETERS {
        return Err(Diagnostic::new(
            function.span,
            "prototype parameter limit exceeded (255)",
        ));
    }
    let mut names = HashSet::new();
    for param in &function.params {
        validate_type(&param.ty, param.span)?;
        if !names.insert(&param.name) {
            return Err(Diagnostic::new(
                param.span,
                format!("duplicate parameter '{}'", param.name),
            ));
        }
    }
    Ok(())
}

/// Validate explicit `ty` without recursive cloning; inference variables are private.
fn validate_type(ty: &Type, span: Span) -> Checked<()> {
    let mut pending = vec![(ty, 0)];
    let mut count = 0;
    while let Some((ty, depth)) = pending.pop() {
        count += 1;
        if depth >= MAX_TYPE_DEPTH || count > MAX_TYPE_NODES {
            return Err(Diagnostic::new(
                span,
                "prototype type nesting or size limit exceeded",
            ));
        }
        match ty {
            Type::Infer(_) => return Err(Diagnostic::new(span, "explicit types cannot contain inference variables; generic definitions are unsupported")),
            Type::List(inner) | Type::Option(inner) => pending.push((inner, depth + 1)),
            Type::Result(ok, err) => { pending.push((ok, depth + 1)); pending.push((err, depth + 1)); }
            Type::Int | Type::Bool | Type::String | Type::Unit => {}
        }
    }
    Ok(())
}

impl Inference {
    /// Allocate an inference variable unique to this bounded function body.
    fn fresh(&mut self) -> Type {
        let id = self.bindings.len() as u32;
        self.bindings.push(None);
        self.ranks.push(0);
        Type::Infer(id)
    }

    /// Resolve `ty` through current substitutions, preserving unresolved variables.
    fn resolve(&self, ty: &Type, span: Span) -> Checked<Type> {
        let mut budget = MAX_TYPE_NODES;
        self.resolve_inner(ty, span, 0, &mut budget)
    }

    /// Follow `ty` substitutions within explicit recursion and expansion budgets.
    fn resolve_inner(
        &self,
        ty: &Type,
        span: Span,
        depth: usize,
        budget: &mut usize,
    ) -> Checked<Type> {
        if depth >= MAX_TYPE_DEPTH || *budget == 0 {
            return Err(Diagnostic::new(
                span,
                "prototype type nesting or size limit exceeded",
            ));
        }
        *budget -= 1;
        Ok(match ty {
            Type::Infer(id) => match self.bindings.get(*id as usize) {
                Some(Some(value)) => self.resolve_inner(value, span, depth, budget)?,
                Some(None) => ty.clone(),
                None => return Err(Diagnostic::new(span, "invalid inference variable")),
            },
            Type::List(inner) => Type::List(Box::new(self.resolve_inner(
                inner,
                span,
                depth + 1,
                budget,
            )?)),
            Type::Option(inner) => Type::Option(Box::new(self.resolve_inner(
                inner,
                span,
                depth + 1,
                budget,
            )?)),
            Type::Result(ok, err) => Type::Result(
                Box::new(self.resolve_inner(ok, span, depth + 1, budget)?),
                Box::new(self.resolve_inner(err, span, depth + 1, budget)?),
            ),
            _ => ty.clone(),
        })
    }

    /// Unify `actual` with `expected`, reporting incompatible types at `span`.
    fn unify(&mut self, actual: &Type, expected: &Type, span: Span, context: &str) -> Checked<()> {
        let actual = self.resolve(actual, span)?;
        let expected = self.resolve(expected, span)?;
        if actual == expected {
            return Ok(());
        }
        match (&actual, &expected) {
            (Type::Infer(id), ty) | (ty, Type::Infer(id)) => self.assign(*id, ty, span),
            (Type::List(a), Type::List(b)) | (Type::Option(a), Type::Option(b)) => {
                self.unify(a, b, span, context)
            }
            (Type::Result(a, b), Type::Result(c, d)) => {
                self.unify(a, c, span, context)?;
                self.unify(b, d, span, context)
            }
            _ => Err(Diagnostic::new(
                span,
                format!("{context}: expected {expected:?}, found {actual:?}"),
            )),
        }
    }

    /// Bind `id` to already-resolved `ty`, rejecting recursive/infinite types.
    fn assign(&mut self, id: u32, ty: &Type, span: Span) -> Checked<()> {
        if let Type::Infer(other) = ty {
            self.union(id, *other);
            return Ok(());
        }
        let mut pending = vec![ty];
        while let Some(ty) = pending.pop() {
            match ty {
                Type::Infer(other) if *other == id => {
                    return Err(Diagnostic::new(
                        span,
                        "recursive inferred type is unsupported",
                    ))
                }
                Type::List(inner) | Type::Option(inner) => pending.push(inner),
                Type::Result(ok, err) => {
                    pending.push(ok);
                    pending.push(err);
                }
                _ => {}
            }
        }
        self.bindings[id as usize] = Some(ty.clone());
        Ok(())
    }

    /// Join two unresolved roots by rank, keeping long flat inference chains shallow.
    fn union(&mut self, left: u32, right: u32) {
        let left_rank = self.ranks[left as usize];
        let right_rank = self.ranks[right as usize];
        if left_rank < right_rank {
            self.bindings[left as usize] = Some(Type::Infer(right));
        } else {
            self.bindings[right as usize] = Some(Type::Infer(left));
            if left_rank == right_rank {
                self.ranks[left as usize] += 1;
            }
        }
    }

    /// Return the concrete form of `ty` or request a missing contextual annotation.
    fn concrete(&self, ty: &Type, span: Span) -> Checked<Type> {
        let ty = self.resolve(ty, span)?;
        let mut pending = vec![&ty];
        while let Some(ty) = pending.pop() {
            match ty {
                Type::Infer(_) => {
                    return Err(Diagnostic::new(
                        span,
                        "cannot infer compound payload type; add a concrete type annotation",
                    ))
                }
                Type::List(inner) | Type::Option(inner) => pending.push(inner),
                Type::Result(ok, err) => {
                    pending.push(ok);
                    pending.push(err);
                }
                _ => {}
            }
        }
        Ok(ty)
    }
}

impl Checker<'_> {
    /// Check a `function`, unify its return and eliminate all inference variables.
    fn function(&mut self, function: &ast::Function) -> Checked<ir::Function> {
        let signature = &self.signatures[&function.name];
        let id = signature.id;
        let return_type = signature.result.clone();
        self.function_return = return_type.clone();
        let params: Vec<ir::Param> = function
            .params
            .iter()
            .map(|param| ir::Param {
                id: self.bind(&param.name, param.ty.clone()),
                ty: param.ty.clone(),
            })
            .collect();
        let mut body = self.expression(&function.body, 0)?;
        let unit_main = function.name == "main" && return_type == Type::Unit;
        if !unit_main {
            self.inference
                .unify(&body.ty, &return_type, body.span, "function return")?;
        }
        self.finalize(&mut body)?;
        reject_unused_results(&body, &params)?;
        if unit_main {
            reject_discard(&body)?;
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

    /// Bind `name` with `ty` in the current scope and return a unique local ID.
    fn bind(&mut self, name: &str, ty: Type) -> ir::LocalId {
        let id = ir::LocalId(self.local_count);
        self.local_count += 1;
        if name != "_" {
            self.scopes
                .last_mut()
                .expect("function scope exists")
                .insert(name.into(), (id, ty));
        }
        id
    }

    /// Return the innermost `name` binding or none if it is unresolved.
    fn local(&self, name: &str) -> Option<(ir::LocalId, Type)> {
        self.scopes
            .iter()
            .rev()
            .find_map(|scope| scope.get(name).cloned())
    }

    /// Resolve `expr` at bounded `depth` into an expression retaining its type.
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
            ast::ExprKind::Unit => (ir::ExprKind::Unit, Type::Unit),
            ast::ExprKind::Try(value) => self.propagate(value, expr.span, depth + 1)?,
            ast::ExprKind::Name(name) => self.name(name, expr.span)?,
            ast::ExprKind::List(values) => self.list(values, depth + 1)?,
            ast::ExprKind::Match { value, arms } => {
                self.matching(value, arms, expr.span, depth + 1)?
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

    /// Propagate an error from `value` only within a compatible Result-returning function.
    fn propagate(&mut self, value: &ast::Expr, span: Span, depth: usize) -> Checked<TypedKind> {
        let Type::Result(_, error) = self.function_return.clone() else {
            return Err(Diagnostic::new(
                span,
                "? requires a function returning Result",
            ));
        };
        let value = self.expression(value, depth)?;
        let payload = self.inference.fresh();
        let actual_error = self.inference.fresh();
        let expected = Type::Result(Box::new(payload.clone()), Box::new(actual_error.clone()));
        self.inference
            .unify(&value.ty, &expected, span, "? operand must be Result")?;
        self.inference
            .unify(&actual_error, &error, span, "? error type")?;
        Ok((ir::ExprKind::Try(Box::new(value)), payload))
    }

    /// Resolve `name` as a local or the payloadless None constructor.
    fn name(&mut self, name: &str, span: Span) -> Checked<TypedKind> {
        if let Some((id, ty)) = self.local(name) {
            return Ok((ir::ExprKind::Local(id), ty));
        }
        if name == "None" {
            return Ok((
                ir::ExprKind::Construct {
                    constructor: Constructor::None,
                    value: None,
                },
                Type::Option(Box::new(self.inference.fresh())),
            ));
        }
        Err(Diagnostic::new(
            span,
            format!("unknown name '{name}' (function values are unsupported in the prototype)"),
        ))
    }

    /// Check homogeneous `values` at `depth`, leaving an empty list contextually inferable.
    fn list(&mut self, values: &[ast::Expr], depth: usize) -> Checked<TypedKind> {
        let element = self.inference.fresh();
        let mut checked = Vec::new();
        for value in values {
            let value = self.expression(value, depth)?;
            self.inference
                .unify(&value.ty, &element, value.span, "list element")?;
            checked.push(value);
        }
        Ok((ir::ExprKind::List(checked), Type::List(Box::new(element))))
    }

    /// Resolve `stmts` in a lexical scope, checking initializers before shadowing.
    fn block(&mut self, stmts: &[ast::Stmt], depth: usize) -> Checked<TypedKind> {
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
                    if let Some(expected) = annotation {
                        validate_type(expected, *span)?;
                    }
                    let value = self.expression(value, depth)?;
                    if let Some(expected) = annotation {
                        self.inference
                            .unify(&value.ty, expected, *span, "let annotation")?;
                    }
                    let id = self.bind(name, value.ty.clone());
                    checked.push(ir::Stmt::Let { id, value });
                    ty = Type::Unit;
                }
                ast::Stmt::Expr(value) => {
                    let value = self.expression(value, depth)?;
                    ty = value.ty.clone();
                    checked.push(ir::Stmt::Expr(value));
                }
            }
        }
        self.scopes.pop();
        Ok((ir::ExprKind::Block(checked), ty))
    }

    /// Check `op` and `value` at `depth`, returning a concretely constrained unary node.
    fn unary(&mut self, op: ast::UnaryOp, value: &ast::Expr, depth: usize) -> Checked<TypedKind> {
        let value = self.expression(value, depth)?;
        let ty = match op {
            ast::UnaryOp::Negate => Type::Int,
            ast::UnaryOp::Not => Type::Bool,
        };
        self.inference
            .unify(&value.ty, &ty, value.span, "unary operator")?;
        Ok((
            ir::ExprKind::Unary {
                op,
                value: Box::new(value),
            },
            ty,
        ))
    }

    /// Unify binary operands and constrain operators with a single concrete domain.
    /// Overloaded addition/equality are validated again after inference is complete.
    fn binary(
        &mut self,
        op: ast::BinaryOp,
        left: &ast::Expr,
        right: &ast::Expr,
        depth: usize,
    ) -> Checked<TypedKind> {
        use ast::BinaryOp::*;
        let left = self.expression(left, depth)?;
        let right = self.expression(right, depth)?;
        self.inference
            .unify(&left.ty, &right.ty, right.span, "binary operator")?;
        let ty = match op {
            Add => left.ty.clone(),
            Eq | Ne => Type::Bool,
            Subtract | Multiply | Divide | Remainder | Lt | Le | Gt | Ge => {
                self.inference
                    .unify(&left.ty, &Type::Int, left.span, "binary operator")?;
                if matches!(op, Lt | Le | Gt | Ge) {
                    Type::Bool
                } else {
                    Type::Int
                }
            }
            And | Or => {
                self.inference
                    .unify(&left.ty, &Type::Bool, left.span, "binary operator")?;
                Type::Bool
            }
        };
        Ok((
            ir::ExprKind::Binary {
                op,
                left: Box::new(left),
                right: Box::new(right),
            },
            ty,
        ))
    }

    /// Check `condition` and unify branch types; a missing else discards the then value.
    fn conditional(
        &mut self,
        condition: &ast::Expr,
        then_branch: &ast::Expr,
        else_branch: Option<&ast::Expr>,
        depth: usize,
    ) -> Checked<TypedKind> {
        let condition = self.expression(condition, depth)?;
        self.inference
            .unify(&condition.ty, &Type::Bool, condition.span, "if condition")?;
        let then_branch = self.expression(then_branch, depth)?;
        let else_branch = else_branch
            .map(|branch| self.expression(branch, depth))
            .transpose()?;
        let ty = if let Some(other) = &else_branch {
            self.inference
                .unify(&other.ty, &then_branch.ty, other.span, "if branch")?;
            then_branch.ty.clone()
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

    /// Reject calling local `name` or a module path whose root is locally shadowed.
    fn callable_name(&self, name: &str, span: Span) -> Checked<()> {
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
        Ok(())
    }

    /// Resolve a function or instantiate fresh type variables for a builtin call.
    fn resolve_callable(
        &mut self,
        name: &str,
        span: Span,
    ) -> Checked<(ir::CallTarget, Vec<Type>, Type)> {
        if let Some(builtin) = builtin(name) {
            let (params, result) = self.builtin_signature(builtin);
            Ok((ir::CallTarget::Builtin(builtin), params, result))
        } else if let Some(signature) = self.signatures.get(name) {
            Ok((
                ir::CallTarget::Function(signature.id),
                signature.params.clone(),
                signature.result.clone(),
            ))
        } else {
            Err(Diagnostic::new(span, format!("unknown function '{name}'")))
        }
    }

    /// Resolve callable `name` and unify each argument with its instantiated signature.
    fn call(
        &mut self,
        name: &str,
        args: &[ast::Expr],
        span: Span,
        depth: usize,
    ) -> Checked<TypedKind> {
        self.callable_name(name, span)?;
        if let Some(constructor) = constructor(name) {
            return self.construct(constructor, args, span, depth);
        }
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
            if !matches!(
                target,
                ir::CallTarget::Builtin(ir::Builtin::Print | ir::Builtin::Println)
            ) {
                self.inference
                    .unify(&arg.ty, &expected, arg.span, "call argument")?;
            }
        }
        Ok((ir::ExprKind::Call { target, args }, result))
    }

    /// Check a one-payload sum constructor; absent variants receive fresh variables.
    fn construct(
        &mut self,
        constructor: Constructor,
        args: &[ast::Expr],
        span: Span,
        depth: usize,
    ) -> Checked<TypedKind> {
        if args.len() != 1 {
            return Err(Diagnostic::new(span, "constructor expects one argument"));
        }
        let value = self.expression(&args[0], depth)?;
        let other = self.inference.fresh();
        let ty = match constructor {
            Constructor::Some => Type::Option(Box::new(value.ty.clone())),
            Constructor::Ok => Type::Result(Box::new(value.ty.clone()), Box::new(other)),
            Constructor::Err => Type::Result(Box::new(other), Box::new(value.ty.clone())),
            Constructor::None => {
                return Err(Diagnostic::new(
                    span,
                    "None must be written without arguments",
                ))
            }
        };
        Ok((
            ir::ExprKind::Construct {
                constructor,
                value: Some(Box::new(value)),
            },
            ty,
        ))
    }

    /// Instantiate the builtin's generic parameters locally to this call.
    fn builtin_signature(&mut self, builtin: ir::Builtin) -> (Vec<Type>, Type) {
        use ir::Builtin::*;
        let item = self.inference.fresh();
        let list = Type::List(Box::new(item.clone()));
        let option = Type::Option(Box::new(item.clone()));
        let result = Type::Result(Box::new(item.clone()), Box::new(self.inference.fresh()));
        match builtin {
            Print | Println => (vec![Type::Int], Type::Unit),
            StringConcat => (vec![Type::String, Type::String], Type::String),
            StringEq => (vec![Type::String, Type::String], Type::Bool),
            StringLen => (vec![Type::String], Type::Int),
            ListLen => (vec![list], Type::Int),
            ListGet => (vec![list, Type::Int], item),
            ListHead => (vec![list], item),
            ListTail | ListReverse => (vec![list.clone()], list),
            ListIsEmpty => (vec![list], Type::Bool),
            ListPush => (vec![list.clone(), item], list),
            ListConcat => (vec![list.clone(), list.clone()], list),
            ListContains => (vec![list, item], Type::Bool),
            OptionIsSome | OptionIsNone => (vec![option], Type::Bool),
            OptionUnwrapOr => (vec![option, item.clone()], item),
            ResultIsOk | ResultIsErr => (vec![result], Type::Bool),
            ResultUnwrapOr => (vec![result, item.clone()], item),
        }
    }

    /// Check scoped match arms, enforce reachability and unify their result types.
    fn matching(
        &mut self,
        value: &ast::Expr,
        arms: &[ast::MatchArm],
        span: Span,
        depth: usize,
    ) -> Checked<TypedKind> {
        let value = self.expression(value, depth)?;
        let ty = self.inference.fresh();
        let mut coverage = HashSet::new();
        let mut checked = Vec::new();
        for arm in arms {
            let subject = self.inference.resolve(&value.ty, span)?;
            if exhaustive(&subject, &coverage) || !coverage.insert(pattern_key(&arm.pattern.kind)) {
                return Err(Diagnostic::new(
                    arm.span,
                    "unreachable duplicate or already-covered match arm",
                ));
            }
            self.scopes.push(HashMap::new());
            let pattern = self.pattern(&arm.pattern, &value.ty)?;
            let body = self.expression(&arm.body, depth)?;
            self.scopes.pop();
            self.inference
                .unify(&body.ty, &ty, body.span, "match branch")?;
            checked.push(ir::MatchArm {
                pattern,
                body,
                span: arm.span,
            });
        }
        let subject = self.inference.resolve(&value.ty, span)?;
        if !exhaustive(&subject, &coverage) {
            return Err(Diagnostic::new(
                span,
                "match must be exhaustive; cover every variant or add a catchall arm",
            ));
        }
        Ok((
            ir::ExprKind::Match {
                value: Box::new(value),
                arms: checked,
            },
            ty,
        ))
    }

    /// Check a flat `pattern` against its subject `ty` and create scoped payload IDs.
    fn pattern(&mut self, pattern: &ast::Pattern, ty: &Type) -> Checked<ir::Pattern> {
        use ast::PatternKind::*;
        let (checked, expected) = match &pattern.kind {
            Wildcard => return Ok(ir::Pattern::Wildcard),
            Bind(name) => return Ok(ir::Pattern::Bind(self.bind(name, ty.clone()))),
            Int(n) => (ir::Pattern::Int(*n), Type::Int),
            Bool(value) => (ir::Pattern::Bool(*value), Type::Bool),
            String(value) => (ir::Pattern::String(value.clone()), Type::String),
            Constructor {
                constructor,
                binding,
            } => {
                return self.constructor_pattern(*constructor, binding.as_deref(), ty, pattern.span)
            }
        };
        self.inference
            .unify(ty, &expected, pattern.span, "match pattern")?;
        Ok(checked)
    }

    /// Constrain a constructor's subject and bind its selected payload, if requested.
    fn constructor_pattern(
        &mut self,
        constructor: Constructor,
        binding: Option<&str>,
        ty: &Type,
        span: Span,
    ) -> Checked<ir::Pattern> {
        if constructor == Constructor::None && binding.is_some() {
            return Err(Diagnostic::new(span, "None pattern has no payload to bind"));
        }
        let payload = self.inference.fresh();
        let other = self.inference.fresh();
        let expected = match constructor {
            Constructor::Some | Constructor::None => Type::Option(Box::new(payload.clone())),
            Constructor::Ok => Type::Result(Box::new(payload.clone()), Box::new(other)),
            Constructor::Err => Type::Result(Box::new(other), Box::new(payload.clone())),
        };
        self.inference
            .unify(ty, &expected, span, "constructor pattern")?;
        let binding = binding.map(|name| self.bind(name, payload));
        Ok(ir::Pattern::Constructor {
            constructor,
            binding,
        })
    }

    /// Normalize all expression types after constraints from the whole function are known.
    fn finalize(&self, expr: &mut ir::Expr) -> Checked<()> {
        expr.ty = self.inference.concrete(&expr.ty, expr.span)?;
        match &mut expr.kind {
            ir::ExprKind::Unary { value, .. } | ir::ExprKind::Try(value) => self.finalize(value)?,
            ir::ExprKind::Binary { op, left, right } => {
                self.finalize(left)?;
                self.finalize(right)?;
                if binary_result(*op, &left.ty, &right.ty).as_ref() != Some(&expr.ty) {
                    return Err(Diagnostic::new(
                        expr.span,
                        "binary operator does not support these operand types",
                    ));
                }
            }
            ir::ExprKind::Call { target, args } => {
                for arg in args.iter_mut() {
                    self.finalize(arg)?;
                }
                validate_builtin(*target, args, expr.span)?;
            }
            ir::ExprKind::List(values) => {
                for value in values {
                    self.finalize(value)?;
                }
            }
            ir::ExprKind::Construct {
                value: Some(value), ..
            } => self.finalize(value)?,
            ir::ExprKind::If {
                condition,
                then_branch,
                else_branch,
            } => {
                self.finalize(condition)?;
                self.finalize(then_branch)?;
                if let Some(branch) = else_branch {
                    self.finalize(branch)?;
                } else {
                    reject_discard(then_branch)?;
                }
            }
            ir::ExprKind::Match { value, arms } => {
                self.finalize(value)?;
                for arm in arms {
                    self.finalize(&mut arm.body)?;
                }
            }
            ir::ExprKind::Block(stmts) => self.finalize_block(stmts)?,
            _ => {}
        }
        Ok(())
    }

    /// Normalize block statements and reject non-final discarded Result expressions.
    fn finalize_block(&self, stmts: &mut [ir::Stmt]) -> Checked<()> {
        let len = stmts.len();
        for (index, stmt) in stmts.iter_mut().enumerate() {
            match stmt {
                ir::Stmt::Let { value, .. } => self.finalize(value)?,
                ir::Stmt::Expr(value) => {
                    self.finalize(value)?;
                    if index + 1 != len {
                        reject_discard(value)?;
                    }
                }
            }
        }
        Ok(())
    }
}

/// Return a stable coverage key for a flat pattern, treating all bindings as catchalls.
fn pattern_key(pattern: &ast::PatternKind) -> String {
    match pattern {
        ast::PatternKind::Wildcard | ast::PatternKind::Bind(_) => "all".into(),
        ast::PatternKind::Int(n) => format!("int:{n}"),
        ast::PatternKind::Bool(b) => format!("bool:{b}"),
        ast::PatternKind::String(s) => format!("string:{s}"),
        ast::PatternKind::Constructor { constructor, .. } => format!("constructor:{constructor:?}"),
    }
}

/// Return whether flat arm `coverage` exhausts `ty`'s domain.
fn exhaustive(ty: &Type, coverage: &HashSet<String>) -> bool {
    coverage.contains("all")
        || match ty {
            Type::Bool => coverage.contains("bool:true") && coverage.contains("bool:false"),
            Type::Option(_) => {
                coverage.contains("constructor:Some") && coverage.contains("constructor:None")
            }
            Type::Result(_, _) => {
                coverage.contains("constructor:Ok") && coverage.contains("constructor:Err")
            }
            _ => false,
        }
}

/// Reject ignored fallible values; binding, returning or consuming them is explicit handling.
fn reject_discard(expr: &ir::Expr) -> Checked<()> {
    if matches!(expr.ty, Type::Result(_, _)) {
        Err(Diagnostic::new(
            expr.span,
            "Result value must be handled; bind, return, or match it",
        ))
    } else {
        Ok(())
    }
}

/// Validate scalar-only builtin operations after argument inference is complete.
fn validate_builtin(target: ir::CallTarget, args: &[ir::Expr], span: Span) -> Checked<()> {
    match target {
        ir::CallTarget::Builtin(ir::Builtin::Print | ir::Builtin::Println)
            if !scalar(&args[0].ty) =>
        {
            Err(Diagnostic::new(
                span,
                "print argument must be Int, Bool, or String",
            ))
        }
        ir::CallTarget::Builtin(ir::Builtin::ListContains) if !scalar(&args[1].ty) => {
            Err(Diagnostic::new(
                span,
                "List.contains requires scalar Int, Bool, or String elements",
            ))
        }
        _ => Ok(()),
    }
}

/// Return whether `ty` supports scalar value equality and printing.
fn scalar(ty: &Type) -> bool {
    matches!(ty, Type::Int | Type::Bool | Type::String)
}

/// Return the payload constructor named by a call, excluding standalone None.
fn constructor(name: &str) -> Option<Constructor> {
    match name {
        "Some" => Some(Constructor::Some),
        "Ok" => Some(Constructor::Ok),
        "Err" => Some(Constructor::Err),
        _ => None,
    }
}

/// Return the stable builtin identity for an unshadowed source name.
fn builtin(name: &str) -> Option<ir::Builtin> {
    use ir::Builtin::*;
    Some(match name {
        "print" => Print,
        "println" => Println,
        "String.concat" => StringConcat,
        "String.eq" => StringEq,
        "String.len" => StringLen,
        "List.len" => ListLen,
        "List.get" => ListGet,
        "List.head" => ListHead,
        "List.tail" => ListTail,
        "List.is_empty" => ListIsEmpty,
        "List.push" => ListPush,
        "List.reverse" => ListReverse,
        "List.concat" => ListConcat,
        "List.contains" => ListContains,
        "Option.is_some" => OptionIsSome,
        "Option.is_none" => OptionIsNone,
        "Option.unwrap_or" => OptionUnwrapOr,
        "Result.is_ok" => ResultIsOk,
        "Result.is_err" => ResultIsErr,
        "Result.unwrap_or" => ResultUnwrapOr,
        _ => return None,
    })
}

/// Compute a supported concrete scalar operation result, rejecting pointer equality.
fn binary_result(op: ast::BinaryOp, left: &Type, right: &Type) -> Option<Type> {
    use ast::BinaryOp::*;
    if left != right {
        return None;
    }
    match op {
        Add if *left == Type::String => Some(Type::String),
        Add | Subtract | Multiply | Divide | Remainder if *left == Type::Int => Some(Type::Int),
        Eq | Ne if scalar(left) => Some(Type::Bool),
        Lt | Le | Gt | Ge if *left == Type::Int => Some(Type::Bool),
        And | Or if *left == Type::Bool => Some(Type::Bool),
        _ => None,
    }
}

/// Reject unused parameters, lets and named patterns whose types contain Results.
/// Alias chains end at another checked binding; deeper control-flow ownership is deferred.
fn reject_unused_results(expr: &ir::Expr, params: &[ir::Param]) -> Checked<()> {
    let mut pending = vec![expr];
    let mut referenced = HashSet::new();
    let mut fallible_bindings: Vec<_> = params
        .iter()
        .filter(|param| contains_result(&param.ty))
        .map(|param| (param.id.0, expr.span))
        .collect();
    while let Some(expr) = pending.pop() {
        match &expr.kind {
            ir::ExprKind::Local(id) => {
                referenced.insert(id.0);
            }
            ir::ExprKind::Block(stmts) => {
                for stmt in stmts {
                    match stmt {
                        ir::Stmt::Let { id, value } => {
                            if contains_result(&value.ty) {
                                fallible_bindings.push((id.0, value.span));
                            }
                            pending.push(value);
                        }
                        ir::Stmt::Expr(value) => pending.push(value),
                    }
                }
            }
            ir::ExprKind::Unary { value, .. } | ir::ExprKind::Try(value) => pending.push(value),
            ir::ExprKind::Binary { left, right, .. } => {
                pending.push(left);
                pending.push(right);
            }
            ir::ExprKind::Call { args, .. } | ir::ExprKind::List(args) => pending.extend(args),
            ir::ExprKind::Construct {
                value: Some(value), ..
            } => pending.push(value),
            ir::ExprKind::If {
                condition,
                then_branch,
                else_branch,
            } => {
                pending.push(condition);
                pending.push(then_branch);
                if let Some(value) = else_branch {
                    pending.push(value);
                }
            }
            ir::ExprKind::Match { value, arms } => {
                pending.push(value);
                for arm in arms {
                    if let Some(id) = fallible_pattern(&arm.pattern, &value.ty) {
                        fallible_bindings.push((id.0, arm.span));
                    }
                    pending.push(&arm.body);
                }
            }
            _ => {}
        }
    }
    for (id, span) in fallible_bindings {
        if !referenced.contains(&id) {
            return Err(Diagnostic::new(
                span,
                "Result value must be handled; this Result binding is never used",
            ));
        }
    }
    Ok(())
}

/// Return whether a concrete type contains a Result, including through list/option wrappers.
fn contains_result(ty: &Type) -> bool {
    let mut current = ty;
    for _ in 0..MAX_TYPE_DEPTH {
        match current {
            Type::Result(_, _) => return true,
            Type::List(inner) | Type::Option(inner) => current = inner,
            _ => return false,
        }
    }
    false
}

/// Find a named fallible binding introduced by `pattern` for a concrete `subject`.
/// Wildcard patterns remain explicit handling; only named payload obligations are tracked.
fn fallible_pattern(pattern: &ir::Pattern, subject: &Type) -> Option<ir::LocalId> {
    let (id, ty) = match (pattern, subject) {
        (ir::Pattern::Bind(id), ty) => (*id, ty),
        (
            ir::Pattern::Constructor {
                constructor: Constructor::Some,
                binding: Some(id),
            },
            Type::Option(payload),
        ) => (*id, payload.as_ref()),
        (
            ir::Pattern::Constructor {
                constructor: Constructor::Ok,
                binding: Some(id),
            },
            Type::Result(payload, _),
        ) => (*id, payload.as_ref()),
        (
            ir::Pattern::Constructor {
                constructor: Constructor::Err,
                binding: Some(id),
            },
            Type::Result(_, payload),
        ) => (*id, payload.as_ref()),
        _ => return None,
    };
    contains_result(ty).then_some(id)
}
