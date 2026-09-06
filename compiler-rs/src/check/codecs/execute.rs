//! Build concrete plans only after checking the single executable operand.
use super::*;
use crate::json_codec as wire;
use std::rc::Rc;
impl Checker<'_> {
    /// A decoder target occupies a type-only slot; it never becomes a checked expression.
    pub(in crate::check) fn json_codec(
        &mut self,
        name: &str,
        args: &[ast::Argument],
        expected: Option<&Type>,
        span: Span,
        depth: usize,
    ) -> Checked<TypedKind> {
        crate::codec_syntax::validate_call(name, args, span)?;
        let decode = crate::codec_syntax::is_decode(name);
        let target = if decode {
            let raw = crate::codec_syntax::target(&args[1].value)?;
            Some(target_type(self.registry, &raw, args[1].span, 0, &mut 0)?)
        } else {
            None
        };
        if let Some(target) = &target {
            self.registry
                .validate(target, &HashSet::new(), args[1].span)?;
        }
        let input =
            self.expression_expected(&args[0].value, decode.then_some(&Type::String), depth)?;
        if input.ty == Type::Never {
            return Ok((input.kind, Type::Never));
        }
        let target = match target {
            Some(ty) => ty,
            None => self.inference.resolve(&input.ty, span)?,
        };
        let plan = concrete(self.registry, &target, span)?;
        let output = if decode { target } else { Type::String };
        let ty = Type::Result(
            Box::new(output),
            Box::new(Type::Native(runtime::NativeType::JsonError)),
        );
        self.constrain_result(&ty, expected, span)?;
        let direction = if decode {
            wire::Direction::Decode
        } else {
            wire::Direction::Encode
        };
        Ok((
            ir::ExprKind::JsonCodec {
                direction,
                input: Box::new(input),
                plan,
            },
            ty,
        ))
    }
}
/// Share concrete plans and their aggregate construction allowance across every compiler pass.
fn concrete(registry: &nominal::Registry, ty: &Type, span: Span) -> Checked<Rc<wire::Plan>> {
    let empty = ast::Program::default();
    let mut builder = Planner::new(&empty, registry);
    builder.work = registry.codec_work.get();
    let result = (|| {
        builder.type_work(ty, span)?;
        if let Some(plan) = registry.codec_plans.borrow().get(ty) {
            return Ok(plan.clone());
        }
        let root = builder.plan(ty, span, 0)?.0;
        builder.finite(span)?;
        let entries = std::mem::take(&mut builder.entries)
            .into_iter()
            .map(concrete_entry)
            .collect::<Checked<Vec<_>>>()?;
        let plan = Rc::new(wire::Plan { root, entries });
        registry
            .codec_plans
            .borrow_mut()
            .insert(ty.clone(), plan.clone());
        Ok(plan)
    })();
    registry.codec_work.set(builder.work);
    result
}
/// Eliminate the private symbolic proof kind at the one concrete publication boundary.
fn concrete_entry(entry: Plan) -> Checked<wire::Entry> {
    let kind = match entry.kind {
        Kind::Int => wire::Kind::Int,
        Kind::Float => wire::Kind::Float,
        Kind::Bool => wire::Kind::Bool,
        Kind::String => wire::Kind::String,
        Kind::Unit => wire::Kind::Unit,
        Kind::Dynamic => wire::Kind::Dynamic,
        Kind::Newtype(id) => wire::Kind::Newtype(id.0),
        Kind::List(id) => wire::Kind::List(id.0),
        Kind::Option(id) => wire::Kind::Option(id.0),
        Kind::Tuple(ids) => wire::Kind::Tuple(ids.into_iter().map(|id| id.0).collect()),
        Kind::Map(id) => wire::Kind::Map(id.0),
        Kind::Record(fields) => wire::Kind::Record(
            fields
                .into_iter()
                .map(|f| wire::Field {
                    name: f.name,
                    index: f.index,
                    codec: f.codec.0,
                    optional: f.optional,
                })
                .collect(),
        ),
        Kind::Parameter | Kind::Pending => {
            return Err(Diagnostic::new(
                Span::default(),
                "symbolic JSON codec cannot become executable",
            ))
        }
    };
    Ok(wire::Entry { ty: entry.ty, kind })
}
/// Resolve direct-AST target aliases through the already validated, fully expanded namespace.
fn target_type(
    registry: &nominal::Registry,
    ty: &Type,
    span: Span,
    depth: usize,
    work: &mut usize,
) -> Checked<Type> {
    crate::unions::bound(ty, span)?;
    *work += 1;
    if depth >= MAX_TYPE_DEPTH || *work > 4096 {
        return Err(Diagnostic::new(
            span,
            "JSON target type expansion limit exceeded",
        ));
    }
    let mut child = |t: &Type| target_type(registry, t, span, depth + 1, work);
    Ok(match ty {
        Type::Named(name, args) => {
            let args = args.iter().map(&mut child).collect::<Checked<Vec<_>>>()?;
            if let Some((parameters, target)) = registry.codec_aliases.get(name) {
                if parameters.len() != args.len() {
                    return Err(Diagnostic::new(span, "wrong JSON target alias arity"));
                }
                let values = parameters.iter().cloned().zip(args).collect();
                nominal::substitute(target, &values)?
            } else {
                Type::Named(name.clone(), args)
            }
        }
        Type::List(a) => Type::List(Box::new(child(a)?)),
        Type::Option(a) => Type::Option(Box::new(child(a)?)),
        Type::Result(a, b) => Type::Result(Box::new(child(a)?), Box::new(child(b)?)),
        Type::Map(a, b) => Type::Map(Box::new(child(a)?), Box::new(child(b)?)),
        Type::Tuple(args) => Type::Tuple(args.iter().map(&mut child).collect::<Checked<_>>()?),
        _ => ty.clone(),
    })
}
