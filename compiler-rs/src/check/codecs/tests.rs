use super::*;
fn graph(source: &str, target: Type) -> Checked<Plans> {
    let program = crate::parse::parse(source)?;
    super::super::preflight::check(&program)?;
    let expanded = super::super::aliases::expand(&program)?;
    let program = expanded.program.as_ref();
    let registry = nominal::Registry::new(program)?;
    let mut planner = Planner::new(program, &registry);
    let root = planner.plan(&target, Span::default(), 0)?;
    Ok(Plans {
        root,
        entries: planner.entries,
    })
}
#[test]
fn concrete_record_plans_preserve_order_and_share_scalar_children() {
    let plans = graph(
        "type Pair derive(Json):\n    left:Int\n    right:Int\n",
        Type::Named("Pair".into(), vec![]),
    )
    .unwrap();
    assert_eq!(plans.entries.len(), 2);
    let Kind::Record(fields) = &plans.entries[plans.root.0].kind else {
        panic!("record")
    };
    assert_eq!(fields[0].name, "left");
    assert_eq!(fields[1].name, "right");
    assert_eq!(fields[0].codec, fields[1].codec);
}
#[test]
fn generic_record_instances_are_independent_and_optional_nullability_is_checked() {
    let source = "type Box(a) derive(Json):\n    value:Option(a)\n";
    assert!(graph(source, Type::Named("Box".into(), vec![Type::String])).is_ok());
    assert!(graph(source, Type::Named("Box".into(), vec![Type::Unit]))
        .unwrap_err()
        .message
        .contains("null"));
}
#[test]
fn unsupported_plans_fail_and_regular_recursive_plans_close() {
    for ty in [
        Type::Infer(0),
        Type::Generic("a".into()),
        Type::Result(Box::new(Type::Int), Box::new(Type::String)),
        Type::Map(Box::new(Type::Int), Box::new(Type::Int)),
    ] {
        assert!(graph("", ty).is_err());
    }
    let source = "type Node derive(Json):\n    children:List(Node)\n";
    assert!(graph(source, Type::Named("Node".into(), vec![])).is_ok());
}
#[test]
fn finite_nested_generic_instantiations_are_not_recursive_declarations() {
    let source = "type Box(a) derive(Json):\n    value:a\n";
    let target = Type::Named(
        "Box".into(),
        vec![Type::Named("Box".into(), vec![Type::Int])],
    );
    assert!(graph(source, target).is_ok());
}
#[test]
fn codec_work_is_aggregate_across_repeated_requests_and_charged_before_copy() {
    let program = crate::parse::parse("").unwrap();
    let registry = nominal::Registry::new(&program).unwrap();
    let mut planner = Planner::new(&program, &registry);
    planner.work = WORK_LIMIT - 1;
    let before = planner.entries.len();
    assert!(planner
        .plan(
            &Type::Tuple(vec![Type::Int, Type::Bool]),
            Span::default(),
            0
        )
        .unwrap_err()
        .message
        .contains("work limit"));
    assert_eq!(planner.entries.len(), before);
}
#[test]
fn aliases_and_result_containers_cannot_conceal_unsupported_payloads() {
    let program = crate::parse::parse(
        "type R=Result(Int,String)\ntype Hidden derive(Json):\n    value:List(R)\n",
    )
    .unwrap();
    let error = crate::check::check_library(&program).unwrap_err();
    assert!(error.message.contains("Result"), "{error:?}");
}
#[test]
fn static_target_aliases_expand_before_dependency_and_specialization_analysis() {
    let mut program = crate::parse::parse(
        "type Data=List(Int)\nfn Data():1\nfn read():json.decode(\"[]\",Data)\n",
    )
    .unwrap();
    crate::codec_syntax::prepare(&mut program.functions[1].body, "json.decode").unwrap();
    super::super::preflight::check(&program).unwrap();
    let expanded = super::super::aliases::expand(&program).unwrap();
    let ast::ExprKind::Call { args, .. } = &expanded.program.functions[1].body.kind else {
        panic!("call")
    };
    assert!(
        matches!(&args[1].value.kind,ast::ExprKind::TypeTarget(Type::List(t)) if **t==Type::Int)
    );
    let graph = super::super::dependencies::analyze(&expanded.program).unwrap();
    assert!(graph
        .groups
        .iter()
        .find(|g| g.name == "read")
        .unwrap()
        .callees
        .is_empty());
}
