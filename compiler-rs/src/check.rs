//! Resolve source names and types once, including local compound-type inference.
use crate::{ast, ir, runtime, Constructor, Diagnostic, Span, Type};
use std::collections::{HashMap, HashSet};
mod clauses;
mod closures;
mod control;
mod coverage;
mod iteration;
mod lift;
mod maps;
mod nominal;
mod parameters;
mod pipes;
mod preflight;
mod returns;
mod sequences;
mod specialize;
mod with_flow;

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
    generics: Vec<String>,
    dispatch: bool,
}
#[derive(Default)]
struct Inference {
    bindings: Vec<Option<Type>>,
    ranks: Vec<u32>,
    probing: bool,
    template: bool,
    template_names: HashSet<String>,
    probe_work: std::cell::Cell<usize>,
    settling: bool,
    pending_returns: Vec<returns::DeferredCall>,
}
struct Checker<'a> {
    signatures: &'a HashMap<String, Signature>,
    registry: &'a nominal::Registry,
    scopes: Vec<HashMap<String, (ir::LocalId, Type)>>,
    local_count: usize,
    expr_count: usize,
    inference: Inference,
    function_return: Type,
    deferred: bool,
    loop_depth: usize,
}

/// Resolve `program` into fully concrete IR or its first source diagnostic.
/// No preconditions: caller-created syntax and recursive types are validated too.
pub fn check(program: &ast::Program) -> Checked<ir::Program> {
    preflight::check(program)?;
    let registry = nominal::Registry::new(program)?;
    let parameters = parameters::resolve(program, &registry)?;
    let normalized = clauses::normalize(&parameters)?;
    if !normalized.dispatch.is_empty() {
        preflight::check(&normalized.program)?;
    }
    let (program, signatures) =
        returns::resolve(&normalized.program, &registry, &normalized.dispatch)?;
    clauses::validate_templates(&program, &registry, &signatures)?;
    specialize::run(&program, &registry, &signatures)
}

