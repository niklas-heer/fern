//! Rigid generic bodies and residual intrinsic requirements before monomorphization.
use super::*;

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub(super) enum Capability {
    Add,
    Numeric,
    Order,
    Equality,
    Display,
    Print,
    Contains,
    MapKey,
}

#[derive(Clone, Debug)]
pub(super) struct Requirement {
    capability: Capability,
    ty: Type,
    span: Span,
}

pub(super) struct Call {
    target: ir::FunctionId,
    arguments: Vec<Type>,
    span: Span,
}

struct Scheme {
    name: String,
    calls: Vec<Call>,
}

impl Capability {
    /// Mirror concrete language domains; representation width never determines membership.
    fn accepts(self, ty: &Type) -> bool {
        match self {
            Self::Add => matches!(ty, Type::Int | Type::Float | Type::String),
            Self::Numeric | Self::Order => matches!(ty, Type::Int | Type::Float),
            Self::MapKey => scalar(ty),
            Self::Equality | Self::Display | Self::Print | Self::Contains => {
                scalar(ty) || *ty == Type::Float
            }
        }
    }

    /// Keep intrinsic diagnostics meaningful at both definitions and instantiated call sites.
    fn message(self) -> &'static str {
        match self {
            Self::Add => "addition operator requires Int, Float, or String operands",
            Self::Numeric => "numeric operator requires Int or Float operands",
            Self::Order => "ordering operator requires Int or Float operands",
            Self::Equality => "equality operator requires Int, Float, Bool, or String operands",
            Self::Display => "interpolation requires Int, Float, Bool, or String",
            Self::Print => "print argument must be Int, Bool, String, or Float",
            Self::Contains => "List.contains requires scalar Int, Float, Bool, or String elements",
            Self::MapKey => "map key must be Int, Bool, or String",
        }
    }
}

/// Fixed-domain operators retain ordinary rigid equality rather than gaining coercions.
pub(super) fn binary_capability(op: ast::BinaryOp) -> Option<Capability> {
    use ast::BinaryOp::*;
    Some(match op {
        Add => Capability::Add,
        Power | Subtract | Multiply | Divide => Capability::Numeric,
        Lt | Le | Gt | Ge => Capability::Order,
        Eq | Ne => Capability::Equality,
        _ => return None,
    })
}

impl Inference {
    /// Discharge known requirements or retain a declared rigid variable without a witness.
    pub(super) fn require(&self, capability: Capability, ty: &Type, span: Span) -> Checked<()> {
        let ty = self.resolve(ty, span)?;
        returns::charge_output(self, &ty, span)?;
        if capability.accepts(&ty) {
            return Ok(());
        }
        if (self.whole_signature && matches!(ty, Type::Infer(_)))
            || self.template
                && matches!(&ty, Type::Generic(name) if self.template_names.contains(name))
        {
            retain_requirement(
                &mut self.requirements.borrow_mut(),
                Requirement {
                    capability,
                    ty,
                    span,
                },
                self,
            )?;
            return Ok(());
        }
        Err(Diagnostic::new(span, capability.message()))
    }

    /// Map key restrictions apply even to unused parameters and first-class signatures.
    pub(super) fn map_keys(&self, ty: &Type, span: Span) -> Checked<()> {
        let mut pending = vec![ty];
        while let Some(ty) = pending.pop() {
            returns::charge(self, span)?;
            match ty {
                Type::Map(key, value) => {
                    self.require(Capability::MapKey, key, span)?;
                    pending.extend([key.as_ref(), value.as_ref()]);
                }
                Type::Result(a, b) => pending.extend([a.as_ref(), b.as_ref()]),
                Type::List(a) | Type::Option(a) => pending.push(a),
                Type::Tuple(args) | Type::Named(_, args) => pending.extend(args),
                Type::Function(args, result) => {
                    pending.extend(args);
                    pending.push(result);
                }
                _ => {}
            }
        }
        Ok(())
    }
}

