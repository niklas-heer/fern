//! Whole-subtree cuts permit finite structural induction without merging sibling obligations.
use super::*;
impl Engine<'_> {
    /// A repeated layout edge owns an indivisible duty for this subtree, never a tag acknowledgement.
    pub(super) fn recursive_cut(
        &mut self,
        layout: usize,
        anchor: usize,
        input: Option<usize>,
        span: Span,
    ) -> Checked<Value> {
        let origin = self.origin(input, span)?;
        self.origins[origin].aggregate = true;
        let value = self.node(
            Region::RecursiveCut {
                layout,
                origin: Some(origin),
            },
            span,
        )?;
        self.charge(1, span)?;
        self.nominal_descendants.insert(value.node.id, anchor);
        Ok(value)
    }
}
/// One recursive nominal input can be handled inductively; other interfaces carry no latent duties.
pub(super) fn candidate(
    program: &ir::Program,
    function: &ir::Function,
    work: &mut usize,
) -> Checked<Option<usize>> {
    if function.return_type != Type::Unit {
        return Ok(None);
    }
    let span = function.body.span;
    let mut found = None;
    for (index, param) in function.params.iter().chain(&function.captures).enumerate() {
        if matches!(param.ty, Type::Named(..)) && gate::contains(program, &param.ty, work, span)? {
            if found.replace(index).is_some() || !storage(program, &param.ty, work, span)? {
                return Ok(None);
            }
        } else if !recursive::closed_type(&param.ty, false, work, span)? {
            return Ok(None);
        }
    }
    Ok(found)
}
/// Follow real storage edges with finite identity tracking, excluding unresolved callable interfaces.
fn storage(program: &ir::Program, ty: &Type, work: &mut usize, span: Span) -> Checked<bool> {
    let mut pending = vec![ty];
    let mut seen = HashSet::new();
    let mut recursive = false;
    while let Some(ty) = pending.pop() {
        gate::type_cost(ty, work, span)?;
        match ty {
            Type::Named(..) => {
                let index = gate::layout_index(program, ty, work, span)?;
                if !seen.insert(index) {
                    recursive = true;
                    continue;
                }
                for fields in &program.types[index].variants {
                    charge(work, fields.len(), span)?;
                    pending.extend(fields);
                }
            }
            Type::List(a) | Type::Option(a) => pending.push(a),
            Type::Result(a, b) | Type::Map(a, b) => {
                charge(work, 2, span)?;
                pending.extend([a.as_ref(), b.as_ref()]);
            }
            Type::Tuple(xs) | Type::Union(xs) => {
                charge(work, xs.len(), span)?;
                pending.extend(xs);
            }
            Type::Function(..) | Type::Infer(_) | Type::Never => return Ok(false),
            _ => {}
        }
    }
    Ok(recursive)
}
/// Provisional recursion applies only to a complete strict descendant of this exact source input.
pub(super) fn apply(
    engine: &mut Engine<'_>,
    function: &ir::Function,
    index: usize,
    args: &[Value],
    span: Span,
) -> Checked<Value> {
    let actual = args
        .get(index)
        .ok_or_else(|| Diagnostic::new(span, "missing recursive tree argument"))?;
    let context = context(engine);
    if !matches!(context, Some((id, parameter, _)) if id == function.id.0 && parameter == index)
        || !strict(
            engine,
            actual,
            context.map_or(usize::MAX, |(_, _, anchor)| anchor),
            span,
            0,
        )?
    {
        return Err(Diagnostic::new(span,"Result obligation recursive tree handler requires a strict descendant of its own input"));
    }
    complete(engine, index, args, span)
}
/// Iteration context is installed only from certified actual provenance; ordinary roots stay exact.
pub(super) fn context(engine: &Engine<'_>) -> Option<(usize, usize, usize)> {
    if let Some(context) = engine.tree_context {
        return Some(context);
    }
    let function = engine.source_function?;
    let index = engine.summaries?.tree_parameter(function)?;
    let value = engine.inputs.get(index)?;
    let anchor = *engine.nominal_roots.get(&value.node.id)?;
    Some((function, index, anchor))
}
/// Choices retain their own certificates; equal layout or type names confer no structural descent.
fn strict(
    engine: &mut Engine<'_>,
    value: &Value,
    anchor: usize,
    span: Span,
    depth: usize,
) -> Checked<bool> {
    engine.charge(1, span)?;
    if depth >= DEPTH_LIMIT {
        return engine.unsupported(span);
    }
    if !value.complete {
        return Ok(false);
    }
    if engine.nominal_descendants.get(&value.node.id) == Some(&anchor) {
        return Ok(true);
    }
    if let Region::Choice(choices) = &value.node.kind {
        engine.charge(choices.len(), span)?;
        for (guard, child) in choices {
            if *guard != Predicate::FALSE && !strict(engine, child, anchor, span, depth + 1)? {
                return Ok(false);
            }
        }
        return Ok(true);
    }
    Ok(false)
}
/// A finalized complete Unit handler accounts for the actual subtree without expanding a cut.
pub(super) fn complete(
    engine: &mut Engine<'_>,
    index: usize,
    args: &[Value],
    span: Span,
) -> Checked<Value> {
    let actual = args
        .get(index)
        .ok_or_else(|| Diagnostic::new(span, "missing complete tree argument"))?;
    engine.dispose(actual, false, false, span)?;
    engine.node(Region::Empty, span)
}

