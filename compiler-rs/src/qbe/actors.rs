//! Managed actor lowering keeps logical functions separate from resumable continuation entries.
use super::*;
use crate::actors::{Lowered, Operation};
#[path = "actors/control_types.rs"]
mod control_types;
#[path = "actors/descriptors.rs"]
mod descriptors;
#[path = "actors/lower.rs"]
mod lower;
#[path = "actors/validate.rs"]
mod validate;

#[derive(Default)]
pub(super) struct Plan {
    pub(super) active: bool,
    pub(super) managed: BTreeSet<usize>,
    types: BTreeMap<Type, usize>,
    functions: Vec<Function>,
    steps: BTreeMap<usize, Type>,
    selectors: BTreeMap<usize, Type>,
    pub(super) entries: BTreeMap<usize, usize>,
}

/// Prepared original layouts remain unchanged when lowering appends private functions.
pub(super) struct Prepared<'a> {
    pub(super) program: std::borrow::Cow<'a, ir::Program>,
    pub(super) plan: Plan,
    pub(super) layouts: HashMap<Type, &'a ir::TypeLayout>,
}

/// Prepare private continuations only after the original public tree passes bounded type preflight.
pub(super) fn prepare(program: &ir::Program) -> Lowering<Prepared<'_>> {
    validate::shape(program)?;
    union_validation::preflight(program)?;
    let layouts = nominal::layouts(&program.types)?;
    union_validation::references(program, &layouts)?;
    validate::program(program, &layouts)?;
    let managed = crate::actors::contracts::validate(program)?;
    let mut plan = lower::program(program)?;
    if plan.functions.is_empty() {
        plan.active = !managed.is_empty();
        plan.managed = managed;
        return Ok(Prepared {
            program: std::borrow::Cow::Borrowed(program),
            plan,
            layouts,
        });
    }
    let mut program = program.clone();
    program.functions.append(&mut plan.functions);
    if !plan.entries.is_empty() && program.functions.len() > 4096 {
        return Err(invalid(
            Span::default(),
            "actor continuation descriptor count limit exceeded",
        ));
    }
    plan.managed = crate::actors::contracts::effects(&program)?;
    plan.active = !plan.managed.is_empty();
    // Original layouts are immutable; validate only the new combined expression tree again.
    union_validation::preflight(&program)?;
    union_validation::references(&program, &layouts)?;
    Ok(Prepared {
        program: std::borrow::Cow::Owned(program),
        plan,
        layouts,
    })
}

