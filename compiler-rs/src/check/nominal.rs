//! Nominal declarations, concrete layouts and bounded type substitution.
use super::{validate_type, Checked, MAX_TYPE_DEPTH, MAX_TYPE_NODES};
use crate::{ast, ir, Diagnostic, Span, Type};
use std::collections::{BTreeSet, HashMap, HashSet};

pub(super) struct Registry {
    declarations: HashMap<String, ast::TypeDecl>,
    constructors: HashMap<String, (String, usize)>,
}

impl Registry {
    /// Validate all forward declarations before checking individual fields.
    pub(super) fn new(program: &ast::Program) -> Checked<Self> {
        let mut result = Self {
            declarations: HashMap::new(),
            constructors: HashMap::new(),
        };
        for decl in &program.types {
            if super::reserved(&decl.name)
                || result
                    .declarations
                    .insert(decl.name.clone(), decl.clone())
                    .is_some()
            {
                return Err(Diagnostic::new(
                    decl.span,
                    "duplicate or reserved type name",
                ));
            }
        }
        for decl in &program.types {
            result.declaration(decl)?;
        }
        Ok(result)
    }

    /// Check a declaration's shape, parameters, fields and constructor namespace.
    fn declaration(&mut self, decl: &ast::TypeDecl) -> Checked<()> {
        let allowed: HashSet<_> = decl.parameters.iter().cloned().collect();
        if allowed.len() != decl.parameters.len() || decl.variants.is_empty() {
            return Err(Diagnostic::new(
                decl.span,
                "duplicate type parameter or empty type declaration",
            ));
        }
        if decl.record && decl.variants.len() != 1 {
            return Err(Diagnostic::new(
                decl.span,
                "record must have exactly one constructor",
            ));
        }
        let mut variants = HashSet::new();
        for (tag, variant) in decl.variants.iter().enumerate() {
            if !variants.insert(&variant.name) {
                return Err(Diagnostic::new(variant.span, "duplicate constructor"));
            }
            let mut fields = HashSet::new();
            for field in &variant.fields {
                self.validate(&field.ty, &allowed, field.span)?;
                if let Some(name) = &field.name {
                    if !fields.insert(name) {
                        return Err(Diagnostic::new(field.span, "duplicate record field"));
                    }
                } else if decl.record {
                    return Err(Diagnostic::new(field.span, "record fields require names"));
                }
            }
            self.add_constructor(&variant.name, &decl.name, tag, variant.span)?;
            let leaf = variant.name.rsplit('.').next().unwrap_or(&variant.name);
            let qualified = format!("{}.{leaf}", decl.name);
            if qualified != variant.name {
                self.add_constructor(&qualified, &decl.name, tag, variant.span)?;
            }
            if decl.record && variant.name != decl.name {
                self.add_constructor(&decl.name, &decl.name, tag, variant.span)?;
            }
        }
        Ok(())
    }

    /// Insert one constructor alias while rejecting ambiguous or builtin names.
    fn add_constructor(&mut self, name: &str, owner: &str, tag: usize, span: Span) -> Checked<()> {
        if super::reserved(name) {
            return Err(Diagnostic::new(span, "constructor name is reserved"));
        }
        if let Some(existing) = self.constructors.get(name) {
            if existing != &(owner.to_owned(), tag) {
                return Err(Diagnostic::new(span, "ambiguous duplicate constructor"));
            }
        } else {
            self.constructors.insert(name.into(), (owner.into(), tag));
        }
        Ok(())
    }

    /// Validate nominal arities and generic names in an already bounded annotation.
    pub(super) fn validate(&self, ty: &Type, allowed: &HashSet<String>, span: Span) -> Checked<()> {
        validate_type(ty, span)?;
        let mut pending = vec![ty];
        while let Some(ty) = pending.pop() {
            match ty {
                Type::Named(name, args) => {
                    let decl = self
                        .declarations
                        .get(name)
                        .ok_or_else(|| Diagnostic::new(span, format!("unknown type '{name}'")))?;
                    if args.len() != decl.parameters.len() {
                        return Err(Diagnostic::new(
                            span,
                            format!(
                                "type '{name}' expects {} type arguments",
                                decl.parameters.len()
                            ),
                        ));
                    }
                    pending.extend(args);
                }
                Type::Generic(name) if !allowed.contains(name) => {
                    return Err(Diagnostic::new(
                        span,
                        format!("undeclared generic type '{name}'"),
                    ))
                }
                Type::Function(args, result) => {
                    pending.extend(args);
                    pending.push(result);
                }
                Type::Tuple(args) => pending.extend(args),
                Type::List(t) | Type::Option(t) => pending.push(t),
                Type::Result(a, b) | Type::Map(a, b) => {
                    pending.push(a);
                    pending.push(b);
                }
                _ => {}
            }
        }
        Ok(())
    }

