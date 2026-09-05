use super::*;

#[test]
fn publication_and_qbe_reject_probes_even_after_unconditional_return() {
    let mut program =
        crate::check::check(&crate::parse::parse("fn main() -> Int: 0\n").unwrap()).unwrap();
    let value = program.functions[0].body.clone();
    let probe = Expr {
        kind: ExprKind::Probe {
            token: ProbeToken::new(0),
            children: vec![value.clone()],
            bindings: vec![],
        },
        ty: Type::Int,
        span: Span::default(),
    };
    let returning = Expr {
        kind: ExprKind::Return(Box::new(value)),
        ty: Type::Never,
        span: Span::default(),
    };
    program.functions[0].body.kind =
        ExprKind::Block(vec![Stmt::Expr(returning), Stmt::Expr(probe)]);
    assert!(reject_probes(&program)
        .unwrap_err()
        .message
        .contains("inference probe"));
    assert!(crate::qbe::emit(&program)
        .unwrap_err()
        .message
        .contains("inference probe"));
}

#[test]
fn shared_child_traversal_retains_probe_operands_in_source_order() {
    let values = [1, 2]
        .into_iter()
        .map(|n| Expr {
            kind: ExprKind::Int(n),
            ty: Type::Int,
            span: Span::default(),
        })
        .collect();
    let probe = Expr {
        kind: ExprKind::Probe {
            token: ProbeToken::new(0),
            children: values,
            bindings: vec![],
        },
        ty: Type::Int,
        span: Span::default(),
    };
    let values = children(&probe);
    assert!(matches!(values[0].kind, ExprKind::Int(1)));
    assert!(matches!(values[1].kind, ExprKind::Int(2)));
}

#[test]
fn executable_boundaries_reject_editor_holes_even_when_unreachable() {
    let mut program =
        crate::check::check(&crate::parse::parse("fn main() -> Int: 0\n").unwrap()).unwrap();
    let value = program.functions[0].body.clone();
    let hole = Expr {
        kind: ExprKind::EditorHole {
            token: EditorHoleToken::new(),
            receiver: Box::new(value.clone()),
        },
        ty: Type::Int,
        span: Span::default(),
    };
    let returning = Expr {
        kind: ExprKind::Return(Box::new(value)),
        ty: Type::Never,
        span: Span::default(),
    };
    program.functions[0].body.kind = ExprKind::Block(vec![Stmt::Expr(returning), Stmt::Expr(hole)]);
    assert!(reject_probes(&program)
        .unwrap_err()
        .message
        .contains("editor hole"));
    assert!(crate::qbe::emit(&program)
        .unwrap_err()
        .message
        .contains("editor hole"));
}