impl Emitter<'_> {
    /// Lower validated actor operations with the caller's separate execution context.
    pub(super) fn actor_expression(
        &mut self,
        actor: &ir::ActorExpr,
        span: Span,
        locals: &mut Locals,
        depth: usize,
    ) -> Lowering<(Type, String)> {
        match actor {
            ir::ActorExpr::Spawn { entry, mailbox } => {
                let (_, params, result) = crate::actors::function(&entry.ty)
                    .ok_or_else(|| invalid(span, "spawn entry must be callable"))?;
                if !params.is_empty() || *result != Type::Unit {
                    return Err(invalid(
                        span,
                        "spawn entry must take no arguments and return Unit",
                    ));
                }
                let entry = self.expr(entry, locals, depth)?;
                let descriptor = self.actor_type(mailbox)?;
                let ty = Type::Pid(Box::new(mailbox.clone()));
                let value = self.assign(
                    locals,
                    ty.clone(),
                    &format!("call $fern_managed_spawn(l %exec, l {entry}, l {descriptor})"),
                );
                self.guard_fault(locals);
                Ok((ty, value))
            }
            ir::ActorExpr::Send { pid, message } => {
                expect_type(
                    pid.ty.clone(),
                    Type::Pid(Box::new(message.ty.clone())),
                    span,
                )?;
                let pid = self.expr(pid, locals, depth)?;
                let value = self.expr(message, locals, depth)?;
                let value = self.payload(locals, &message.ty, value);
                let descriptor = self.actor_type(&message.ty)?;
                let ty = Type::Result(Box::new(Type::Unit), Box::new(Type::Int));
                let result = self.assign(
                    locals,
                    ty.clone(),
                    &format!(
                        "call $fern_managed_send(l %exec, l {pid}, l {value}, l {descriptor})"
                    ),
                );
                self.guard_fault(locals);
                Ok((ty, result))
            }
            ir::ActorExpr::Lowered(value) => self.actor_operation(&value.operation, locals, depth),
            _ => Err(invalid(span, "unconverted actor suspension")),
        }
    }

    /// Private continuation operations publish only known closure identities after selection.
    fn actor_operation(
        &mut self,
        op: &Operation,
        locals: &mut Locals,
        depth: usize,
    ) -> Lowering<(Type, String)> {
        let value = match op {
            Operation::Pointer(entry) => self.expr(entry, locals, depth)?,
            Operation::Continue(entry) => {
                let entry = self.expr(entry, locals, depth)?;
                self.assign(
                    locals,
                    Type::Int,
                    &format!("call $fern_managed_continue(l %exec, l {entry})"),
                )
            }
            Operation::Register {
                selector,
                timeout,
                duration,
            } => {
                let duration = self.expr(duration, locals, depth)?;
                let selector = self.expr(selector, locals, depth)?;
                let timeout = timeout
                    .as_ref()
                    .map(|e| self.expr(e, locals, depth))
                    .transpose()?
                    .unwrap_or_else(|| "0".into());
                self.assign(locals, Type::Int, &format!("call $fern_managed_receive(l %exec, l {selector}, l {timeout}, l {duration})"))
            }
            Operation::Select { value, arms } => {
                return self.matching_mode(value, arms, locals, depth, false, true)
            }
        };
        self.guard_fault(locals);
        Ok((Type::Int, value))
    }

    /// Preserve main's result/fault precedence before the scheduler can execute queued work.
    pub(super) fn actor_main(&mut self, main: &Function) {
        self.output.push_str("export function w $fern_main() {\n@start\n    %fault =l alloc8 8\n    storel 0, %fault\n");
        self.output.push_str(&format!(
            "    %exec =l call $fern_managed_new(l %fault, l $actor_functions, l {})\n",
            self.functions.len()
        ));
        self.output.push_str("    %initialized =w cnel %exec, 0\n    jnz %initialized, @entry, @initial_failed\n@initial_failed\n    %initial_code =l loadl %fault\n    call $fern_rs_report_fault(l %initial_code)\n    ret 1\n@entry\n");
        let context = if self.actors.managed.contains(&main.id.0) {
            ", l %exec"
        } else {
            ""
        };
        self.output.push_str(&format!(
            "    %exit ={} call $f{}(l 0, l %fault{context})\n",
            self.width(main.return_type.clone()),
            main.id.0
        ));
        self.output.push_str("    %before =l loadl %fault\n    %bad =w cnel %before, 0\n    jnz %bad, @stopped, @main_ok\n@main_ok\n");
        if matches!(main.return_type, Type::Result(_, _)) {
            self.output.push_str("    %actor_ok =l call $fern_result_is_ok(l %exit)\n    %actor_is_ok =w cnel %actor_ok, 0\n    jnz %actor_is_ok, @drain, @stopped\n");
        } else {
            self.output.push_str("    jmp @drain\n");
        }
        self.output.push_str("@drain\n    call $fern_managed_run(l %exec)\n    jmp @stopped\n@stopped\n    call $fern_managed_stop(l %exec)\n    %code =l loadl %fault\n    %failed =w cnel %code, 0\n    jnz %failed, @failed, @success\n@failed\n    call $fern_rs_report_fault(l %code)\n    ret 1\n@success\n");
        if main.return_type == Type::Int {
            self.output
                .push_str("    %status =w copy %exit\n    ret %status\n}\n");
        } else if matches!(main.return_type, Type::Result(_, _)) {
            self.result_main_exit();
        } else {
            self.output.push_str("    ret 0\n}\n");
        }
    }
}

#[cfg(test)]
mod preparation_tests {
    use super::*;

    #[test]
    fn unchanged_program_is_borrowed_after_original_validation() {
        let ast = crate::parse::parse("fn main(): ()").unwrap();
        let program = crate::check::check(&ast).unwrap();
        let prepared = prepare(&program).unwrap();
        assert!(matches!(prepared.program, std::borrow::Cow::Borrowed(_)));
        assert!(!prepared.plan.active);
        assert!(std::ptr::eq(prepared.program.as_ref(), &program));
    }
    #[test]
    fn managed_program_without_continuations_borrows_with_its_effects() {
        let source = "fn worker(): ()\nfn main():\n    let pid:Pid(Int)=spawn(worker)\n    ()\n";
        let program = crate::check::check(&crate::parse::parse(source).unwrap()).unwrap();
        let prepared = prepare(&program).unwrap();
        assert!(matches!(prepared.program, std::borrow::Cow::Borrowed(_)));
        assert!(prepared.plan.active);
        assert!(!prepared.plan.managed.is_empty());
    }
}
