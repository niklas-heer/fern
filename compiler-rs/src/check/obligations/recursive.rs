//! Recursive approximations grant no handling; exact returned aliases require an inductive body proof.
use super::*;
#[derive(Clone)]
pub(super) enum Contract {
    Fresh,
    Alias(usize),
    Handler(usize),
    Callables(recursive_callables::Contract),
}
/// A backedge may create an arbitrary result or promise an exact input identity, never discharge it.
pub(super) fn contract(
    program: &ir::Program,
    function: &ir::Function,
    mode: Mode,
    work: &mut usize,
) -> Checked<Contract> {
    let span = function.body.span;
    if mode == Mode::Template
        && (matches!(function.return_type, Type::Generic(_))
            || closed_type(&function.return_type, true, work, span)?)
    {
        // No input is handled or transferred by this approximation; concrete bodies are proved too.
        return Ok(Contract::Fresh);
    }
    let mut closed_inputs = true;
    let mut aliases = Vec::new();
    gate::type_cost(&function.return_type, work, span)?;
    for (index, param) in function.params.iter().chain(&function.captures).enumerate() {
        charge(work, 1, span)?;
        closed_inputs &= closed_type(&param.ty, false, work, span)?;
        if param.ty == function.return_type {
            aliases.push(index);
        }
    }
    if closed_inputs && closed_type(&function.return_type, true, work, span)? {
        return Ok(Contract::Fresh);
    }
    if super::nominal::closed_signature(program, function, work)? {
        return Ok(Contract::Fresh);
    }
    if let Some(index) = super::recursive_handlers::candidate(program, function, work)? {
        return Ok(Contract::Handler(index));
    }
    if let Some(contract) = recursive_callables::candidate(program, function, work)? {
        return Ok(Contract::Callables(contract));
    }
    if let [index] = aliases.as_slice() {
        return Ok(Contract::Alias(*index));
    }
    Err(Diagnostic::new(
        span,
        "Result obligation recursive summary requires a fixed point",
    ))
}
/// Only concrete interfaces without latent callbacks can use an arbitrary fresh-result approximation.
pub(super) fn closed_type(ty: &Type, results: bool, work: &mut usize, span: Span) -> Checked<bool> {
    gate::type_cost(ty, work, span)?;
    let mut pending = vec![ty];
    while let Some(ty) = pending.pop() {
        charge(work, 1, span)?;
        match ty {
            Type::Unit
            | Type::Int
            | Type::Float
            | Type::Bool
            | Type::String
            | Type::Range
            | Type::Native(_) => {}
            Type::List(inner) | Type::Option(inner) => pending.push(inner),
            Type::Map(key, value) => {
                charge(work, 2, span)?;
                pending.extend([key.as_ref(), value.as_ref()]);
            }
            Type::Result(ok, error) if results => {
                charge(work, 2, span)?;
                pending.extend([ok.as_ref(), error.as_ref()]);
            }
            Type::Tuple(fields) | Type::Union(fields) => {
                charge(work, fields.len(), span)?;
                pending.extend(fields);
            }
            _ => return Ok(false),
        }
    }
    Ok(true)
}
/// Approximate one recursive return with zero input effects; no runtime recursion is unrolled.
pub(super) fn apply(
    engine: &mut Engine<'_>,
    contract: &Contract,
    function: &ir::Function,
    args: &[Value],
    span: Span,
) -> Checked<Value> {
    match contract {
        Contract::Fresh => engine.fresh(&function.return_type, None, span, 0),
        Contract::Callables(contract) => recursive_callables::apply(engine, contract, args, span),
        Contract::Handler(index) => {
            super::recursive_handlers::apply(engine, function, *index, args, span)
        }
        Contract::Alias(index) => args
            .get(*index)
            .cloned()
            .ok_or_else(|| Diagnostic::new(span, "missing recursive Result alias input")),
    }
}
/// Every actual terminating return must retain the exact candidate input, including guarded returns.
pub(super) fn verify(contract: &Contract, summary: &mut Summary, span: Span) -> Checked<()> {
    if let Contract::Callables(contract) = contract {
        return recursive_callables::verify(contract, summary, span);
    }
    if let Contract::Handler(index) = contract {
        return super::recursive_handlers::verify(*index, summary, span);
    }
    let Contract::Alias(index) = contract else {
        return Ok(());
    };
    let input = summary
        .inputs
        .get(*index)
        .ok_or_else(|| Diagnostic::new(span, "missing recursive Result contract input"))?;
    let mut pending = vec![(Predicate::TRUE, &summary.output, 0usize)];
    while let Some((guard, value, depth)) = pending.pop() {
        charge(&mut summary.work, 1, span)?;
        if depth >= DEPTH_LIMIT {
            return Err(Diagnostic::new(
                span,
                "Result recursive alias depth limit exceeded",
            ));
        }
        if guard == Predicate::FALSE {
            continue;
        }
        if value.complete && value.node.id == input.node.id {
            continue;
        }
        let Region::Choice(choices) = &value.node.kind else {
            return Err(Diagnostic::new(
                span,
                "Result obligation recursive return does not preserve its input alias",
            ));
        };
        if !value.complete {
            return Err(Diagnostic::new(
                span,
                "Result obligation recursive alias is only partial",
            ));
        }
        charge(&mut summary.work, choices.len(), span)?;
        for (selected, child) in choices {
            let selected = summary
                .predicates
                .and(guard, *selected, &mut summary.work, span)?;
            pending.push((selected, child, depth + 1));
        }
    }
    Ok(())
}
