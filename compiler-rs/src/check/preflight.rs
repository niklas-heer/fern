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
                Type::Result(a, b) | Type::Map(a, b) => {
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
                ast::PatternKind::List { prefix, rest } => {
                    self.sequence(prefix, rest.as_deref(), &mut pending, depth, pattern.span)?;
                    0
                }
                ast::PatternKind::TupleRest { prefix, rest } => {
                    self.sequence(prefix, Some(rest), &mut pending, depth, pattern.span)?;
                    0
                }
                ast::PatternKind::Tuple(fields) => {
                    if fields.len() > 128 {
                        return Err(Diagnostic::new(
                            pattern.span,
                            "sequence pattern prefix limit exceeded",
                        ));
                    }
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
    /// Bound flat sequence fields and reject recursively structured rest payloads before copies.
    fn sequence<'a>(
        &mut self,
        prefix: &'a [ast::Pattern],
        rest: Option<&'a ast::Pattern>,
        pending: &mut Vec<(&'a ast::Pattern, usize)>,
        depth: usize,
        span: Span,
    ) -> Checked<()> {
        if prefix.len() > 128 {
            return Err(Diagnostic::new(
                span,
                "sequence pattern prefix limit exceeded",
            ));
        }
        pending.extend(prefix.iter().map(|p| (p, depth + 1)));
        if let Some(rest) = rest {
            if !matches!(
                rest.kind,
                ast::PatternKind::Bind(_) | ast::PatternKind::Wildcard
            ) {
                return Err(Diagnostic::new(
                    rest.span,
                    "sequence rest must be a binding or wildcard",
                ));
            }
            pending.push((rest, depth + 1));
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
            if let ast::Stmt::LetElse { else_branch, .. } = stmt {
                pending.push((else_branch, depth + 1));
            }
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
                ast::Stmt::LetElse {
                    pattern,
                    annotation,
                    value,
                    span,
                    ..
                }
                | ast::Stmt::LetPattern {
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

    /// Charge record field names and queue initializer expressions before cloning.
    fn update_fields<'a>(
        &mut self,
        fields: &'a [ast::RecordField],
        pending: &mut Vec<(&'a ast::Expr, usize)>,
        depth: usize,
    ) -> Checked<()> {
        for field in fields {
            self.charge(field.name.len(), field.span)?;
            pending.push((&field.value, depth + 1));
        }
        Ok(())
    }

    /// Queue flat control flow and charge each newly introduced binding pattern.
    fn iteration<'a>(
        &mut self,
        expr: &'a ast::Expr,
        pending: &mut Vec<(&'a ast::Expr, usize)>,
        depth: usize,
    ) -> Checked<bool> {
        match &expr.kind {
            ast::ExprKind::Range { start, end, .. } => {
                pending.push((start, depth + 1));
                pending.push((end, depth + 1));
            }
            ast::ExprKind::For {
                pattern,
                iterable,
                body,
            } => {
                self.pattern(pattern)?;
                pending.push((iterable, depth + 1));
                pending.push((body, depth + 1));
            }
            ast::ExprKind::With {
                bindings,
                body,
                arms,
            } => {
                for binding in bindings {
                    self.pattern(&binding.pattern)?;
                    pending.push((&binding.value, depth + 1));
                }
                pending.push((body, depth + 1));
                if let Some(arms) = arms {
                    self.match_arms(arms, pending, depth)?;
                }
            }
            _ => return Ok(false),
        }
        Ok(true)
    }

    /// Validate expression depth and annotations before source-instance cloning.
    fn expression(&mut self, expr: &ast::Expr) -> Checked<()> {
        let mut pending = vec![(expr, 0)];
        while let Some((expr, depth)) = pending.pop() {
            self.expression_node(expr.span, depth)?;
            if self.iteration(expr, &mut pending, depth)? {
                continue;
            }
            match &expr.kind {
                ast::ExprKind::ConditionMatch(arms) => queue_conditions(arms, &mut pending, depth),
                ast::ExprKind::Interpolate(parts) | ast::ExprKind::MultilineString(parts) => {
                    self.interpolation(parts, &mut pending, depth, expr.span)?
                }
                ast::ExprKind::Map(entries) => queue_map(entries, &mut pending, depth),
                ast::ExprKind::RecordUpdate { value, fields } => {
                    pending.push((value, depth + 1));
                    self.update_fields(fields, &mut pending, depth)?;
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
                ast::ExprKind::Return(value)
                | ast::ExprKind::Defer(value)
                | ast::ExprKind::Unary { value, .. }
                | ast::ExprKind::Try(value) => pending.push((value, depth + 1)),
                ast::ExprKind::Field { value, name } => {
                    self.charge(name.len(), expr.span)?;
                    pending.push((value, depth + 1));
                }
                ast::ExprKind::PostfixIf {
                    value: left,
                    condition: right,
                }
                | ast::ExprKind::Binary { left, right, .. } => {
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
                ast::ExprKind::If { .. } => queue_if(expr, &mut pending, depth),
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
    for doc in &program.docs {
        budget.charge(doc.target.len().saturating_add(doc.text.len()), doc.span)?;
    }
    for function in &program.functions {
        budget.charge(function.name.len(), function.span)?;
        for param in &function.params {
            budget.pattern(&param.pattern)?;
            if let Some(ty) = &param.annotation {
                budget.ty(ty, param.span)?;
            }
        }
        if let Some(ty) = &function.return_type {
            budget.ty(ty, function.span)?;
        }
        if let Some(guard) = &function.guard {
            budget.expression(guard)?;
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

/// Queue map keys and values at the same bounded expression depth.
fn queue_map<'a>(
    entries: &'a [(ast::Expr, ast::Expr)],
    pending: &mut Vec<(&'a ast::Expr, usize)>,
    depth: usize,
) {
    pending.extend(
        entries
            .iter()
            .flat_map(|(key, value)| [(key, depth + 1), (value, depth + 1)]),
    );
}

/// Queue lazy condition branches without increasing source nesting for sibling arms.
fn queue_conditions<'a>(
    arms: &'a [ast::ConditionArm],
    pending: &mut Vec<(&'a ast::Expr, usize)>,
    depth: usize,
) {
    for arm in arms {
        pending.extend(arm.condition.iter().map(|c| (c, depth + 1)));
        pending.push((&arm.body, depth + 1));
    }
}
/// Queue each branch at its bounded lexical child depth.
fn queue_if<'a>(expr: &'a ast::Expr, pending: &mut Vec<(&'a ast::Expr, usize)>, depth: usize) {
    if let ast::ExprKind::If {
        condition,
        then_branch,
        else_branch,
    } = &expr.kind
    {
        pending.push((condition, depth + 1));
        pending.push((then_branch, depth + 1));
        pending.extend(else_branch.as_deref().map(|n| (n, depth + 1)));
    }
}
