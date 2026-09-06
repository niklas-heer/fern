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

#[test]
fn unboxed_layers_cannot_hide_private_editor_or_inference_nodes() {
    for editor in [true, false] {
        let mut program = crate::check::check(
            &crate::parse::parse("newtype Id=Id(Int)\nfn main()->Int:Id(0).0\n").unwrap(),
        )
        .unwrap();
        let leaf = Expr {
            kind: ExprKind::Int(0),
            ty: Type::Int,
            span: Span::default(),
        };
        let kind = if editor {
            ExprKind::EditorHole {
                token: EditorHoleToken::new(),
                receiver: Box::new(leaf),
            }
        } else {
            ExprKind::Probe {
                token: ProbeToken::new(0),
                children: vec![leaf],
                bindings: vec![],
            }
        };
        let hidden = Expr {
            kind,
            ty: Type::Int,
            span: Span::default(),
        };
        let wrapped = Expr {
            kind: ExprKind::Wrap(Box::new(hidden)),
            ty: Type::Named("Id".into(), vec![]),
            span: Span::default(),
        };
        program.functions[0].body = Expr {
            kind: ExprKind::Unwrap(Box::new(wrapped)),
            ty: Type::Int,
            span: Span::default(),
        };
        let message = if editor {
            "editor hole"
        } else {
            "inference probe"
        };
        assert!(reject_probes(&program)
            .unwrap_err()
            .message
            .contains(message));
        assert!(crate::qbe::emit(&program)
            .unwrap_err()
            .message
            .contains(message));
    }
}

#[test]
fn inactive_union_conversions_cannot_hide_private_nodes() {
    for editor in [false, true] {
        for widening in [false, true] {
            let source = "fn unused()->Int | String: 1\nfn main(): ()\n";
            let mut program = crate::check::check(&crate::parse::parse(source).unwrap()).unwrap();
            let function = program
                .functions
                .iter_mut()
                .find(|f| f.name == "unused")
                .unwrap();
            let leaf = function.body.clone();
            let kind = if editor {
                ExprKind::EditorHole {
                    token: EditorHoleToken::new(),
                    receiver: Box::new(leaf.clone()),
                }
            } else {
                ExprKind::Probe {
                    token: ProbeToken::new(0),
                    children: vec![leaf.clone()],
                    bindings: vec![],
                }
            };
            let hidden = Box::new(Expr {
                kind,
                ty: leaf.ty.clone(),
                span: leaf.span,
            });
            let kind = if widening {
                ExprKind::UnionWiden { value: hidden }
            } else {
                ExprKind::UnionInject { value: hidden }
            };
            let conversion = Expr {
                kind,
                ty: leaf.ty.clone(),
                span: leaf.span,
            };
            let returning = Expr {
                kind: ExprKind::Return(Box::new(leaf)),
                ty: Type::Never,
                span: Span::default(),
            };
            function.body.kind =
                ExprKind::Block(vec![Stmt::Expr(returning), Stmt::Expr(conversion)]);
            let message = if editor {
                "editor hole"
            } else {
                "inference probe"
            };
            assert!(reject_probes(&program)
                .unwrap_err()
                .message
                .contains(message));
            assert!(crate::qbe::emit(&program)
                .unwrap_err()
                .message
                .contains(message));
        }
    }
}