    /// Return owned constructor metadata, allowing the caller to allocate fresh variables.
    pub(super) fn constructor(
        &self,
        name: &str,
    ) -> Option<(String, Vec<String>, usize, Vec<Type>)> {
        let (owner, tag) = self.constructors.get(name)?;
        let decl = &self.declarations[owner];
        Some((
            owner.clone(),
            decl.parameters.clone(),
            *tag,
            decl.variants[*tag]
                .fields
                .iter()
                .map(|f| f.ty.clone())
                .collect(),
        ))
    }

    /// Instantiate one nominal layout without recursively expanding referenced types.
    pub(super) fn layout(&self, ty: &Type, span: Span) -> Checked<ir::TypeLayout> {
        validate_layout_type(ty, span)?;
        let Type::Named(name, args) = ty else {
            return Err(Diagnostic::new(span, "field access requires a record type"));
        };
        let decl = self
            .declarations
            .get(name)
            .ok_or_else(|| Diagnostic::new(span, format!("unknown type '{name}'")))?;
        if args.len() != decl.parameters.len() {
            return Err(Diagnostic::new(span, "wrong nominal type argument count"));
        }
        let substitutions = decl
            .parameters
            .iter()
            .cloned()
            .zip(args.iter().cloned())
            .collect();
        let variants = decl
            .variants
            .iter()
            .map(|v| {
                v.fields
                    .iter()
                    .map(|f| substitute(&f.ty, &substitutions))
                    .collect::<Checked<Vec<_>>>()
            })
            .collect::<Checked<Vec<_>>>()?;
        let fields = if decl.record {
            decl.variants[0]
                .fields
                .iter()
                .map(|f| f.name.clone().unwrap_or_default())
                .collect()
        } else {
            Vec::new()
        };
        Ok(ir::TypeLayout {
            ty: ty.clone(),
            variants,
            fields,
        })
    }

    /// Resolve constructor payload types for builtin and nominal sums.
    pub(super) fn variants(&self, ty: &Type, span: Span) -> Checked<Vec<Vec<Type>>> {
        match ty {
            Type::Tuple(fields) => Ok(vec![fields.clone()]),
            Type::Option(a) => Ok(vec![vec![(**a).clone()], vec![]]),
            Type::Result(a, b) => Ok(vec![vec![(**a).clone()], vec![(**b).clone()]]),
            Type::Named(..) => Ok(self.layout(ty, span)?.variants),
            _ => Err(Diagnostic::new(
                span,
                "constructor pattern requires a sum or record type",
            )),
        }
    }

    /// Find hidden Result values while terminating on ordinary recursive nominal types.
    pub(super) fn contains_result(&self, ty: &Type) -> Checked<bool> {
        let mut pending = vec![ty.clone()];
        let mut seen = HashSet::new();
        while let Some(ty) = pending.pop() {
            if seen.len() >= MAX_TYPE_NODES {
                return Err(Diagnostic::new(
                    Span::default(),
                    "type expansion limit exceeded",
                ));
            }
            if !seen.insert(ty.clone()) {
                continue;
            }
            match ty {
                Type::Result(..) => return Ok(true),
                Type::Map(key, value) => {
                    pending.push(*key);
                    pending.push(*value);
                }
                Type::Tuple(fields) => pending.extend(fields),
                Type::List(a) | Type::Option(a) => pending.push(*a),
                Type::Named(..) => pending.extend(
                    self.layout(&ty, Span::default())?
                        .variants
                        .into_iter()
                        .flatten(),
                ),
                _ => {}
            }
        }
        Ok(false)
    }