/// Collect validated concrete signatures from `program` before checking bodies.
fn signatures(
    program: &ast::Program,
    registry: &nominal::Registry,
    inference: &mut Inference,
    dispatch: &HashSet<String>,
) -> Checked<HashMap<String, Signature>> {
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
                format!("duplicate normalized function '{}'", function.name),
            ));
        }
        if reserved(&function.name) || registry.constructor(&function.name).is_some() {
            return Err(Diagnostic::new(
                function.span,
                format!(
                    "function name '{}' is reserved for a builtin",
                    function.name
                ),
            ));
        }
        let result = returns::initial_result(function, inference)?;
        validate_parameters(function)?;
        let generics = nominal::generics(
            function
                .params
                .iter()
                .map(|p| clauses::parameter_type(p).clone())
                .chain(std::iter::once(result.clone())),
        );
        let allowed = generics.iter().cloned().collect();
        if !matches!(result, Type::Infer(_)) {
            registry.validate(&result, &allowed, function.span)?;
        }
        for param in &function.params {
            registry.validate(clauses::parameter_type(param), &allowed, param.span)?;
        }
        signatures.insert(
            function.name.clone(),
            Signature {
                id: ir::FunctionId(index),
                params: function
                    .params
                    .iter()
                    .map(|param| clauses::parameter_type(param).clone())
                    .collect(),
                result,
                generics,
                dispatch: dispatch.contains(&function.name),
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
    if runtime::native_type(name).is_some() {
        return true;
    }
    builtin(name).is_some()
        || runtime::lookup(name).is_some()
        || runtime::names().iter().any(|api| {
            api.strip_prefix(name)
                .is_some_and(|suffix| suffix.starts_with('.'))
        })
        || matches!(
            name,
            "Range"
                | "String"
                | "List"
                | "Map"
                | "Option"
                | "Result"
                | "Some"
                | "None"
                | "Ok"
                | "Err"
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
        if !returns::main_result(&ty) {
            return Err(Diagnostic::new(
                function.span,
                "main must return Int, Unit, or Result(Unit, concrete error type)",
            ));
        }
        Ok(ty)
    } else {
        function.return_type.clone().ok_or_else(|| {
            Diagnostic::new(
                function.span,
                "public functions require a return type annotation",
            )
        })
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
        validate_type(clauses::parameter_type(param), param.span)?;
        let name = clauses::parameter_name(param);
        if name != "_" && !names.insert(name) {
            return Err(Diagnostic::new(
                param.span,
                format!("duplicate parameter '{name}'"),
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
            Type::Function(args, result) => {
                if args.len() > MAX_PARAMETERS { return Err(Diagnostic::new(span, "function parameter limit exceeded")); }
                pending.extend(args.iter().map(|a| (a, depth + 1)));
                pending.push((result, depth + 1));
            }
            Type::Never => return Err(Diagnostic::new(span, "Never is an internal control-flow type")),
            Type::Infer(_) => return Err(Diagnostic::new(span, "explicit types cannot contain inference variables; generic definitions are unsupported")),
            Type::List(inner) | Type::Option(inner) => pending.push((inner, depth + 1)),
            Type::Map(key, value) => { maps::validate_key(key, span, true)?; pending.push((key, depth + 1)); pending.push((value, depth + 1)); }
            Type::Result(ok, err) => { pending.push((ok, depth + 1)); pending.push((err, depth + 1)); }
            Type::Tuple(args) | Type::Named(_, args) => pending.extend(args.iter().map(|a|(a, depth + 1))),
            Type::Range | Type::Native(_) | Type::Generic(_) | Type::Float | Type::Int | Type::Bool | Type::String | Type::Unit => {}
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
        returns::charge(self, span)?;
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
            Type::Function(args, result) => Type::Function(
                args.iter()
                    .map(|a| self.resolve_inner(a, span, depth + 1, budget))
                    .collect::<Checked<Vec<_>>>()?,
                Box::new(self.resolve_inner(result, span, depth + 1, budget)?),
            ),
            Type::Tuple(args) => Type::Tuple(
                args.iter()
                    .map(|a| self.resolve_inner(a, span, depth + 1, budget))
                    .collect::<Checked<Vec<_>>>()?,
            ),
            Type::Named(name, args) => Type::Named(
                name.clone(),
                args.iter()
                    .map(|a| self.resolve_inner(a, span, depth + 1, budget))
                    .collect::<Checked<Vec<_>>>()?,
            ),
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
            Type::Map(key, value) => Type::Map(
                Box::new(self.resolve_inner(key, span, depth + 1, budget)?),
                Box::new(self.resolve_inner(value, span, depth + 1, budget)?),
            ),
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
        if actual == Type::Never || expected == Type::Never || actual == expected {
            return Ok(());
        }
        if self.template
            && (matches!(actual, Type::Generic(_)) || matches!(expected, Type::Generic(_)))
            && !matches!(actual, Type::Infer(_))
            && !matches!(expected, Type::Infer(_))
        {
            return Ok(());
        }
        match (&actual, &expected) {
            (Type::Infer(id), ty) | (ty, Type::Infer(id)) => self.assign(*id, ty, span),
            (Type::Function(a, result_a), Type::Function(b, result_b)) if a.len() == b.len() => {
                for (a, b) in a.iter().zip(b) {
                    self.unify(a, b, span, context)?;
                }
                self.unify(result_a, result_b, span, context)
            }
            (Type::List(a), Type::List(b)) | (Type::Option(a), Type::Option(b)) => {
                self.unify(a, b, span, context)
            }
            (Type::Named(a, xs), Type::Named(b, ys)) if a == b && xs.len() == ys.len() => {
                for (x, y) in xs.iter().zip(ys) {
                    self.unify(x, y, span, context)?;
                }
                Ok(())
            }
            (Type::Tuple(xs), Type::Tuple(ys)) if xs.len() == ys.len() => {
                for (x, y) in xs.iter().zip(ys) {
                    self.unify(x, y, span, context)?;
                }
                Ok(())
            }
            (Type::Result(a, b), Type::Result(c, d)) | (Type::Map(a, b), Type::Map(c, d)) => {
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
                Type::Result(ok, err) | Type::Map(ok, err) => {
                    pending.push(ok);
                    pending.push(err);
                }
                Type::Function(args, result) => {
                    pending.extend(args);
                    pending.push(result);
                }
                Type::Tuple(args) | Type::Named(_, args) => pending.extend(args),
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
        if ty == Type::Never {
            return Ok(ty);
        }
        let mut pending = vec![&ty];
        while let Some(ty) = pending.pop() {
            match ty {
                Type::Never => {
                    return Err(Diagnostic::new(
                        span,
                        "Never cannot be stored inside a value type",
                    ))
                }
                Type::Infer(_) | Type::Generic(_) => {
                    return Err(Diagnostic::new(
                        span,
                        "cannot infer compound payload type; add a concrete type annotation",
                    ))
                }
                Type::List(inner) | Type::Option(inner) => pending.push(inner),
                Type::Result(ok, err) | Type::Map(ok, err) => {
                    pending.push(ok);
                    pending.push(err);
                }
                Type::Function(args, result) => {
                    pending.extend(args);
                    pending.push(result);
                }
                Type::Tuple(args) | Type::Named(_, args) => pending.extend(args),
                _ => {}
            }
        }
        maps::validate_concrete_keys(&ty, span)?;
        Ok(ty)
    }
}

impl Checker<'_> {
    /// Check a `function`, unify its return and eliminate all inference variables.
    fn function(&mut self, function: &ast::Function) -> Checked<ir::Function> {
        let signature = &self.signatures[&function.name];
        let id = signature.id;
        let return_type = function_result(function)?;
        self.function_return = return_type.clone();
        let params: Vec<ir::Param> = function
            .params
            .iter()
            .map(|param| ir::Param {
                id: self.bind(
                    clauses::parameter_name(param),
                    clauses::parameter_type(param).clone(),
                ),
                ty: clauses::parameter_type(param).clone(),
            })
            .collect();
        let unit_main = function.name == "main" && return_type == Type::Unit;
        let mut body = self
            .expression_expected(
                &function.body,
                if unit_main { None } else { Some(&return_type) },
                0,
            )
            .map_err(|e| closures::context(e, "function return"))?;
        if !unit_main {
            self.inference
                .unify(&body.ty, &return_type, body.span, "function return")?;
        }
        self.finalize(&mut body)?;
        if signature.dispatch {
            clauses::validate_dispatch(&body, self.registry)?;
        }
        reject_unused_results(&body, &params, self.registry)?;
        if unit_main {
            reject_discard(&body, self.registry)?;
        }
        Ok(ir::Function {
            id,
            name: function.name.clone(),
            params,
            captures: Vec::new(),
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
        self.expression_expected(expr, None, depth)
    }

    /// Bound checked expression traversal before allocating typed nodes.
    fn expression_budget(&mut self, span: Span, depth: usize) -> Checked<()> {
        returns::charge(&self.inference, span)?;
        if depth >= MAX_EXPR_DEPTH {
            return Err(Diagnostic::new(
                span,
                "prototype expression nesting limit exceeded (128)",
            ));
        }
        self.expr_count += 1;
        if self.expr_count > MAX_EXPR_COUNT {
            return Err(Diagnostic::new(
                span,
                "prototype expression count limit exceeded",
            ));
        }
        Ok(())
    }

    /// Propagate contextual types before checking lambda bodies and nested result expressions.
    fn expression_expected(
        &mut self,
        expr: &ast::Expr,
        expected: Option<&Type>,
        depth: usize,
    ) -> Checked<ir::Expr> {
        self.expression_budget(expr.span, depth)?;
        let (kind, ty) = self.expression_kind(expr, expected, depth)?;
        let (kind, ty) = control::strict_divergence(kind, ty);
        returns::charge_output(&self.inference, &ty, expr.span)?;
        if let Some(expected) = expected {
            self.inference
                .unify(&ty, expected, expr.span, "expression type")?;
        }
        Ok(ir::Expr {
            kind,
            ty,
            span: expr.span,
        })
    }

    /// Resolve expression forms while preserving the enclosing expected type.
    fn expression_kind(
        &mut self,
        expr: &ast::Expr,
        expected: Option<&Type>,
        depth: usize,
    ) -> Checked<TypedKind> {
        Ok(match &expr.kind {
            ast::ExprKind::Break
            | ast::ExprKind::Continue
            | ast::ExprKind::Range { .. }
            | ast::ExprKind::For { .. }
            | ast::ExprKind::With { .. }
            | ast::ExprKind::Return(_)
            | ast::ExprKind::Defer(_)
            | ast::ExprKind::PostfixIf { .. }
            | ast::ExprKind::ConditionMatch(_) => self.iteration_kind(expr, expected, depth + 1)?,
            ast::ExprKind::Lambda { params, body } => {
                self.lambda(params, body, expected, expr.span, depth + 1)?
            }
            ast::ExprKind::Apply { callee, args } => {
                self.apply(callee, args, expected, expr.span, depth + 1)?
            }
            ast::ExprKind::Int(n) => (ir::ExprKind::Int(*n), Type::Int),
            ast::ExprKind::Float(n) => (ir::ExprKind::Float(*n), Type::Float),
            ast::ExprKind::Bool(b) => (ir::ExprKind::Bool(*b), Type::Bool),
            ast::ExprKind::String(s) => (ir::ExprKind::String(s.clone()), Type::String),
            ast::ExprKind::Interpolate(parts) | ast::ExprKind::MultilineString(parts) => {
                self.interpolate(parts, expr.span, depth + 1)?
            }
            ast::ExprKind::Unit => (ir::ExprKind::Unit, Type::Unit),
            ast::ExprKind::Try(value) => self.propagate(value, expr.span, depth + 1)?,
            ast::ExprKind::Pipe {
                value,
                name,
                args,
                position,
            } => self.pipe(value, name, args, *position, expr.span, depth + 1)?,
            ast::ExprKind::Field { .. } => self.source_field(expr, depth + 1)?,
            ast::ExprKind::Name(name) => self.name(name, expr.span)?,
            ast::ExprKind::Tuple(values) => self.tuple(values, expected, expr.span, depth + 1)?,
            ast::ExprKind::Map(entries) => self.map(entries, expected, expr.span, depth + 1)?,
            ast::ExprKind::RecordUpdate { value, fields } => {
                self.record_update(value, fields, expected, expr.span, depth + 1)?
            }
            ast::ExprKind::List(values) => self.list(values, expected, expr.span, depth + 1)?,
            ast::ExprKind::Match { value, arms } => {
                self.matching(value, arms, expected, expr.span, depth + 1)?
            }
            ast::ExprKind::Unary { op, value } => self.unary(*op, value, depth + 1)?,
            ast::ExprKind::Binary { op, left, right } => {
                self.binary(*op, left, right, depth + 1)?
            }
            ast::ExprKind::Call { name, args } => {
                self.call_expected(name, args, expected, expr.span, depth + 1)?
            }
            ast::ExprKind::If {
                condition,
                then_branch,
                else_branch,
            } => self.conditional(
                condition,
                then_branch,
                else_branch.as_deref(),
                expected,
                depth + 1,
            )?,
            ast::ExprKind::Block(stmts) => self.block(stmts, expected, depth + 1)?,
        })
    }

    /// Lower text and embedded values in their original evaluation order.
    fn interpolate(
        &mut self,
        parts: &[ast::StringPart],
        span: Span,
        depth: usize,
    ) -> Checked<TypedKind> {
        let mut values = Vec::new();
        for part in parts {
            values.push(match part {
                ast::StringPart::Text(text) => ir::Expr {
                    kind: ir::ExprKind::String(text.clone()),
                    ty: Type::String,
                    span,
                },
                ast::StringPart::Value(value) => self.expression(value, depth)?,
            });
        }
        Ok((ir::ExprKind::Interpolate(values), Type::String))
    }

    /// Propagate an error from `value` only within a compatible Result-returning function.
    fn propagate(&mut self, value: &ast::Expr, span: Span, depth: usize) -> Checked<TypedKind> {
        if self.deferred {
            return Err(Diagnostic::new(span, "defer cannot use ?"));
        }
        let returning = self.inference.resolve(&self.function_return, span)?;
        let returning = if matches!(returning, Type::Infer(_)) {
            let result = Type::Result(
                Box::new(self.inference.fresh()),
                Box::new(self.inference.fresh()),
            );
            self.inference
                .unify(&returning, &result, span, "lambda return")?;
            result
        } else {
            returning
        };
        let Type::Result(_, error) = returning else {
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
        if let Some((root, rest)) = name.split_once('.') {
            if let Some((id, ty)) = self.local(root) {
                let mut value = ir::Expr {
                    kind: ir::ExprKind::Local(id),
                    ty,
                    span,
                };
                for field in rest.split('.') {
                    let (kind, ty) = self.field(value, field, span)?;
                    value = ir::Expr { kind, ty, span };
                }
                return Ok((value.kind, value.ty));
            }
        }
        if self.registry.constructor(name).is_some() {
            return self.custom_construct(name, &[], None, span, 0);
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
        if builtin(name).is_some()
            || self.signatures.contains_key(name)
            || runtime::lookup(name).is_some()
        {
            let (target, params, result) = self.resolve_callable(name, span)?;
            return Ok((
                ir::ExprKind::FunctionValue { target },
                Type::Function(params, Box::new(result)),
            ));
        }
        Err(Diagnostic::new(span, format!("unknown name '{name}'")))
    }

    /// Check homogeneous `values` at `depth`, leaving an empty list contextually inferable.
    fn list(
        &mut self,
        values: &[ast::Expr],
        expected: Option<&Type>,
        span: Span,
        depth: usize,
    ) -> Checked<TypedKind> {
        let element = self.inference.fresh();
        self.constrain_result(&Type::List(Box::new(element.clone())), expected, span)?;
        let mut checked = Vec::new();
        for value in values {
            let value = self
                .expression_expected(value, Some(&element), depth)
                .map_err(|e| closures::context(e, "list element"))?;
            self.inference
                .unify(&value.ty, &element, value.span, "list element")?;
            checked.push(value);
        }
        Ok((ir::ExprKind::List(checked), Type::List(Box::new(element))))
    }

    /// Resolve `stmts` in a lexical scope, checking initializers before shadowing.
    fn block(
        &mut self,
        stmts: &[ast::Stmt],
        expected: Option<&Type>,
        depth: usize,
    ) -> Checked<TypedKind> {
        self.scopes.push(HashMap::new());
        let mut checked = Vec::new();
        let mut ty = Type::Unit;
        for (index, stmt) in stmts.iter().enumerate() {
            if ty == Type::Never {
                return Err(Diagnostic::new(
                    control::statement_span(stmt),
                    "unreachable statement after control flow exits",
                ));
            }
            ty = self.statement(
                stmt,
                if index + 1 == stmts.len() {
                    expected
                } else {
                    None
                },
                depth,
                &mut checked,
            )?;
        }
        self.scopes.pop();
        Ok((ir::ExprKind::Block(checked), ty))
    }

    /// Bind an irrefutable tuple once, then lower each binding to typed field access.
    fn destructure(
        &mut self,
        pattern: &ast::Pattern,
        annotation: Option<&Type>,
        value: &ast::Expr,
        span: Span,
        depth: usize,
        statements: &mut Vec<ir::Stmt>,
    ) -> Checked<Type> {
        if let Some(ty) = annotation {
            self.registry
                .validate(ty, &self.inference.template_names, span)?;
        }
        let value = self.expression_expected(value, annotation, depth)?;
        if let Some(ty) = annotation {
            self.inference
                .unify(&value.ty, ty, span, "let annotation")?;
        }
        if value.ty == Type::Never {
            statements.push(ir::Stmt::Expr(value));
            return Ok(Type::Never);
        }
        let checked = self.pattern(pattern, &value.ty, &mut HashSet::new(), 0)?;
        let ty = self.inference.resolve(&value.ty, span)?;
        iteration::irrefutable(&checked, &ty, span, self.registry)?;
        let id = self.bind("_", ty.clone());
        statements.push(ir::Stmt::Let { id, value });
        Self::destructure_fields(
            &checked,
            ir::Expr {
                kind: ir::ExprKind::Local(id),
                ty,
                span,
            },
            statements,
            self.registry,
            0,
        )?;
        Ok(Type::Unit)
    }

    /// Project nested tuple bindings without reevaluating their original initializer.
    fn destructure_fields(
        pattern: &ir::Pattern,
        value: ir::Expr,
        statements: &mut Vec<ir::Stmt>,
        registry: &nominal::Registry,
        depth: usize,
    ) -> Checked<()> {
        if depth >= MAX_EXPR_DEPTH {
            return Err(Diagnostic::new(
                value.span,
                "tuple binding depth limit exceeded",
            ));
        }
        match pattern {
            ir::Pattern::List { .. } | ir::Pattern::TupleRest { .. } => {
                sequences::destructure(pattern, value, statements, registry, depth)?;
            }
            ir::Pattern::Bind(id) => statements.push(ir::Stmt::Let { id: *id, value }),
            ir::Pattern::Wildcard => reject_discard(&value, registry)?,
            ir::Pattern::Tuple(patterns) if patterns.is_empty() && value.ty == Type::Unit => {}
            ir::Pattern::Tuple(patterns) => {
                let Type::Tuple(types) = &value.ty else {
                    return Err(Diagnostic::new(
                        value.span,
                        "tuple binding requires a tuple",
                    ));
                };
                for (index, (pattern, ty)) in patterns.iter().zip(types).enumerate() {
                    let field = ir::Expr {
                        kind: ir::ExprKind::Field {
                            value: Box::new(value.clone()),
                            index,
                        },
                        ty: ty.clone(),
                        span: value.span,
                    };
                    Self::destructure_fields(pattern, field, statements, registry, depth + 1)?;
                }
            }
            _ => {
                return Err(Diagnostic::new(
                    value.span,
                    "let patterns must be irrefutable; use match for literals or constructors",
                ))
            }
        }
        Ok(())
    }

    /// Check `op` and `value` at `depth`, returning a concretely constrained unary node.
    fn unary(&mut self, op: ast::UnaryOp, value: &ast::Expr, depth: usize) -> Checked<TypedKind> {
        let value = self.expression(value, depth)?;
        let ty = match op {
            ast::UnaryOp::Negate
                if self.inference.resolve(&value.ty, value.span)? == Type::Float =>
            {
                Type::Float
            }
            ast::UnaryOp::Negate if self.inference.probing => value.ty.clone(),
            ast::UnaryOp::Negate | ast::UnaryOp::BitNot => Type::Int,
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
            Power | Subtract | Multiply | Divide | Remainder | Lt | Le | Gt | Ge => {
                let numeric = self.numeric_domain(op, &left.ty, left.span)?;
                self.inference
                    .unify(&left.ty, &numeric, left.span, "binary operator")?;
                if matches!(op, Lt | Le | Gt | Ge) {
                    Type::Bool
                } else {
                    numeric
                }
            }
            BitAnd | BitOr | BitXor | ShiftLeft | ShiftRight => {
                self.inference
                    .unify(&left.ty, &Type::Int, left.span, "bitwise operator")?;
                Type::Int
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
        expected: Option<&Type>,
        depth: usize,
    ) -> Checked<TypedKind> {
        let condition = self.expression(condition, depth)?;
        self.inference
            .unify(&condition.ty, &Type::Bool, condition.span, "if condition")?;
        let expected = if else_branch.is_some() {
            expected
        } else {
            None
        };
        let then_branch = self
            .expression_expected(then_branch, expected, depth)
            .map_err(|e| closures::context(e, "if branch"))?;
        let else_branch = else_branch
            .map(|branch| {
                self.expression_expected(
                    branch,
                    expected.or(if then_branch.ty == Type::Never {
                        None
                    } else {
                        Some(&then_branch.ty)
                    }),
                    depth,
                )
                .map_err(|e| closures::context(e, "if branch"))
            })
            .transpose()?;
        let ty = if let Some(other) = &else_branch {
            self.inference
                .unify(&other.ty, &then_branch.ty, other.span, "if branch")?;
            if then_branch.ty == Type::Never {
                other.ty.clone()
            } else {
                then_branch.ty.clone()
            }
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
        } else if runtime::lookup(name).is_some() {
            self.runtime_signature(name, span)
        } else if let Some(signature) = self.signatures.get(name) {
            let values = signature
                .generics
                .iter()
                .map(|n| (n.clone(), self.inference.fresh()))
                .collect();
            let result = returns::call_result(&mut self.inference, signature, &values, span)?;
            Ok((
                ir::CallTarget::Function(signature.id),
                signature
                    .params
                    .iter()
                    .map(|ty| nominal::substitute(ty, &values))
                    .collect::<Checked<Vec<_>>>()?,
                result,
            ))
        } else if let Some(omission) = runtime::omissions()
            .iter()
            .find(|entry| entry.names.contains(&name))
        {
            Err(Diagnostic::new(
                span,
                format!("runtime API '{name}' is unsupported: {}", omission.reason),
            ))
        } else {
            Err(Diagnostic::new(span, format!("unknown function '{name}'")))
        }
    }

    /// Instantiate audited runtime signatures only when their ABI lowering is available.
    fn runtime_signature(
        &mut self,
        name: &str,
        span: Span,
    ) -> Checked<(ir::CallTarget, Vec<Type>, Type)> {
        let id = runtime::resolve(name)
            .ok_or_else(|| Diagnostic::new(span, "missing runtime registry identity"))?;
        let signature = runtime::signature(id)
            .ok_or_else(|| Diagnostic::new(span, "missing runtime registry signature"))?;
        if signature.return_abi == runtime::ValueAbi::NullableStringList
            || signature
                .parameter_abi
                .contains(&runtime::ValueAbi::NullableStringList)
        {
            return Err(Diagnostic::new(
                span,
                format!("runtime API '{name}' requires an unavailable representation adapter"),
            ));
        }
        let generics = nominal::generics(
            signature
                .parameters
                .iter()
                .cloned()
                .chain(std::iter::once(signature.return_type.clone())),
        );
        let values = generics
            .into_iter()
            .map(|name| (name, self.inference.fresh()))
            .collect();
        let params = signature
            .parameters
            .iter()
            .map(|ty| nominal::substitute(ty, &values))
            .collect::<Checked<Vec<_>>>()?;
        let result = nominal::substitute(&signature.return_type, &values)?;
        Ok((ir::CallTarget::Runtime(id), params, result))
    }

    /// Resolve callable `name` and unify each argument with its instantiated signature.
    fn call(
        &mut self,
        name: &str,
        args: &[ast::Expr],
        span: Span,
        depth: usize,
    ) -> Checked<TypedKind> {
        self.call_expected(name, args, None, span, depth)
    }

    /// Check a one-payload sum constructor; absent variants receive fresh variables.
    fn construct(
        &mut self,
        constructor: Constructor,
        args: &[ast::Expr],
        expected: Option<&Type>,
        span: Span,
        depth: usize,
    ) -> Checked<TypedKind> {
        if args.len() != 1 {
            return Err(Diagnostic::new(span, "constructor expects one argument"));
        }
        let payload = self.inference.fresh();
        let other = self.inference.fresh();
        let ty = match constructor {
            Constructor::Some => Type::Option(Box::new(payload.clone())),
            Constructor::Ok => Type::Result(Box::new(payload.clone()), Box::new(other)),
            Constructor::Err => Type::Result(Box::new(other), Box::new(payload.clone())),
            Constructor::None => {
                return Err(Diagnostic::new(
                    span,
                    "None must be written without arguments",
                ))
            }
        };
        self.constrain_result(&ty, expected, span)?;
        let value = self.expression_expected(&args[0], Some(&payload), depth)?;
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
            Print | Println => (vec![item], Type::Unit),
            StringConcat => (vec![Type::String, Type::String], Type::String),
            StringEq => (vec![Type::String, Type::String], Type::Bool),
            StringLen => (vec![Type::String], Type::Int),
            ListEnumerate => (
                vec![list],
                Type::List(Box::new(Type::Tuple(vec![Type::Int, item]))),
            ),
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
            MapNew | MapGet | MapPut | MapDelete | MapLen | MapIsEmpty | MapContains | MapKeys
            | MapValues => self.map_signature(builtin),
            other => self.higher_order_signature(other),
        }
    }

    /// Check every nested pattern and guard in its own lexical arm scope.
    fn matching(
        &mut self,
        value: &ast::Expr,
        arms: &[ast::MatchArm],
        expected: Option<&Type>,
        span: Span,
        depth: usize,
    ) -> Checked<TypedKind> {
        if arms.is_empty() {
            return Err(Diagnostic::new(span, "match must be exhaustive"));
        }
        let value = self.expression(value, depth)?;
        let ty = expected.cloned().unwrap_or_else(|| self.inference.fresh());
        let mut checked = Vec::new();
        for arm in arms {
            self.scopes.push(HashMap::new());
            let pattern = self.pattern(&arm.pattern, &value.ty, &mut HashSet::new(), 0)?;
            let guard = arm
                .guard
                .as_ref()
                .map(|g| self.expression(g, depth))
                .transpose()?;
            if let Some(guard) = &guard {
                self.inference
                    .unify(&guard.ty, &Type::Bool, guard.span, "match guard")?;
            }
            let body = self
                .expression_expected(&arm.body, Some(&ty), depth)
                .map_err(|e| closures::context(e, "match branch"))?;
            self.scopes.pop();
            self.inference
                .unify(&body.ty, &ty, body.span, "match branch")?;
            checked.push(ir::MatchArm {
                pattern,
                guard,
                body,
                span: arm.span,
            });
        }
        let ty = if checked.iter().all(|arm| arm.body.ty == Type::Never) {
            Type::Never
        } else {
            ty
        };
        Ok((
            ir::ExprKind::Match {
                value: Box::new(value),
                arms: checked,
            },
            ty,
        ))
    }

    /// Check recursively nested patterns, rejecting duplicate binders within one arm.
    fn pattern(
        &mut self,
        pattern: &ast::Pattern,
        ty: &Type,
        names: &mut HashSet<String>,
        depth: usize,
    ) -> Checked<ir::Pattern> {
        use ast::PatternKind::*;
        if depth >= MAX_EXPR_DEPTH {
            return Err(Diagnostic::new(
                pattern.span,
                "pattern nesting limit exceeded",
            ));
        }
        let (checked, expected) = match &pattern.kind {
            Tuple(_) | List { .. } | TupleRest { .. } => {
                return self.sequence_pattern(pattern, ty, names, depth)
            }
            Wildcard => return Ok(ir::Pattern::Wildcard),
            Bind(name) => {
                self.pattern_name(name, names, pattern.span)?;
                return Ok(ir::Pattern::Bind(self.bind(name, ty.clone())));
            }
            Int(n) => (ir::Pattern::Int(*n), Type::Int),
            Bool(b) => (ir::Pattern::Bool(*b), Type::Bool),
            String(s) => (ir::Pattern::String(s.clone()), Type::String),
            Constructor {
                constructor,
                binding,
            } => {
                if let Some(name) = binding {
                    self.pattern_name(name, names, pattern.span)?;
                }
                return self.constructor_pattern(
                    *constructor,
                    binding.as_deref(),
                    ty,
                    pattern.span,
                );
            }
            NamedConstructor { name, fields } => {
                return self.named_pattern(name, fields, ty, names, pattern.span, depth + 1)
            }
        };
        self.inference
            .unify(ty, &expected, pattern.span, "match pattern")?;
        Ok(checked)
    }

    /// Record a name introduced within a pattern, allowing shadowing only across scopes.
    fn pattern_name(&self, name: &str, names: &mut HashSet<String>, span: Span) -> Checked<()> {
        if name != "_" && !names.insert(name.into()) {
            return Err(Diagnostic::new(span, "duplicate pattern binding"));
        }
        Ok(())
    }

    /// Instantiate and check a named custom or builtin constructor's nested fields.
    fn named_pattern(
        &mut self,
        name: &str,
        fields: &[ast::Pattern],
        ty: &Type,
        names: &mut HashSet<String>,
        span: Span,
        depth: usize,
    ) -> Checked<ir::Pattern> {
        let (expected, tag, payload) = self.pattern_signature(name, span)?;
        self.inference
            .unify(ty, &expected, span, "constructor pattern")?;
        if fields.len() != payload.len() {
            return Err(Diagnostic::new(
                span,
                "constructor pattern field arity mismatch",
            ));
        }
        let fields = fields
            .iter()
            .zip(payload)
            .map(|(p, t)| self.pattern(p, &t, names, depth))
            .collect::<Checked<Vec<_>>>()?;
        Ok(ir::Pattern::Variant { tag, fields })
    }

    /// Instantiate a nominal constructor or the common builtin sum tag convention.
    fn pattern_signature(&mut self, name: &str, span: Span) -> Checked<(Type, usize, Vec<Type>)> {
        if let Some((owner, params, tag, fields)) = self.registry.constructor(name) {
            let args: Vec<_> = params.iter().map(|_| self.inference.fresh()).collect();
            let values = params.into_iter().zip(args.iter().cloned()).collect();
            return Ok((
                Type::Named(owner, args),
                tag,
                fields
                    .iter()
                    .map(|t| nominal::substitute(t, &values))
                    .collect::<Checked<Vec<_>>>()?,
            ));
        }
        let a = self.inference.fresh();
        let b = self.inference.fresh();
        match name {
            "Some" => Ok((Type::Option(Box::new(a.clone())), 0, vec![a])),
            "None" => Ok((Type::Option(Box::new(a)), 1, vec![])),
            "Ok" => Ok((Type::Result(Box::new(a.clone()), Box::new(b)), 0, vec![a])),
            "Err" => Ok((Type::Result(Box::new(a), Box::new(b.clone())), 1, vec![b])),
            _ => Err(Diagnostic::new(
                span,
                format!("unknown constructor '{name}'"),
            )),
        }
    }

    /// Check a custom constructor call against fresh nominal type parameters.
    fn custom_construct(
        &mut self,
        name: &str,
        args: &[ast::Expr],
        expected: Option<&Type>,
        span: Span,
        depth: usize,
    ) -> Checked<TypedKind> {
        let (ty, tag, fields) = self.pattern_signature(name, span)?;
        self.constrain_result(&ty, expected, span)?;
        if args.len() != fields.len() {
            return Err(Diagnostic::new(
                span,
                format!("constructor '{name}' expects {} arguments", fields.len()),
            ));
        }
        let mut checked = Vec::new();
        for (arg, field) in args.iter().zip(fields) {
            let arg = self.expression_expected(arg, Some(&field), depth)?;
            self.inference
                .unify(&arg.ty, &field, arg.span, "constructor argument")?;
            checked.push(arg);
        }
        Ok((
            ir::ExprKind::CustomConstruct {
                tag,
                fields: checked,
            },
            ty,
        ))
    }

    /// Evaluate a source field receiver once before resolving its semantic layout.
    fn source_field(&mut self, expr: &ast::Expr, depth: usize) -> Checked<TypedKind> {
        let ast::ExprKind::Field { value, name } = &expr.kind else {
            return Err(Diagnostic::new(expr.span, "invalid field expression"));
        };
        let value = self.expression(value, depth)?;
        self.field(value, name, expr.span)
    }

    /// Resolve a record field once, retaining its index and instantiated semantic type.
    fn field(&mut self, value: ir::Expr, name: &str, span: Span) -> Checked<TypedKind> {
        returns::shape_ready(&self.inference, &value.ty, span)?;
        if value.ty == Type::Never {
            return Ok((value.kind, Type::Never));
        }
        let ty = self.inference.resolve(&value.ty, span)?;
        if let Type::Tuple(fields) = &ty {
            let index = name
                .parse::<usize>()
                .map_err(|_| Diagnostic::new(span, "tuple field must be a numeric index"))?;
            let field = fields
                .get(index)
                .cloned()
                .ok_or_else(|| Diagnostic::new(span, "tuple field index is out of range"))?;
            return Ok((
                ir::ExprKind::Field {
                    value: Box::new(value),
                    index,
                },
                field,
            ));
        }
        let layout = self.registry.layout(&ty, span)?;
        let index = layout
            .fields
            .iter()
            .position(|field| field == name)
            .ok_or_else(|| Diagnostic::new(span, format!("unknown record field '{name}'")))?;
        Ok((
            ir::ExprKind::Field {
                value: Box::new(value),
                index,
            },
            layout.variants[0][index].clone(),
        ))
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
            ir::ExprKind::Range { start, end, .. } => {
                self.finalize(start)?;
                self.finalize(end)?;
            }
            ir::ExprKind::For {
                pattern,
                iterable,
                body,
            } => self.finalize_for(pattern, iterable, body)?,
            ir::ExprKind::With {
                steps,
                body,
                handlers,
            } => self.finalize_with(steps, body, handlers)?,
            ir::ExprKind::Return(value) => self.finalize(value)?,
            ir::ExprKind::Defer(value) => self.finalize_defer(value)?,
            ir::ExprKind::Lambda {
                params,
                captures,
                body,
                ..
            } => self.finalize_lambda(params, captures, body)?,
            ir::ExprKind::FunctionValue { target } => {
                self.validate_function_value(*target, &expr.ty, expr.span)?
            }
            ir::ExprKind::Invoke { callee, args } => {
                self.finalize(callee)?;
                for arg in args {
                    self.finalize(arg)?;
                }
            }
            ir::ExprKind::Unary { value, .. }
            | ir::ExprKind::Try(value)
            | ir::ExprKind::Field { value, .. } => self.finalize(value)?,
            ir::ExprKind::Binary { op, left, right } => {
                self.finalize_binary(*op, left, right, &expr.ty, expr.span)?
            }
            ir::ExprKind::Call { target, args } => {
                for arg in args.iter_mut() {
                    self.finalize(arg)?;
                }
                validate_builtin(*target, args, expr.span)?;
            }
            ir::ExprKind::Map(entries) => self.finalize_map(entries)?,
            ir::ExprKind::Interpolate(values) => self.finalize_interpolation(values)?,
            ir::ExprKind::Tuple(values)
            | ir::ExprKind::List(values)
            | ir::ExprKind::CustomConstruct { fields: values, .. } => {
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
            } => self.finalize_if(condition, then_branch, else_branch)?,
            ir::ExprKind::Match { value, arms } => self.finalize_match(value, arms, expr.span)?,
            ir::ExprKind::Block(stmts) => self.finalize_block(stmts)?,
            _ => {}
        }
        Ok(())
    }

    /// Normalize match operands before bounded usefulness and exhaustiveness validation.
    fn finalize_match(
        &self,
        value: &mut ir::Expr,
        arms: &mut [ir::MatchArm],
        span: Span,
    ) -> Checked<()> {
        self.finalize(value)?;
        for arm in arms.iter_mut() {
            sequences::reject_discards(&arm.pattern, &value.ty, arm.span, self.registry)?;
            if let Some(guard) = &mut arm.guard {
                self.finalize(guard)?;
            }
            self.finalize(&mut arm.body)?;
        }
        coverage::validate(&value.ty, arms, self.registry, span)
    }

    /// Finalize lazy branches and enforce discarded Result obligations on no-else paths.
    fn finalize_if(
        &self,
        condition: &mut ir::Expr,
        then_branch: &mut ir::Expr,
        else_branch: &mut Option<Box<ir::Expr>>,
    ) -> Checked<()> {
        self.finalize(condition)?;
        self.finalize(then_branch)?;
        if let Some(branch) = else_branch {
            self.finalize(branch)?;
        } else {
            reject_discard(then_branch, self.registry)?;
        }
        Ok(())
    }

    /// Validate overloaded scalar operators only after both operands have concrete types.
    fn finalize_binary(
        &self,
        op: ast::BinaryOp,
        left: &mut ir::Expr,
        right: &mut ir::Expr,
        result: &Type,
        span: Span,
    ) -> Checked<()> {
        self.finalize(left)?;
        self.finalize(right)?;
        if matches!(op, ast::BinaryOp::And | ast::BinaryOp::Or) && right.ty == Type::Never {
            return Ok(());
        }
        if binary_result(op, &left.ty, &right.ty).as_ref() != Some(result) {
            return Err(Diagnostic::new(
                span,
                "binary operator does not support these operand types",
            ));
        }
        Ok(())
    }

    /// Resolve interpolated expressions before validating their printable scalar types.
    fn finalize_interpolation(&self, values: &mut [ir::Expr]) -> Checked<()> {
        for value in values {
            self.finalize(value)?;
            if !matches!(
                value.ty,
                Type::Int | Type::Float | Type::Bool | Type::String
            ) {
                return Err(Diagnostic::new(
                    value.span,
                    "interpolation requires Int, Float, Bool, or String",
                ));
            }
        }
        Ok(())
    }

    /// Normalize block statements and reject non-final discarded Result expressions.
    fn finalize_block(&self, stmts: &mut [ir::Stmt]) -> Checked<()> {
        let len = stmts.len();
        for (index, stmt) in stmts.iter_mut().enumerate() {
            match stmt {
                ir::Stmt::LetElse {
                    pattern,
                    value,
                    else_branch,
                } => {
                    self.finalize(value)?;
                    self.finalize(else_branch)?;
                    control::pattern_discards(pattern, &value.ty, value.span, self.registry)?;
                }
                ir::Stmt::Let { value, .. } => self.finalize(value)?,
                ir::Stmt::Expr(value) => {
                    self.finalize(value)?;
                    if index + 1 != len {
                        reject_discard(value, self.registry)?;
                    }
                }
            }
        }
        Ok(())
    }
}

/// Reject ignored fallible values; binding, returning or consuming them is explicit handling.
fn reject_discard(expr: &ir::Expr, registry: &nominal::Registry) -> Checked<()> {
    if registry.contains_result(&expr.ty)? {
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
        ir::CallTarget::Runtime(id) => {
            let signature = runtime::signature(id)
                .ok_or_else(|| Diagnostic::new(span, "invalid runtime registry identity"))?;
            if signature.operation == runtime::Operation::ScalarContains
                && !scalar(&args[1].ty)
                && args[1].ty != Type::Float
            {
                return Err(Diagnostic::new(
                    span,
                    "runtime contains requires scalar Int, Float, Bool, or String elements",
                ));
            }
            Ok(())
        }
        ir::CallTarget::Builtin(ir::Builtin::Print | ir::Builtin::Println)
            if !scalar(&args[0].ty) && args[0].ty != Type::Float =>
        {
            Err(Diagnostic::new(
                span,
                "print argument must be Int, Bool, String, or Float",
            ))
        }
        ir::CallTarget::Builtin(ir::Builtin::ListContains)
            if !scalar(&args[1].ty) && args[1].ty != Type::Float =>
        {
            Err(Diagnostic::new(
                span,
                "List.contains requires scalar Int, Float, Bool, or String elements",
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
pub(crate) fn builtin(name: &str) -> Option<ir::Builtin> {
    use ir::Builtin::*;
    Some(match name {
        "Map.new" => MapNew,
        "Map.get" => MapGet,
        "Map.put" => MapPut,
        "Map.delete" => MapDelete,
        "Map.len" => MapLen,
        "Map.is_empty" => MapIsEmpty,
        "Map.contains" => MapContains,
        "Map.keys" => MapKeys,
        "Map.values" => MapValues,

        "print" => Print,
        "println" => Println,
        "String.concat" => StringConcat,
        "String.eq" => StringEq,
        "String.len" => StringLen,
        "List.enumerate" => ListEnumerate,
        "List.map" => ListMap,
        "List.fold" => ListFold,
        "List.filter" => ListFilter,
        "List.find" => ListFind,
        "List.any" => ListAny,
        "List.all" => ListAll,
        "Option.map" => OptionMap,
        "Result.map" => ResultMap,
        "Result.and_then" => ResultAndThen,
        "Result.unwrap_or_else" => ResultUnwrapOrElse,
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
        Add | Subtract | Multiply | Divide | Remainder | Power | BitAnd | BitOr | BitXor
        | ShiftLeft | ShiftRight
            if *left == Type::Int =>
        {
            Some(Type::Int)
        }
        Add | Subtract | Multiply | Divide | Power if *left == Type::Float => Some(Type::Float),
        Eq | Ne if scalar(left) || *left == Type::Float => Some(Type::Bool),
        Lt | Le | Gt | Ge if matches!(left, Type::Int | Type::Float) => Some(Type::Bool),
        And | Or if *left == Type::Bool => Some(Type::Bool),
        _ => None,
    }
}

/// Reject unconsumed fallible bindings, including custom fields and guarded patterns.
fn reject_unused_results(
    expr: &ir::Expr,
    params: &[ir::Param],
    registry: &nominal::Registry,
) -> Checked<()> {
    let mut pending = vec![expr];
    let mut referenced = HashSet::new();
    let mut fallible = Vec::new();
    for param in params {
        if registry.contains_result(&param.ty)? {
            fallible.push((param.id.0, expr.span));
        }
    }
    while let Some(expr) = pending.pop() {
        if let ir::ExprKind::Local(id) = &expr.kind {
            referenced.insert(id.0);
        }
        if let ir::ExprKind::Block(stmts) = &expr.kind {
            for stmt in stmts {
                if let ir::Stmt::LetElse { pattern, value, .. } = stmt {
                    fallible_bindings(pattern, &value.ty, value.span, registry, &mut fallible)?;
                }
                if let ir::Stmt::Let { id, value } = stmt {
                    if registry.contains_result(&value.ty)? {
                        fallible.push((id.0, value.span));
                    }
                }
            }
        }
        iteration::collect_bindings(expr, registry, &mut fallible)?;
        if let ir::ExprKind::Match { value, arms } = &expr.kind {
            for arm in arms {
                fallible_bindings(&arm.pattern, &value.ty, arm.span, registry, &mut fallible)?;
            }
        }
        if let ir::ExprKind::Lambda { captures, .. } = &expr.kind {
            pending.extend(captures.iter().map(|capture| &capture.value));
        } else {
            pending.extend(nominal::children(expr));
        }
    }
    for (id, span) in fallible {
        if !referenced.contains(&id) {
            return Err(Diagnostic::new(
                span,
                "Result value must be handled; this Result binding is never used",
            ));
        }
    }
    Ok(())
}

/// Collect fallible named bindings through nested constructor fields.
fn fallible_bindings(
    pattern: &ir::Pattern,
    ty: &Type,
    span: Span,
    registry: &nominal::Registry,
    bindings: &mut Vec<(usize, Span)>,
) -> Checked<()> {
    match pattern {
        ir::Pattern::List { .. } | ir::Pattern::TupleRest { .. } => {
            for (p, t) in sequences::parts(pattern, ty, span)? {
                fallible_bindings(p, &t, span, registry, bindings)?;
            }
        }
        ir::Pattern::Bind(id) => {
            if registry.contains_result(ty)? {
                bindings.push((id.0, span));
            }
        }
        ir::Pattern::Tuple(fields) if fields.is_empty() && *ty == Type::Unit => {}
        ir::Pattern::Tuple(fields) => {
            let variants = registry.variants(ty, span)?;
            for (pattern, ty) in fields.iter().zip(&variants[0]) {
                fallible_bindings(pattern, ty, span, registry, bindings)?;
            }
        }
        ir::Pattern::Variant { tag, fields } => {
            let variants = registry.variants(ty, span)?;
            for (pattern, ty) in fields.iter().zip(&variants[*tag]) {
                fallible_bindings(pattern, ty, span, registry, bindings)?;
            }
        }
        ir::Pattern::Constructor {
            constructor,
            binding: Some(id),
        } => {
            let tag = if matches!(constructor, Constructor::Some | Constructor::Ok) {
                0
            } else {
                1
            };
            let variants = registry.variants(ty, span)?;
            if registry.contains_result(&variants[tag][0])? {
                bindings.push((id.0, span));
            }
        }
        _ => {}
    }
    Ok(())
}
