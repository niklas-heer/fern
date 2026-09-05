//! Bound caller-created syntax before recursive cloning for monomorphization.
use super::{
    validate_type, Checked, MAX_EXPR_COUNT, MAX_EXPR_DEPTH, MAX_FUNCTIONS, MAX_PARAMETERS,
};
use crate::{ast, Diagnostic, Span, Type};

struct Budget {
    nodes: usize,
    bytes: usize,
}
impl Budget {
    /// Charge a source node and its owned text before copying it.
    fn charge(&mut self, bytes: usize, span: Span) -> Checked<()> {
        self.nodes += 1;
        self.bytes = self.bytes.saturating_add(bytes);
        if self.nodes > MAX_EXPR_COUNT || self.bytes > 1024 * 1024 {
            return Err(Diagnostic::new(
                span,
                "prototype syntax size limit exceeded",
            ));
        }
        Ok(())
    }
    /// Check recursive annotations iteratively and account for their names.
    fn ty(&mut self, ty: &Type, span: Span) -> Checked<()> {
        validate_type(ty, span)?;
        let mut pending = vec![ty];
        while let Some(ty) = pending.pop() {
            match ty {
                Type::Named(n, args) => {
                    self.charge(n.len(), span)?;
                    pending.extend(args);
                }
                Type::Function(args, result) => {
                    pending.extend(args);
                    pending.push(result);
                }
                Type::Tuple(args) => pending.extend(args),
                Type::Generic(n) => self.charge(n.len(), span)?,
                Type::List(a) | Type::Option(a) => pending.push(a),
                Type::Result(a, b) => {
                    pending.push(a);
                    pending.push(b);
                }
                _ => {}
            }
        }
        Ok(())
    }
    /// Validate one bounded pattern tree, including otherwise unused generic bodies.
    fn pattern(&mut self, pattern: &ast::Pattern) -> Checked<()> {
        let mut pending = vec![(pattern, 0)];
        while let Some((pattern, depth)) = pending.pop() {
            if depth >= MAX_EXPR_DEPTH {
                return Err(Diagnostic::new(
                    pattern.span,
                    "pattern nesting limit exceeded",
                ));
            }
            let bytes = match &pattern.kind {
                ast::PatternKind::Tuple(fields) => {
                    pending.extend(fields.iter().map(|p| (p, depth + 1)));
                    0
                }
                ast::PatternKind::Bind(n) | ast::PatternKind::String(n) => n.len(),
                ast::PatternKind::Constructor { binding, .. } => {
                    binding.as_ref().map_or(0, String::len)
                }
                ast::PatternKind::NamedConstructor { name, fields } => {
                    pending.extend(fields.iter().map(|p| (p, depth + 1)));
                    name.len()
                }
                _ => 0,
            };
            self.charge(bytes, pattern.span)?;
        }
        Ok(())
    }
    /// Check block annotations and enqueue initializers without recursive traversal.
    fn block<'a>(
        &mut self,
        stmts: &'a [ast::Stmt],
        pending: &mut Vec<(&'a ast::Expr, usize)>,
        depth: usize,
    ) -> Checked<()> {
        for stmt in stmts {
            match stmt {
                ast::Stmt::Let {
                    name,
                    annotation,
                    value,
                    span,
                } => {
                    self.charge(name.len(), *span)?;
                    if let Some(ty) = annotation {
                        self.ty(ty, *span)?;
                    }
                    pending.push((value, depth + 1));
                }
                ast::Stmt::LetPattern {
                    pattern,
                    annotation,
                    value,
                    span,
                } => {
                    self.pattern(pattern)?;
                    if let Some(ty) = annotation {
                        self.ty(ty, *span)?;
                    }
                    pending.push((value, depth + 1));
                }
                ast::Stmt::Expr(value) => pending.push((value, depth + 1)),
            }
        }
        Ok(())
    }

    /// Charge interpolation text and enqueue expressions at the enclosing traversal depth.
    fn interpolation<'a>(
        &mut self,
        parts: &'a [ast::StringPart],
        pending: &mut Vec<(&'a ast::Expr, usize)>,
        depth: usize,
        span: Span,
    ) -> Checked<()> {
        for part in parts {
            match part {
                ast::StringPart::Text(text) => self.charge(text.len(), span)?,
                ast::StringPart::Value(value) => pending.push((value, depth + 1)),
            }
        }
        Ok(())
    }

    /// Check lambda annotations and duplicate names even in uninstantiated generic templates.
    fn lambda_params(&mut self, params: &[ast::LambdaParam], span: Span) -> Checked<()> {
        if params.len() > MAX_PARAMETERS {
            return Err(Diagnostic::new(span, "lambda parameter limit exceeded"));
        }
        let mut names = std::collections::HashSet::new();
        for param in params {
            if !names.insert(&param.name) {
                return Err(Diagnostic::new(param.span, "duplicate lambda parameter"));
            }
            self.charge(param.name.len(), param.span)?;
            if let Some(ty) = &param.annotation {
                self.ty(ty, param.span)?;
            }
        }
        Ok(())
    }

