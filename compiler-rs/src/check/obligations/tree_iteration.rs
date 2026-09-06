//! Representative children inherit structural evidence only from complete actual parent provenance.
use super::*;
#[derive(Clone, PartialEq, Eq)]
enum Shape {
    Plain,
    Cut(usize),
    Product(Vec<Shape>),
}
pub(super) struct Plan {
    function: usize,
    parameter: usize,
    shape: Shape,
}
/// Build an intersection of actual descendant evidence, without guessing any scalar or tag value.
pub(super) fn prepare(
    engine: &mut Engine<'_>,
    actual: &Value,
    span: Span,
) -> Checked<Option<Plan>> {
    let Some((function, parameter, anchor)) = recursive_trees::context(engine) else {
        return Ok(None);
    };
    let shape = shape(engine, actual, anchor, span, 0)?;
    Ok((shape != Shape::Plain).then_some(Plan {
        function,
        parameter,
        shape,
    }))
}
/// Every possible actual alternative must carry the same structural evidence.
fn shape(
    engine: &mut Engine<'_>,
    value: &Value,
    anchor: usize,
    span: Span,
    depth: usize,
) -> Checked<Shape> {
    engine.charge(1, span)?;
    if depth >= DEPTH_LIMIT {
        return engine.unsupported(span);
    }
    if !value.complete {
        return Ok(Shape::Plain);
    }
    if let Region::RecursiveCut { layout, .. } = &value.node.kind {
        return Ok(
            if engine.nominal_descendants.get(&value.node.id) == Some(&anchor) {
                Shape::Cut(*layout)
            } else {
                Shape::Plain
            },
        );
    }
    match &value.node.kind {
        Region::Product(fields) => {
            engine.charge(fields.len(), span)?;
            let fields = fields
                .iter()
                .map(|v| shape(engine, v, anchor, span, depth + 1))
                .collect::<Checked<Vec<_>>>()?;
            Ok(if fields.iter().all(|s| *s == Shape::Plain) {
                Shape::Plain
            } else {
                Shape::Product(fields)
            })
        }
        Region::Choice(choices) => choices_shape(engine, choices, anchor, span, depth + 1),
        _ => Ok(Shape::Plain),
    }
}
/// Inactive choices confer no evidence; every reachable choice must agree on its actual origin.
fn choices_shape(
    engine: &mut Engine<'_>,
    choices: &[(Predicate, Value)],
    anchor: usize,
    span: Span,
    depth: usize,
) -> Checked<Shape> {
    engine.charge(choices.len(), span)?;
    let mut found = None;
    for (guard, value) in choices {
        if engine
            .predicates
            .and(engine.path, *guard, &mut engine.work, span)?
            == Predicate::FALSE
        {
            continue;
        }
        let next = shape(engine, value, anchor, span, depth)?;
        if found.as_ref().is_some_and(|old| *old != next) {
            return Ok(Shape::Plain);
        }
        found = Some(next);
    }
    Ok(found.unwrap_or(Shape::Plain))
}
/// A new proof namespace prevents captures or copied source node IDs from inheriting descent.
pub(super) fn input(
    engine: &mut Engine<'_>,
    ty: &Type,
    plan: Option<&Plan>,
    span: Span,
) -> Checked<Value> {
    let Some(plan) = plan else {
        return engine.fresh(ty, Some(0), span, 0);
    };
    let anchor = engine.node(Region::Empty, span)?.node.id;
    engine.tree_context = Some((plan.function, plan.parameter, anchor));
    build(engine, ty, &plan.shape, anchor, span, 0)
}
/// Only certified typed components become cuts; all other regions are fresh independent shapes.
fn build(
    engine: &mut Engine<'_>,
    ty: &Type,
    shape: &Shape,
    anchor: usize,
    span: Span,
    depth: usize,
) -> Checked<Value> {
    engine.charge(1, span)?;
    if depth >= DEPTH_LIMIT {
        return engine.unsupported(span);
    }
    match (shape, ty) {
        (Shape::Plain, _) => engine.fresh(ty, Some(0), span, depth),
        (Shape::Cut(layout), Type::Named(..)) => {
            if gate::layout_index(engine.program, ty, &mut engine.work, span)? != *layout {
                return engine.unsupported(span);
            }
            engine.recursive_cut(*layout, anchor, Some(0), span)
        }
        (Shape::Product(shapes), Type::Tuple(types)) if shapes.len() == types.len() => {
            engine.charge(types.len(), span)?;
            let values = types
                .iter()
                .zip(shapes)
                .map(|(ty, shape)| build(engine, ty, shape, anchor, span, depth + 1))
                .collect::<Checked<_>>()?;
            engine.node(Region::Product(values), span)
        }
        _ => engine.unsupported(span),
    }
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn representatives_require_exact_complete_descendant_evidence() {
        let program = ir::Program::default();
        let mut engine = Engine::new(&program);
        let span = Span::default();
        engine.tree_context = Some((7, 0, 42));
        let child = engine.recursive_cut(0, 42, Some(0), span).unwrap();
        let other = engine.recursive_cut(0, 43, Some(1), span).unwrap();
        assert!(prepare(&mut engine, &child, span).unwrap().is_some());
        assert!(prepare(&mut engine, &child.partial(), span)
            .unwrap()
            .is_none());
        assert!(prepare(&mut engine, &other, span).unwrap().is_none());
        let mixed = engine
            .node(
                Region::Choice(vec![
                    (Predicate::TRUE, child.clone()),
                    (Predicate::TRUE, other.clone()),
                ]),
                span,
            )
            .unwrap();
        assert!(prepare(&mut engine, &mixed, span).unwrap().is_none());
        let inactive = engine
            .node(
                Region::Choice(vec![(Predicate::TRUE, child), (Predicate::FALSE, other)]),
                span,
            )
            .unwrap();
        assert!(prepare(&mut engine, &inactive, span).unwrap().is_some());
    }
    #[test]
    fn representative_shape_uses_the_shared_budget_before_allocation() {
        let program = ir::Program::default();
        let mut engine = Engine::new(&program);
        let span = Span::default();
        engine.tree_context = Some((7, 0, 42));
        let child = engine.recursive_cut(0, 42, Some(0), span).unwrap();
        engine.work = WORK_LIMIT;
        let error = prepare(&mut engine, &child, span)
            .err()
            .expect("bounded proof");
        assert!(error.message.contains("work limit"));
    }
}
