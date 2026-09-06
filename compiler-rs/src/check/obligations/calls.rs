//! Source bodies are summarized once in dependency order, never interpreted per call site.
use super::*;
#[derive(Default)]
pub(super) struct Summaries {
    ready: HashMap<usize, Rc<Summary>>,
    index: HashMap<usize, usize>,
    recursive_defaults: HashMap<usize, recursive::Contract>,
}
impl Summaries {
    /// Expose only the checked structural contract identity needed for child traversal proofs.
    pub(super) fn tree_parameter(&self, function: usize) -> Option<usize> {
        match self.recursive_defaults.get(&function) {
            Some(recursive::Contract::TreeHandler(index)) => Some(*index),
            _ => None,
        }
    }
}
/// Build acyclic source summaries under one aggregate budget; recursive equations follow separately.
#[cfg(test)]
pub(super) fn analyze(program: &ir::Program, root: &ir::Function) -> Checked<Summary> {
    let mut work = 0;
    let mut summaries = Summaries::default();
    extend(
        program,
        root,
        None,
        Mode::Concrete,
        &mut summaries,
        &mut work,
    )?;
    let result = summaries
        .ready
        .remove(&root.id.0)
        .ok_or_else(|| Diagnostic::new(root.body.span, "missing Result obligation root summary"))?;
    Rc::try_unwrap(result)
        .map_err(|_| Diagnostic::new(root.body.span, "shared Result obligation root summary"))
}
/// Reuse already checked callees and one aggregate budget across independent source definitions.
pub(super) fn extend(
    program: &ir::Program,
    root: &ir::Function,
    relevance: Option<&HashSet<usize>>,
    mode: Mode,
    summaries: &mut Summaries,
    work: &mut usize,
) -> Checked<()> {
    if summaries.ready.contains_key(&root.id.0) {
        return Ok(());
    }
    if summaries.index.is_empty() {
        charge(work, program.functions.len(), root.body.span)?;
        summaries.index = program
            .functions
            .iter()
            .enumerate()
            .map(|(index, f)| (f.id.0, index))
            .collect();
    }
    let order = dependency_order(program, root, relevance, mode, summaries, work)?;
    for function in order {
        let mut engine = Engine::new(program);
        engine.work = *work;
        engine.summaries = Some(summaries);
        engine.relevance = relevance;
        engine.mode = mode;
        let mut summary = analyze_body(engine, function)?;
        if let Some(contract) = summaries.recursive_defaults.get(&function.id.0) {
            recursive::verify(contract, &mut summary, function.body.span)?;
        }
        *work = summary.work;
        summaries.ready.insert(function.id.0, Rc::new(summary));
    }
    Ok(())
}
/// A bounded graph walk orders declarations independently of their textual order.
fn dependency_order<'a>(
    program: &'a ir::Program,
    root: &'a ir::Function,
    relevance: Option<&HashSet<usize>>,
    mode: Mode,
    summaries: &mut Summaries,
    work: &mut usize,
) -> Checked<Vec<&'a ir::Function>> {
    let mut pending = vec![(root, false)];
    let mut active = HashSet::new();
    let mut done = HashSet::new();
    let mut ordered = Vec::new();
    while let Some((function, exit)) = pending.pop() {
        charge(work, 1, function.body.span)?;
        if exit {
            active.remove(&function.id.0);
            done.insert(function.id.0);
            ordered.push(function);
            continue;
        }
        if done.contains(&function.id.0)
            || summaries.ready.contains_key(&function.id.0)
            || relevance.is_some_and(|set| !set.contains(&function.id.0))
        {
            continue;
        }
        if !active.insert(function.id.0) {
            let contract = recursive::contract(program, function, mode, work)?;
            summaries.recursive_defaults.insert(function.id.0, contract);
            continue;
        }
        pending.push((function, true));
        for id in ordered_dependencies(program, &function.body, work)? {
            let child = summaries
                .index
                .get(&id)
                .and_then(|index| program.functions.get(*index))
                .ok_or_else(|| {
                    Diagnostic::new(function.body.span, "missing Result obligation callee")
                })?;
            charge(work, 1, function.body.span)?;
            pending.push((child, false));
        }
    }
    Ok(ordered)
}
/// Deferred bodies execute in their registering proof context, not as independent recursive assumptions.
fn ordered_dependencies(
    program: &ir::Program,
    expr: &ir::Expr,
    work: &mut usize,
) -> Checked<Vec<usize>> {
    let mut pending = vec![expr];
    let mut inline = HashSet::new();
    let mut found = BTreeMap::new();
    while let Some(expr) = pending.pop() {
        charge(work, 1, expr.span)?;
        if let ir::ExprKind::Defer(value) = &expr.kind {
            if let ir::ExprKind::Closure { function, captures } = &value.kind {
                charge(work, captures.len(), expr.span)?;
                pending.extend(captures);
                if inline.insert(function.0) {
                    charge(work, program.functions.len(), expr.span)?;
                    let body = program
                        .functions
                        .iter()
                        .find(|f| f.id == *function)
                        .ok_or_else(|| {
                            Diagnostic::new(expr.span, "missing deferred obligation function")
                        })?;
                    pending.push(&body.body);
                }
                continue;
            }
        }
        match &expr.kind {
            ir::ExprKind::Call {
                target: ir::CallTarget::Function(id),
                ..
            }
            | ir::ExprKind::Closure { function: id, .. } => {
                found.insert(id.0, ());
            }
            _ => {}
        }
        for child in ir::children(expr) {
            charge(work, 1, child.span)?;
            pending.push(child);
        }
    }
    Ok(found.into_keys().collect())
}
/// Scan all typed children, charging each edge before it enters the bounded traversal queue.
pub(super) fn dependencies(expr: &ir::Expr, work: &mut usize) -> Checked<Vec<usize>> {
    let mut pending = vec![expr];
    let mut found = BTreeMap::new();
    while let Some(expr) = pending.pop() {
        charge(work, 1, expr.span)?;
        if let ir::ExprKind::Call {
            target: ir::CallTarget::Function(id),
            ..
        } = &expr.kind
        {
            found.insert(id.0, ());
        }
        if let ir::ExprKind::Closure { function, .. } = &expr.kind {
            found.insert(function.0, ());
        }
        for child in ir::children(expr) {
            charge(work, 1, child.span)?;
            pending.push(child);
        }
    }
    Ok(found.into_keys().collect())
}
impl Engine<'_> {
    /// Instantiate finalized parameter and output provenance without treating a call as consuming.
    pub(super) fn source_call(&mut self, id: usize, args: &[Value], span: Span) -> Checked<Value> {
        if let Some(summaries) = self.summaries {
            if let Some(contract) = summaries
                .recursive_defaults
                .get(&id)
                .filter(|_| !summaries.ready.contains_key(&id))
            {
                let function = summaries
                    .index
                    .get(&id)
                    .and_then(|index| self.program.functions.get(*index))
                    .ok_or_else(|| {
                        Diagnostic::new(span, "missing recursive obligation signature")
                    })?;
                return recursive::apply(self, contract, function, args, span);
            }
        }
        if let Some(summaries) = self.summaries {
            if let Some(recursive::Contract::TreeHandler(index)) =
                summaries.recursive_defaults.get(&id)
            {
                if summaries.ready.contains_key(&id) {
                    return recursive_trees::complete(self, *index, args, span);
                }
            }
        }
        let specialized = self.effect_summary(id, args, span)?;
        let summary = specialized
            .or_else(|| self.summaries.and_then(|s| s.ready.get(&id)).cloned())
            .ok_or_else(|| Diagnostic::new(span, "missing Result obligation callee summary"))?;
        let mut substitution = super::substitute::Substitution::new(&summary);
        substitution.apply(self, args, span)
    }
}