    /// Collect every concrete layout reachable from emitted function types and values.
    pub(super) fn layouts(&self, functions: &[ir::Function]) -> Checked<Vec<ir::TypeLayout>> {
        let mut pending = Vec::new();
        for function in functions {
            pending.push(function.return_type.clone());
            pending.extend(function.params.iter().map(|p| p.ty.clone()));
            let mut expressions = vec![&function.body];
            while let Some(expr) = expressions.pop() {
                pending.push(expr.ty.clone());
                expressions.extend(children(expr));
            }
        }
        let mut seen = HashSet::new();
        let mut layouts = Vec::new();
        while let Some(ty) = pending.pop() {
            if !seen.insert(ty.clone()) {
                continue;
            }
            if seen.len() > MAX_TYPE_NODES {
                return Err(Diagnostic::new(
                    Span::default(),
                    "concrete type layout expansion limit exceeded",
                ));
            }
            match &ty {
                Type::Named(_, args) => {
                    let layout = self.layout(&ty, Span::default())?;
                    pending.extend(args.iter().cloned());
                    pending.extend(layout.variants.iter().flatten().cloned());
                    layouts.push(layout);
                }
                Type::Function(args, result) => {
                    pending.extend(args.iter().cloned());
                    pending.push((**result).clone());
                }
                Type::Tuple(fields) => pending.extend(fields.iter().cloned()),
                Type::List(a) | Type::Option(a) => pending.push((**a).clone()),
                Type::Result(a, b) | Type::Map(a, b) => {
                    pending.push((**a).clone());
                    pending.push((**b).clone());
                }
                _ => {}
            }
        }
        Ok(layouts)
    }
}

/// List child expressions, including guards, without traversing type layouts.
pub(super) fn children(expr: &ir::Expr) -> Vec<&ir::Expr> {
    match &expr.kind {
        ir::ExprKind::Lambda { captures, body, .. } => captures
            .iter()
            .map(|c| &c.value)
            .chain(std::iter::once(body.as_ref()))
            .collect(),
        ir::ExprKind::Map(entries) => entries.iter().flat_map(|(k, v)| [k, v]).collect(),
        ir::ExprKind::Closure { captures, .. } => captures.iter().collect(),
        ir::ExprKind::Invoke { callee, args } => std::iter::once(callee.as_ref())
            .chain(args.iter())
            .collect(),
        ir::ExprKind::Unary { value, .. }
        | ir::ExprKind::Try(value)
        | ir::ExprKind::Field { value, .. } => vec![value],
        ir::ExprKind::Binary { left, right, .. } => vec![left, right],
        ir::ExprKind::Call { args, .. }
        | ir::ExprKind::Interpolate(args)
        | ir::ExprKind::Tuple(args)
        | ir::ExprKind::List(args)
        | ir::ExprKind::CustomConstruct { fields: args, .. } => args.iter().collect(),
        ir::ExprKind::Construct {
            value: Some(value), ..
        } => vec![value],
        ir::ExprKind::If {
            condition,
            then_branch,
            else_branch,
        } => {
            let mut values = vec![condition.as_ref(), then_branch.as_ref()];
            values.extend(else_branch.as_deref());
            values
        }
        ir::ExprKind::Match { value, arms } => {
            let mut values = vec![value.as_ref()];
            for arm in arms {
                values.extend(arm.guard.as_ref());
                values.push(&arm.body);
            }
            values
        }
        ir::ExprKind::Block(stmts) => stmts
            .iter()
            .map(|s| match s {
                ir::Stmt::Let { value, .. } | ir::Stmt::Expr(value) => value,
            })
            .collect(),
        _ => Vec::new(),
    }
}

/// Collect generic parameter names in deterministic order from bounded annotations.
pub(super) fn generics(types: impl IntoIterator<Item = Type>) -> Vec<String> {
    let mut names = BTreeSet::new();
    let mut pending: Vec<_> = types.into_iter().collect();
    while let Some(ty) = pending.pop() {
        match ty {
            Type::Generic(n) => {
                names.insert(n);
            }
            Type::Function(args, result) => {
                pending.extend(args);
                pending.push(*result);
            }
            Type::Tuple(args) | Type::Named(_, args) => pending.extend(args),
            Type::List(a) | Type::Option(a) => pending.push(*a),
            Type::Result(a, b) | Type::Map(a, b) => {
                pending.push(*a);
                pending.push(*b);
            }
            _ => {}
        }
    }
    names.into_iter().collect()
}

/// Substitute generic annotations with a strict bound on the expanded result.
pub(super) fn substitute(ty: &Type, values: &HashMap<String, Type>) -> Checked<Type> {
    let mut budget = MAX_TYPE_NODES;
    substitute_inner(ty, values, 0, &mut budget, true)
}

