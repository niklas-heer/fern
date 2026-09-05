//! Lower pipes through a private local so input effects run before explicit arguments.
use super::*;
impl Checker<'_> {
    /// Bind a pipe input once, then insert its reference at the checked argument position.
    pub(super) fn pipe(
        &mut self,
        value: &ast::Expr,
        target: (&str, bool),
        args: &[ast::Expr],
        position: usize,
        span: Span,
        depth: usize,
    ) -> Checked<TypedKind> {
        if position > args.len() {
            return Err(Diagnostic::new(span, "invalid pipe argument position"));
        }
        let value = self.expression(value, depth)?;
        if value.ty == Type::Never {
            return Ok((value.kind, Type::Never));
        }
        self.scopes.push(HashMap::new());
        // '$' cannot occur in a source identifier, so this binding cannot capture user names.
        let temporary = format!("$pipe{}", self.local_count);
        let id = self.bind(&temporary, value.ty.clone());
        let mut arguments = args.to_vec();
        arguments.insert(
            position,
            ast::Expr {
                kind: ast::ExprKind::Name(temporary),
                span,
            },
        );
        let result = if target.1 {
            self.global_call(target.0, &arguments, None, span, depth)
        } else {
            self.call(target.0, &arguments, span, depth)
        };
        self.scopes.pop();
        let (kind, ty) = result?;
        let (kind, ty) = control::strict_divergence(kind, ty);
        Ok((
            ir::ExprKind::Block(vec![
                ir::Stmt::Let { id, value },
                ir::Stmt::Expr(ir::Expr {
                    kind,
                    ty: ty.clone(),
                    span,
                }),
            ]),
            ty,
        ))
    }
}
