//! Bounded pattern-matrix usefulness for scalar and nested nominal/sum patterns.
use super::{nominal::Registry, Checked, MAX_EXPR_DEPTH};
use crate::{ir, Constructor, Diagnostic, Span, Type};

#[derive(Clone, Debug, PartialEq, Eq)]
enum Head {
    Int(i64),
    Bool(bool),
    String(String),
    Variant(usize),
    Unit,
}
type Heads = Vec<(Head, Vec<Type>)>;

#[derive(Clone)]
enum Pattern {
    Any,
    Specific(Head, Vec<Pattern>),
}

/// Reject unreachable arms and require coverage independent of guard conditions.
pub(super) fn validate(
    subject: &Type,
    arms: &[ir::MatchArm],
    registry: &Registry,
    span: Span,
) -> Checked<()> {
    let mut matrix = Vec::new();
    let mut budget = 20_000;
    for arm in arms {
        let pattern = lower(&arm.pattern);
        if !useful(
            &matrix,
            &[pattern.clone()],
            std::slice::from_ref(subject),
            registry,
            0,
            &mut budget,
            span,
        )? {
            return Err(Diagnostic::new(
                arm.span,
                "unreachable match arm: previous unguarded patterns already cover it",
            ));
        }
        if arm.guard.is_none() {
            matrix.push(vec![pattern]);
        }
    }
    if useful(
        &matrix,
        &[Pattern::Any],
        std::slice::from_ref(subject),
        registry,
        0,
        &mut budget,
        span,
    )? {
        return Err(Diagnostic::new(
            span,
            "match must be exhaustive; guards do not guarantee coverage",
        ));
    }
    Ok(())
}

/// Convert resolved patterns into constructor matrices; binders behave as wildcards.
fn lower(pattern: &ir::Pattern) -> Pattern {
    match pattern {
        ir::Pattern::Wildcard | ir::Pattern::Bind(_) => Pattern::Any,
        ir::Pattern::Int(n) => Pattern::Specific(Head::Int(*n), Vec::new()),
        ir::Pattern::Bool(b) => Pattern::Specific(Head::Bool(*b), Vec::new()),
        ir::Pattern::String(s) => Pattern::Specific(Head::String(s.clone()), Vec::new()),
        ir::Pattern::Tuple(fields) if fields.is_empty() => Pattern::Specific(Head::Unit, vec![]),
        ir::Pattern::Tuple(fields) => {
            Pattern::Specific(Head::Variant(0), fields.iter().map(lower).collect())
        }
        ir::Pattern::Variant { tag, fields } => {
            Pattern::Specific(Head::Variant(*tag), fields.iter().map(lower).collect())
        }
        ir::Pattern::Constructor { constructor, .. } => {
            let (tag, fields) = match constructor {
                Constructor::Some | Constructor::Ok => (0, vec![Pattern::Any]),
                Constructor::Err => (1, vec![Pattern::Any]),
                Constructor::None => (1, vec![]),
            };
            Pattern::Specific(Head::Variant(tag), fields)
        }
    }
}

/// Determine whether a candidate tuple contains a value not covered by the matrix.
fn useful(
    matrix: &[Vec<Pattern>],
    candidate: &[Pattern],
    types: &[Type],
    registry: &Registry,
    depth: usize,
    budget: &mut usize,
    span: Span,
) -> Checked<bool> {
    if *budget == 0 || depth >= MAX_EXPR_DEPTH {
        return Err(Diagnostic::new(
            span,
            "pattern coverage complexity limit exceeded",
        ));
    }
    *budget -= 1;
    if matrix.is_empty() {
        return Ok(true);
    }
    if candidate.is_empty() {
        return Ok(false);
    }
    if matrix
        .iter()
        .any(|row| row.iter().all(|p| matches!(p, Pattern::Any)))
    {
        return Ok(false);
    }
    match &candidate[0] {
        Pattern::Specific(head, fields) => {
            let payload = payload_types(&types[0], head, registry, span)?;
            let matrix = specialize(matrix, head, fields.len());
            let mut next = fields.clone();
            next.extend_from_slice(&candidate[1..]);
            let mut next_types = payload;
            next_types.extend_from_slice(&types[1..]);
            useful(
                &matrix,
                &next,
                &next_types,
                registry,
                depth + 1,
                budget,
                span,
            )
        }
        Pattern::Any => useful_any(matrix, candidate, types, registry, depth, budget, span),
    }
}

/// Search a wildcard candidate through finite constructors or the default matrix.
fn useful_any(
    matrix: &[Vec<Pattern>],
    candidate: &[Pattern],
    types: &[Type],
    registry: &Registry,
    depth: usize,
    budget: &mut usize,
    span: Span,
) -> Checked<bool> {
    if let Some(heads) = finite_heads(&types[0], registry, span)? {
        for (head, payload) in heads {
            let matrix = specialize(matrix, &head, payload.len());
            let mut next = vec![Pattern::Any; payload.len()];
            next.extend_from_slice(&candidate[1..]);
            let mut next_types = payload;
            next_types.extend_from_slice(&types[1..]);
            if useful(
                &matrix,
                &next,
                &next_types,
                registry,
                depth + 1,
                budget,
                span,
            )? {
                return Ok(true);
            }
        }
        Ok(false)
    } else {
        let defaults: Vec<_> = matrix
            .iter()
            .filter(|row| matches!(row[0], Pattern::Any))
            .map(|row| row[1..].to_vec())
            .collect();
        useful(
            &defaults,
            &candidate[1..],
            &types[1..],
            registry,
            depth + 1,
            budget,
            span,
        )
    }
}

/// Restrict each matrix row to one constructor, expanding wildcard payload slots.
fn specialize(matrix: &[Vec<Pattern>], head: &Head, arity: usize) -> Vec<Vec<Pattern>> {
    matrix
        .iter()
        .filter_map(|row| {
            let mut fields = match &row[0] {
                Pattern::Any => vec![Pattern::Any; arity],
                Pattern::Specific(other, fields) if other == head => fields.clone(),
                _ => return None,
            };
            fields.extend_from_slice(&row[1..]);
            Some(fields)
        })
        .collect()
}

/// Enumerate a finite type's constructors, leaving scalar integers/strings open-ended.
fn finite_heads(ty: &Type, registry: &Registry, span: Span) -> Checked<Option<Heads>> {
    match ty {
        Type::Bool => Ok(Some(vec![
            (Head::Bool(true), vec![]),
            (Head::Bool(false), vec![]),
        ])),
        Type::Unit => Ok(Some(vec![(Head::Unit, vec![])])),
        Type::Tuple(_) | Type::Option(_) | Type::Result(..) | Type::Named(..) => Ok(Some(
            registry
                .variants(ty, span)?
                .into_iter()
                .enumerate()
                .map(|(tag, fields)| (Head::Variant(tag), fields))
                .collect(),
        )),
        _ => Ok(None),
    }
}

/// Return the checked payload types of one concrete constructor descriptor.
fn payload_types(ty: &Type, head: &Head, registry: &Registry, span: Span) -> Checked<Vec<Type>> {
    if let Head::Variant(tag) = head {
        registry
            .variants(ty, span)?
            .get(*tag)
            .cloned()
            .ok_or_else(|| Diagnostic::new(span, "invalid constructor tag in pattern"))
    } else {
        Ok(Vec::new())
    }
}
