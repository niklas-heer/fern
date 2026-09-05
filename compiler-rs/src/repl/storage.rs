//! Aggregate limits for retained closures, including their originating compiled programs.
use super::*;

/// Count retained code and types iteratively so shared program identities can be charged once.
pub(super) fn program_size(program: &ir::Program) -> Result<(usize, usize), String> {
    let mut budget = CodeBudget {
        pending: Vec::new(),
        bytes: 0,
        count: 0,
    };
    for function in &program.functions {
        budget.bytes += function.name.len() + std::mem::size_of::<ir::Function>();
        budget.pending.push(Part::Expr(&function.body));
        budget.pending.push(Part::Type(&function.return_type));
        for parameter in function.params.iter().chain(&function.captures) {
            budget.pending.push(Part::Type(&parameter.ty));
        }
    }
    for layout in &program.types {
        budget.pending.push(Part::Type(&layout.ty));
        budget.bytes += std::mem::size_of::<ir::TypeLayout>()
            + layout.variants.len() * std::mem::size_of::<Vec<Type>>()
            + layout.fields.len() * std::mem::size_of::<String>()
            + layout.fields.iter().map(String::len).sum::<usize>();
        budget
            .pending
            .extend(layout.variants.iter().flatten().map(Part::Type));
    }
    while let Some(part) = budget.pending.pop() {
        budget.count += 1;
        match part {
            Part::Expr(expr) => budget.expression(expr)?,
            Part::Type(ty) => budget.ty(ty),
            Part::Pattern(pattern) => budget.pattern(pattern),
        }
        if budget.bytes > 16 * 1024 * 1024 || budget.count + budget.pending.len() > 200_000 {
            return Err("interactive value storage limit exceeded".into());
        }
    }
    Ok((budget.bytes, budget.count))
}
enum Part<'a> {
    Expr(&'a ir::Expr),
    Type(&'a Type),
    Pattern(&'a ir::Pattern),
}
struct CodeBudget<'a> {
    pending: Vec<Part<'a>>,
    bytes: usize,
    count: usize,
}
impl<'a> CodeBudget<'a> {
    /// Include expression-owned strings, child storage and all retained type information.
    fn expression(&mut self, expr: &'a ir::Expr) -> Result<(), String> {
        use ir::ExprKind::*;
        self.bytes += std::mem::size_of::<ir::Expr>();
        self.pending.push(Part::Type(&expr.ty));
        match &expr.kind {
            String(text) => self.bytes += text.len(),
            List(xs)
            | Tuple(xs)
            | Interpolate(xs)
            | Closure { captures: xs, .. }
            | CustomConstruct { fields: xs, .. }
            | Call { args: xs, .. } => self.expressions(xs),
            Invoke { callee, args } => {
                self.pending.push(Part::Expr(callee));
                self.expressions(args);
            }
            Try(value) | Field { value, .. } | Unary { value, .. } => {
                self.pending.push(Part::Expr(value))
            }
            Construct { value, .. } => self.pending.extend(value.iter().map(|v| Part::Expr(v))),
            Binary { left, right, .. } => {
                self.pending.extend([Part::Expr(left), Part::Expr(right)]);
            }
            If {
                condition,
                then_branch,
                else_branch,
            } => {
                self.pending
                    .extend([Part::Expr(condition), Part::Expr(then_branch)]);
                self.pending
                    .extend(else_branch.iter().map(|v| Part::Expr(v)));
            }
            Match { value, arms } => {
                self.pending.push(Part::Expr(value));
                for arm in arms {
                    self.pending
                        .extend([Part::Pattern(&arm.pattern), Part::Expr(&arm.body)]);
                    self.pending.extend(arm.guard.iter().map(Part::Expr));
                }
            }
            Block(statements) => {
                self.bytes += statements.len() * std::mem::size_of::<ir::Stmt>();
                for stmt in statements {
                    let value = match stmt {
                        ir::Stmt::Let { value, .. } | ir::Stmt::Expr(value) => value,
                    };
                    self.pending.push(Part::Expr(value));
                }
            }
            Lambda { .. } | FunctionValue { .. } => {
                return Err("unfinalized interactive closure".into())
            }
            Int(_) | Float(_) | Bool(_) | Local(_) | Unit => {}
        }
        Ok(())
    }
    fn expressions(&mut self, values: &'a [ir::Expr]) {
        self.pending.extend(values.iter().map(Part::Expr));
    }
    /// Traverse function and nominal arguments without expanding recursive nominal layouts.
    fn ty(&mut self, ty: &'a Type) {
        self.bytes += std::mem::size_of::<Type>();
        match ty {
            Type::List(a) | Type::Option(a) => self.pending.push(Part::Type(a)),
            Type::Result(a, b) => self.pending.extend([Part::Type(a), Part::Type(b)]),
            Type::Function(args, result) => {
                self.pending.extend(args.iter().map(Part::Type));
                self.pending.push(Part::Type(result));
            }
            Type::Named(name, args) => {
                self.bytes += name.len();
                self.pending.extend(args.iter().map(Part::Type));
            }
            Type::Tuple(args) => self.pending.extend(args.iter().map(Part::Type)),
            Type::Generic(name) => self.bytes += name.len(),
            _ => {}
        }
    }
    /// Retained pattern strings and nested structures are part of closure code storage.
    fn pattern(&mut self, pattern: &'a ir::Pattern) {
        self.bytes += std::mem::size_of::<ir::Pattern>();
        match pattern {
            ir::Pattern::Tuple(xs) | ir::Pattern::Variant { fields: xs, .. } => {
                self.pending.extend(xs.iter().map(Part::Pattern));
            }
            ir::Pattern::String(text) => self.bytes += text.len(),
            _ => {}
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    fn closure(program: Rc<ir::Program>) -> Value {
        Value::Closure(Rc::new(ClosureValue {
            program,
            function: ir::FunctionId(0),
            captures: Vec::new(),
        }))
    }
    fn program() -> Rc<ir::Program> {
        Rc::new(ir::Program {
            types: Vec::new(),
            functions: vec![ir::Function {
                id: ir::FunctionId(0),
                name: "retained".into(),
                params: Vec::new(),
                captures: Vec::new(),
                return_type: Type::String,
                local_count: 0,
                body: ir::Expr {
                    kind: ir::ExprKind::String("x".repeat(1_000_000)),
                    ty: Type::String,
                    span: crate::Span::default(),
                },
            }],
        })
    }
    #[test]
    fn unique_programs_count_toward_storage_but_shared_programs_count_once() {
        let independent: Vec<_> = (0..18).map(|_| closure(program())).collect();
        assert!(graph_budget(independent.iter())
            .unwrap_err()
            .contains("storage limit"));
        let shared = program();
        let aliases: Vec<_> = (0..100).map(|_| closure(shared.clone())).collect();
        assert!(graph_budget(aliases.iter()).is_ok());
    }
}