impl Checker<'_> {
    /// Validate only calls surviving strict divergence, after their argument types are resolved.
    pub(super) fn finalize_call(
        &self,
        target: ir::CallTarget,
        args: &mut [ir::Expr],
        result: &Type,
        span: Span,
    ) -> Checked<()> {
        for arg in args.iter_mut() {
            self.finalize(arg)?;
        }
        let params: Vec<_> = args.iter().map(|arg| arg.ty.clone()).collect();
        self.named_requirements(target, &params, result, span)?;
        validate_builtin(target, args, span, &self.inference)
    }

    /// A symbolic negation retains a numeric capability instead of choosing an integer witness.
    pub(super) fn finalize_unary(&self, op: ast::UnaryOp, value: &mut ir::Expr) -> Checked<()> {
        self.finalize(value)?;
        if op == ast::UnaryOp::Negate {
            self.inference
                .require(Capability::Numeric, &value.ty, value.span)?;
        }
        Ok(())
    }

    /// Recover each callee's fresh substitution from the surviving fully checked call signature.
    pub(super) fn named_requirements(
        &self,
        target: ir::CallTarget,
        params: &[Type],
        result: &Type,
        span: Span,
    ) -> Checked<()> {
        let ir::CallTarget::Function(id) = target else {
            return Ok(());
        };
        let name = &self.inference.call_names[&id.0];
        let signature = &self.signatures[name];
        if signature.generics.is_empty() {
            return Ok(());
        }
        let mut values = HashMap::new();
        for (template, actual) in signature
            .params
            .iter()
            .zip(params)
            .chain([(&signature.result, result)])
        {
            returns::charge_output(&self.inference, template, span)?;
            returns::charge_output(&self.inference, actual, span)?;
            nominal::capture(template, actual, &mut values, 0)?;
        }
        let arguments = signature
            .generics
            .iter()
            .map(|name| {
                values
                    .get(name)
                    .cloned()
                    .ok_or_else(|| Diagnostic::new(span, "cannot infer generic call requirement"))
            })
            .collect::<Checked<Vec<_>>>()?;
        let call = Call {
            target: id,
            arguments,
            span,
        };
        instantiate_call(&call, signature, &self.inference)?;
        if self.inference.template {
            self.inference.scheme_calls.borrow_mut().push(call);
        }
        Ok(())
    }

    /// Expand nominal fields once per instantiated type, preserving recursive sharing and limits.
    pub(super) fn nominal_requirements(&self, ty: &Type, span: Span) -> Checked<()> {
        let mut pending = vec![(ty.clone(), 0)];
        let mut budget = MAX_TYPE_NODES;
        while let Some((ty, depth)) = pending.pop() {
            returns::charge_output(&self.inference, &ty, span)?;
            if budget == 0 || depth >= MAX_TYPE_DEPTH {
                return Err(Diagnostic::new(
                    span,
                    "nominal requirement expansion limit exceeded",
                ));
            }
            budget -= 1;
            match ty {
                Type::Named(_, ref args) => {
                    pending.extend(args.iter().cloned().map(|t| (t, depth + 1)));
                    if !self
                        .inference
                        .checked_nominals
                        .borrow_mut()
                        .insert(ty.clone())
                    {
                        continue;
                    }
                    let fields = self
                        .registry
                        .requirement_fields(&ty, &self.inference, span)?;
                    for field in fields {
                        self.inference.map_keys(&field, span)?;
                        pending.push((field, depth + 1));
                    }
                }
                Type::List(a) | Type::Option(a) => pending.push((*a, depth + 1)),
                Type::Result(a, b) | Type::Map(a, b) => {
                    pending.extend([(*a, depth + 1), (*b, depth + 1)])
                }
                Type::Tuple(args) => pending.extend(args.into_iter().map(|t| (t, depth + 1))),
                Type::Function(args, result) => {
                    pending.extend(args.into_iter().map(|t| (t, depth + 1)));
                    pending.push((*result, depth + 1));
                }
                _ => {}
            }
        }
        Ok(())
    }
}

/// Validate every declared generic body once, then close its finite intrinsic requirements.
pub(super) fn validate(
    program: &ast::Program,
    registry: &nominal::Registry,
    signatures: &mut HashMap<String, Signature>,
    mut work: usize,
) -> Checked<()> {
    let mut schemes = Vec::new();
    for function in &program.functions {
        if signatures[&function.name].generics.is_empty() {
            continue;
        }
        let (scheme, requirements, used) = check_scheme(function, registry, signatures, work)?;
        work = used;
        signatures
            .get_mut(&function.name)
            .expect("known scheme")
            .requirements = requirements;
        schemes.push(scheme);
    }
    propagate(&schemes, signatures, work)
}