    /// Check patterns before enqueueing each arm body and guard at its inherited depth.
    fn match_arms<'a>(
        &mut self,
        arms: &'a [ast::MatchArm],
        pending: &mut Vec<(&'a ast::Expr, usize)>,
        depth: usize,
    ) -> Checked<()> {
        for arm in arms {
            self.pattern(&arm.pattern)?;
            pending.push((&arm.body, depth + 1));
            if let Some(guard) = &arm.guard {
                pending.push((guard, depth + 1));
            }
        }
        Ok(())
    }

    /// Charge expression nodes only after checking their current traversal depth.
    fn expression_node(&mut self, span: Span, depth: usize) -> Checked<()> {
        if depth >= MAX_EXPR_DEPTH {
            return Err(Diagnostic::new(
                span,
                "prototype expression nesting limit exceeded (128)",
            ));
        }
        self.charge(0, span)
    }

    /// Validate expression depth and annotations before source-instance cloning.
    fn expression(&mut self, expr: &ast::Expr) -> Checked<()> {
        let mut pending = vec![(expr, 0)];
        while let Some((expr, depth)) = pending.pop() {
            self.expression_node(expr.span, depth)?;
            match &expr.kind {
                ast::ExprKind::Interpolate(parts) => {
                    self.interpolation(parts, &mut pending, depth, expr.span)?
                }
                ast::ExprKind::Lambda { params, body } => {
                    self.lambda_params(params, expr.span)?;
                    pending.push((body, depth + 1));
                }
                ast::ExprKind::Apply { callee, args } => {
                    pending.push((callee, depth + 1));
                    pending.extend(args.iter().map(|a| (a, depth + 1)));
                }
                ast::ExprKind::Name(n) | ast::ExprKind::String(n) => {
                    self.charge(n.len(), expr.span)?
                }
                ast::ExprKind::Unary { value, .. } | ast::ExprKind::Try(value) => {
                    pending.push((value, depth + 1))
                }
                ast::ExprKind::Field { value, name } => {
                    self.charge(name.len(), expr.span)?;
                    pending.push((value, depth + 1));
                }
                ast::ExprKind::Binary { left, right, .. } => {
                    pending.push((left, depth + 1));
                    pending.push((right, depth + 1));
                }
                ast::ExprKind::Pipe {
                    value, name, args, ..
                } => {
                    self.charge(name.len(), expr.span)?;
                    pending.push((value, depth + 1));
                    pending.extend(args.iter().map(|e| (e, depth + 1)));
                }
                ast::ExprKind::Call { name, args } => {
                    self.charge(name.len(), expr.span)?;
                    pending.extend(args.iter().map(|e| (e, depth + 1)));
                }
                ast::ExprKind::Tuple(args) | ast::ExprKind::List(args) => {
                    pending.extend(args.iter().map(|e| (e, depth + 1)))
                }
                ast::ExprKind::If {
                    condition,
                    then_branch,
                    else_branch,
                } => {
                    pending.push((condition, depth + 1));
                    pending.push((then_branch, depth + 1));
                    if let Some(value) = else_branch {
                        pending.push((value, depth + 1));
                    }
                }
                ast::ExprKind::Match { value, arms } => {
                    pending.push((value, depth + 1));
                    self.match_arms(arms, &mut pending, depth)?;
                }
                ast::ExprKind::Block(stmts) => self.block(stmts, &mut pending, depth)?,
                _ => {}
            }
        }
        Ok(())
    }
}

/// Bound the entire source AST before registry copies and specialization work begin.
pub(super) fn check(program: &ast::Program) -> Checked<()> {
    if program.functions.len() > MAX_FUNCTIONS || program.types.len() > MAX_FUNCTIONS {
        return Err(Diagnostic::new(
            Span::default(),
            "prototype declaration count limit exceeded",
        ));
    }
    let mut budget = Budget { nodes: 0, bytes: 0 };
    for function in &program.functions {
        budget.charge(function.name.len(), function.span)?;
        for param in &function.params {
            budget.charge(param.name.len(), param.span)?;
            budget.ty(&param.ty, param.span)?;
        }
        if let Some(ty) = &function.return_type {
            budget.ty(ty, function.span)?;
        }
        budget.expression(&function.body)?;
    }
    for decl in &program.types {
        budget.charge(decl.name.len(), decl.span)?;
        if decl.parameters.len() > MAX_PARAMETERS || decl.variants.len() > MAX_PARAMETERS {
            return Err(Diagnostic::new(
                decl.span,
                "type declaration arity limit exceeded",
            ));
        }
        for name in &decl.parameters {
            budget.charge(name.len(), decl.span)?;
        }
        for variant in &decl.variants {
            budget.charge(variant.name.len(), variant.span)?;
            if variant.fields.len() > MAX_PARAMETERS {
                return Err(Diagnostic::new(
                    variant.span,
                    "constructor field count limit exceeded",
                ));
            }
            for field in &variant.fields {
                budget.charge(field.name.as_ref().map_or(0, String::len), field.span)?;
                budget.ty(&field.ty, field.span)?;
            }
        }
    }
    Ok(())
}