#[cfg(test)]
mod tests {
    use super::*;
    fn tree_program() -> ir::Program {
        let source="type Tree:\n    Empty\n    Leaf(Result(Int,String))\n    Branch(Tree,Tree)\nfn identity(tree:Tree)->Tree:tree\nfn main():()\n";
        let ast = crate::parse::parse(source).unwrap();
        super::super::super::pipeline_mode(&ast, |_, _, _| Ok(()), false)
            .unwrap()
            .0
    }
    #[test]
    fn repeated_edges_are_distinct_indivisible_subtree_duties() {
        let program = tree_program();
        let mut engine = Engine::new(&program);
        let ty = Type::Named("Tree".into(), vec![]);
        let value = engine.fresh(&ty, Some(0), Span::default(), 0).unwrap();
        assert_eq!(engine.origins.len(), 3);
        assert_eq!(engine.origins.iter().filter(|o| o.aggregate).count(), 2);
        assert_eq!(
            engine
                .origins_of(&value, false, Span::default())
                .unwrap()
                .len(),
            3
        );
        assert!(engine
            .origins_of(&value, true, Span::default())
            .unwrap()
            .is_empty());
    }
    #[test]
    fn equal_layouts_and_partial_views_do_not_confer_descent() {
        let program = tree_program();
        let mut engine = Engine::new(&program);
        let ty = Type::Named("Tree".into(), vec![]);
        let first = engine.fresh(&ty, Some(0), Span::default(), 0).unwrap();
        let second = engine.fresh(&ty, Some(1), Span::default(), 0).unwrap();
        let first_anchor = engine.nominal_roots[&first.node.id];
        let second_anchor = engine.nominal_roots[&second.node.id];
        let (_, fields) = engine.variant(&first, 2, Span::default(), 0).unwrap();
        assert!(strict(&mut engine, &fields[0], first_anchor, Span::default(), 0).unwrap());
        assert!(!strict(&mut engine, &fields[0], second_anchor, Span::default(), 0).unwrap());
        assert!(!strict(
            &mut engine,
            &fields[0].partial(),
            first_anchor,
            Span::default(),
            0
        )
        .unwrap());
    }
    #[test]
    fn cut_graph_construction_keeps_the_shared_work_limit() {
        let program = tree_program();
        let mut engine = Engine::new(&program);
        engine.work = WORK_LIMIT - 1;
        let error = engine
            .fresh(
                &Type::Named("Tree".into(), vec![]),
                Some(0),
                Span::default(),
                0,
            )
            .unwrap_err();
        assert!(error.message.contains("work limit"), "{error:?}");
    }
}