/// Each body owns its rigid names; every callee's quantified variables are freshly instantiated.
fn check_scheme(
    function: &ast::Function,
    registry: &nominal::Registry,
    signatures: &HashMap<String, Signature>,
    work: usize,
) -> Checked<(Scheme, Vec<Requirement>, usize)> {
    let signature = &signatures[&function.name];
    let inference = Inference {
        template: true,
        probing: true,
        template_names: signature.generics.iter().cloned().collect(),
        probe_work: std::cell::Cell::new(work),
        ..Inference::default()
    };
    let mut checker = Checker {
        editor: None,
        recovery: None,
        signatures,
        registry,
        scopes: vec![HashMap::new()],
        local_count: 0,
        expr_count: 0,
        inference,
        function_return: signature.result.clone(),
        deferred: false,
        loop_depth: 0,
    };
    for ty in signature.params.iter().chain([&signature.result]) {
        checker.inference.concrete(ty, function.span)?;
    }
    checker.function(function)?;
    let calls = checker.inference.scheme_calls.into_inner();
    Ok((
        Scheme {
            name: function.name.clone(),
            calls,
        },
        checker.inference.requirements.into_inner(),
        checker.inference.probe_work.get(),
    ))
}

/// Close recursive requirements monotonically, charging every scan and instantiated type.
fn propagate(
    schemes: &[Scheme],
    signatures: &mut HashMap<String, Signature>,
    work: usize,
) -> Checked<()> {
    let inference = Inference {
        template: true,
        probing: true,
        probe_work: std::cell::Cell::new(work),
        ..Inference::default()
    };
    let mut names = vec![String::new(); signatures.len()];
    for (name, signature) in signatures.iter() {
        names[signature.id.0] = name.clone();
    }
    let mut inference = inference;
    loop {
        let mut changed = false;
        for scheme in schemes {
            returns::charge(&inference, Span::default())?;
            inference.template_names = signatures[&scheme.name].generics.iter().cloned().collect();
            for call in &scheme.calls {
                instantiate_call(call, &signatures[&names[call.target.0]], &inference)?;
            }
            let target = signatures.get_mut(&scheme.name).expect("known scheme");
            for requirement in inference.requirements.take() {
                changed |= retain_requirement(&mut target.requirements, requirement, &inference)?;
            }
        }
        if !changed {
            return Ok(());
        }
    }
}

/// Substitute requirements through direct calls and function values without rechecking bodies.
fn instantiate_call(call: &Call, signature: &Signature, inference: &Inference) -> Checked<()> {
    returns::charge(inference, call.span)?;
    for argument in &call.arguments {
        returns::charge_output(inference, argument, call.span)?;
    }
    let values = signature
        .generics
        .iter()
        .cloned()
        .zip(call.arguments.iter().cloned())
        .collect();
    for requirement in &signature.requirements {
        let ty = nominal::substitute(&requirement.ty, &values)?;
        inference.require(requirement.capability, &ty, call.span)?;
    }
    Ok(())
}

/// Bound linear deduplication scans as well as newly retained obligations.
fn retain_requirement(
    requirements: &mut Vec<Requirement>,
    requirement: Requirement,
    inference: &Inference,
) -> Checked<bool> {
    for existing in requirements.iter() {
        returns::charge(inference, requirement.span)?;
        if existing.capability == requirement.capability && existing.ty == requirement.ty {
            return Ok(false);
        }
    }
    returns::charge(inference, requirement.span)?;
    requirements.push(requirement);
    Ok(true)
}

impl Requirement {
    /// Expose bounded semantic wording without publishing internal capability identities.
    pub(super) fn editor_view(&self) -> (&Type, &'static str) {
        (&self.ty, self.capability.message())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn deduplication_scans_spend_the_shared_work_budget() {
        let inference = Inference {
            template: true,
            probing: true,
            template_names: ["a".into()].into_iter().collect(),
            probe_work: std::cell::Cell::new(MAX_EXPR_COUNT * 4 - 2),
            requirements: std::cell::RefCell::new(vec![Requirement {
                capability: Capability::Add,
                ty: Type::Generic("a".into()),
                span: Span::default(),
            }]),
            ..Inference::default()
        };
        let error = inference
            .require(Capability::Add, &Type::Generic("a".into()), Span::default())
            .expect_err("the existing requirement scan must consume work");
        assert!(error.message.contains("inference work limit"));
    }

    #[test]
    fn call_substitution_scans_spend_work_even_before_capabilities_arrive() {
        let inference = Inference {
            template: true,
            probing: true,
            probe_work: std::cell::Cell::new(MAX_EXPR_COUNT * 4 - 2),
            ..Inference::default()
        };
        let signature = Signature {
            id: ir::FunctionId(0),
            params: vec![],
            result: Type::Unit,
            generics: vec!["a".into()],
            dispatch: false,
            requirements: vec![],
            monotype: false,
        };
        let call = Call {
            target: signature.id,
            arguments: vec![Type::Tuple(vec![Type::Int; 10])],
            span: Span::default(),
        };
        let error = instantiate_call(&call, &signature, &inference)
            .expect_err("type substitution setup cannot bypass the shared work budget");
        assert!(error.message.contains("inference work limit"));
    }
}