/// Charge every expanded type node, including repeated generic substitutions.
fn substitute_inner(
    ty: &Type,
    values: &HashMap<String, Type>,
    depth: usize,
    budget: &mut usize,
    expand: bool,
) -> Checked<Type> {
    if depth >= MAX_TYPE_DEPTH || *budget == 0 {
        return Err(Diagnostic::new(
            Span::default(),
            "generic type expansion limit exceeded",
        ));
    }
    *budget -= 1;
    Ok(match ty {
        Type::Generic(n) if expand => match values.get(n) {
            Some(value) => substitute_inner(value, values, depth, budget, false)?,
            None => ty.clone(),
        },
        Type::Function(args, result) => Type::Function(
            args.iter()
                .map(|a| substitute_inner(a, values, depth + 1, budget, expand))
                .collect::<Checked<Vec<_>>>()?,
            Box::new(substitute_inner(result, values, depth + 1, budget, expand)?),
        ),
        Type::Tuple(args) => Type::Tuple(
            args.iter()
                .map(|a| substitute_inner(a, values, depth + 1, budget, expand))
                .collect::<Checked<Vec<_>>>()?,
        ),
        Type::Named(n, args) => Type::Named(
            n.clone(),
            args.iter()
                .map(|a| substitute_inner(a, values, depth + 1, budget, expand))
                .collect::<Checked<Vec<_>>>()?,
        ),
        Type::List(a) => Type::List(Box::new(substitute_inner(
            a,
            values,
            depth + 1,
            budget,
            expand,
        )?)),
        Type::Option(a) => Type::Option(Box::new(substitute_inner(
            a,
            values,
            depth + 1,
            budget,
            expand,
        )?)),
        Type::Map(a, b) => Type::Map(
            Box::new(substitute_inner(a, values, depth + 1, budget, expand)?),
            Box::new(substitute_inner(b, values, depth + 1, budget, expand)?),
        ),
        Type::Result(a, b) => Type::Result(
            Box::new(substitute_inner(a, values, depth + 1, budget, expand)?),
            Box::new(substitute_inner(b, values, depth + 1, budget, expand)?),
        ),
        _ => ty.clone(),
    })
}

/// Infer a template's generic arguments from concrete argument and result types.
pub(super) fn capture(
    template: &Type,
    actual: &Type,
    values: &mut HashMap<String, Type>,
    depth: usize,
) -> Checked<()> {
    if depth >= MAX_TYPE_DEPTH {
        return Err(Diagnostic::new(
            Span::default(),
            "generic specialization type depth exceeded",
        ));
    }
    match (template, actual) {
        (Type::Generic(n), ty) => {
            if let Some(previous) = values.insert(n.clone(), ty.clone()) {
                if previous != *ty {
                    return Err(Diagnostic::new(
                        Span::default(),
                        "inconsistent generic specialization",
                    ));
                }
            }
        }
        (Type::Named(n, a), Type::Named(m, b)) if n == m && a.len() == b.len() => {
            for (a, b) in a.iter().zip(b) {
                capture(a, b, values, depth + 1)?;
            }
        }
        (Type::Tuple(a), Type::Tuple(b)) if a.len() == b.len() => {
            for (a, b) in a.iter().zip(b) {
                capture(a, b, values, depth + 1)?;
            }
        }
        (Type::Function(a, ar), Type::Function(b, br)) if a.len() == b.len() => {
            for (a, b) in a.iter().zip(b) {
                capture(a, b, values, depth + 1)?;
            }
            capture(ar, br, values, depth + 1)?;
        }
        (Type::List(a), Type::List(b)) | (Type::Option(a), Type::Option(b)) => {
            capture(a, b, values, depth + 1)?
        }
        (Type::Result(a, b), Type::Result(c, d)) | (Type::Map(a, b), Type::Map(c, d)) => {
            capture(a, c, values, depth + 1)?;
            capture(b, d, values, depth + 1)?;
        }
        _ if template == actual => {}
        _ => {
            return Err(Diagnostic::new(
                Span::default(),
                "invalid concrete generic specialization",
            ))
        }
    }
    Ok(())
}

/// Bound concrete/inferred layout arguments before substituting recursive fields.
fn validate_layout_type(ty: &Type, span: Span) -> Checked<()> {
    let mut pending = vec![(ty, 0)];
    let mut nodes = 0;
    while let Some((ty, depth)) = pending.pop() {
        nodes += 1;
        if depth >= MAX_TYPE_DEPTH || nodes > MAX_TYPE_NODES {
            return Err(Diagnostic::new(
                span,
                "nominal type expansion limit exceeded",
            ));
        }
        match ty {
            Type::Tuple(args) | Type::Named(_, args) => {
                pending.extend(args.iter().map(|t| (t, depth + 1)))
            }
            Type::List(a) | Type::Option(a) => pending.push((a, depth + 1)),
            Type::Result(a, b) | Type::Map(a, b) => {
                pending.push((a, depth + 1));
                pending.push((b, depth + 1));
            }
            _ => {}
        }
    }
    Ok(())
}
