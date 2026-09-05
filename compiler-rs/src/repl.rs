//! Persistent typed-IR evaluation for interactive language exploration.
use crate::{ast, check, ir, parse, runtime, Type};
use std::{collections::HashMap, io::BufRead, rc::Rc};

#[derive(Clone, Debug, PartialEq)]
enum Value {
    Int(i64),
    Float(f64),
    Bool(bool),
    String(Rc<String>),
    Unit,
    List(Rc<Vec<Value>>),
    Sum(usize, Rc<Vec<Value>>),
    Closure(Rc<ClosureValue>),
}
/// Closures retain their originating program because later entries can renumber functions.
#[derive(Clone, Debug)]
struct ClosureValue {
    program: Rc<ir::Program>,
    function: ir::FunctionId,
    captures: Vec<Value>,
}
impl PartialEq for ClosureValue {
    fn eq(&self, other: &Self) -> bool {
        std::ptr::eq(self, other)
    }
}
#[derive(Debug)]
enum Failure {
    Message(String),
    Return(Value),
}
type Eval<T> = Result<T, Failure>;
fn fault(message: impl Into<String>) -> Failure {
    Failure::Message(message.into())
}

/// An isolated session retaining successful definitions and values, never replaying effects.
#[derive(Default)]
pub struct Session {
    definitions: String,
    bindings: Vec<String>,
    values: HashMap<usize, Value>,
    statements: usize,
}
impl Session {
    /// Check an entry before evaluating it; failed entries never replace prior bindings.
    pub fn evaluate(&mut self, source: &str) -> Result<String, String> {
        let declaration =
            source.trim_start().starts_with("fn ") || source.trim_start().starts_with("type ");
        let binding = source.trim_start().starts_with("let ");
        let definitions = if declaration {
            format!("{}\n{source}\n", self.definitions)
        } else {
            self.definitions.clone()
        };
        let mut body = self.bindings.join("\n");
        if !declaration {
            body.push('\n');
            body.push_str(source);
        }
        if body.trim().is_empty() {
            body.push_str("()");
        }
        let mut indented = String::new();
        for line in body.lines() {
            indented.push_str("    ");
            indented.push_str(line);
            indented.push('\n');
        }
        let program = format!("{definitions}\nfn main():\n{indented}");
        let syntax = parse::parse(&program).map_err(|e| e.message)?;
        let typed = Rc::new(check::check(&syntax).map_err(|e| e.message)?);
        let main = typed
            .functions
            .iter()
            .find(|f| f.name == "main")
            .ok_or("missing REPL entry")?;
        let ir::ExprKind::Block(statements) = &main.body.kind else {
            return Err("invalid REPL entry block".into());
        };
        if declaration {
            self.definitions = definitions;
            return Ok(String::new());
        }
        let mut machine = Machine {
            program: typed.clone(),
            locals: self.values.clone(),
            output: String::new(),
            steps: 0,
            depth: 0,
        };
        let result = machine.statements(&statements[self.statements..]);
        let value = match result {
            Ok(value) => value,
            Err(Failure::Message(message)) => return Err(message),
            Err(Failure::Return(_)) => return Err("return outside an interactive function".into()),
        };
        graph_budget(machine.locals.values())?;
        if binding {
            self.bindings.push(source.into());
            self.values = machine.locals;
            self.statements = statements.len();
        } else if value != Value::Unit {
            let ty = match statements.last() {
                Some(ir::Stmt::Expr(expr)) => &expr.ty,
                _ => &Type::Unit,
            };
            machine.output.push_str(&format!(
                "{} : {}\n",
                display_typed(&value, ty, &syntax, &mut 4096),
                type_name(ty)
            ));
        }
        Ok(machine.output)
    }
}
struct Machine {
    program: Rc<ir::Program>,
    locals: HashMap<usize, Value>,
    output: String,
    steps: usize,
    depth: usize,
}
impl Machine {
    /// Bound evaluator recursion and work independently of compiler syntax limits.
    fn expression(&mut self, expr: &ir::Expr) -> Eval<Value> {
        self.steps += 1;
        if self.steps > 100_000 || self.depth >= 128 {
            return Err(fault("interactive evaluation limit exceeded"));
        }
        self.depth += 1;
        let result = self.node(expr);
        self.depth -= 1;
        result
    }
    /// Evaluate one typed node using resolved identities and semantic values.
    fn node(&mut self, expr: &ir::Expr) -> Eval<Value> {
        use ir::ExprKind::*;
        match &expr.kind {
            Closure { function, captures } => self.closure(*function, captures),
            Invoke { callee, args } => {
                let callee = self.expression(callee)?;
                let args = self.arguments(args)?;
                self.invoke(&callee, args)
            }
            Lambda { .. } | FunctionValue { .. } => Err(fault("unfinalized interactive closure")),
            Interpolate(parts) => self.interpolate(parts),
            Int(n) => Ok(Value::Int(*n)),
            Float(n) => Ok(Value::Float(*n)),
            Bool(v) => Ok(Value::Bool(*v)),
            String(s) => Ok(Value::String(Rc::new(s.clone()))),
            Unit => Ok(Value::Unit),
            Local(id) => self
                .locals
                .get(&id.0)
                .cloned()
                .ok_or_else(|| fault("missing interactive local")),
            List(values) => Ok(Value::List(Rc::new(self.arguments(values)?))),
            Tuple(values) => Ok(Value::Sum(0, Rc::new(self.arguments(values)?))),
            CustomConstruct { tag, fields } => {
                Ok(Value::Sum(*tag, Rc::new(self.arguments(fields)?)))
            }
            Field { value, index } => self.field(value, *index),
            Construct { constructor, value } => self.construct(*constructor, value.as_deref()),
            Try(value) => match self.expression(value)? {
                Value::Sum(0, fields) if fields.len() == 1 => Ok(fields[0].clone()),
                value @ Value::Sum(1, _) => Err(Failure::Return(value)),
                _ => Err(fault("invalid Result")),
            },
            Unary { op, value } => unary(*op, self.expression(value)?),
            Binary { op, left, right } => self.binary(*op, left, right),
            Call { target, args } => {
                let args = self.arguments(args)?;
                self.call(*target, args)
            }
            If {
                condition,
                then_branch,
                else_branch,
            } => self.conditional(condition, then_branch, else_branch.as_deref()),
            Match { value, arms } => {
                let value = self.expression(value)?;
                self.matching(&value, arms)
            }
            Block(statements) => {
                let previous = self.locals.clone();
                let result = self.statements(statements);
                self.locals = previous;
                result
            }
        }
    }
    /// Evaluate each capture once and retain the code that assigned its function identity.
    fn closure(&mut self, function: ir::FunctionId, captures: &[ir::Expr]) -> Eval<Value> {
        let captures = self.arguments(captures)?;
        Ok(Value::Closure(Rc::new(ClosureValue {
            program: self.program.clone(),
            function,
            captures,
        })))
    }
    /// Read a checked structural or nominal field from immutable storage.
    fn field(&mut self, value: &ir::Expr, index: usize) -> Eval<Value> {
        match self.expression(value)? {
            Value::Sum(_, fields) => fields
                .get(index)
                .cloned()
                .ok_or_else(|| fault("invalid field")),
            _ => Err(fault("invalid record")),
        }
    }
    /// Preserve semantic sum tags without depending on the native transport encoding.
    fn construct(
        &mut self,
        constructor: crate::Constructor,
        value: Option<&ir::Expr>,
    ) -> Eval<Value> {
        let tag = usize::from(matches!(
            constructor,
            crate::Constructor::None | crate::Constructor::Err
        ));
        let fields = match value {
            Some(value) => vec![self.expression(value)?],
            None => Vec::new(),
        };
        Ok(Value::Sum(tag, Rc::new(fields)))
    }
    /// Convert scalar interpolation parts in order while bounding the resulting string.
    fn interpolate(&mut self, parts: &[ir::Expr]) -> Eval<Value> {
        let mut text = String::new();
        for part in parts {
            let value = self.expression(part)?;
            let next = match value {
                Value::String(s) => s.to_string(),
                other => display(&other),
            };
            if text.len() + next.len() > 1024 * 1024 {
                return Err(fault("interactive string limit exceeded"));
            }
            text.push_str(&next);
        }
        Ok(Value::String(Rc::new(text)))
    }
    /// Discard the true branch value for a conditional with no else, preserving effects.
    fn conditional(
        &mut self,
        condition: &ir::Expr,
        yes: &ir::Expr,
        no: Option<&ir::Expr>,
    ) -> Eval<Value> {
        match self.expression(condition)? {
            Value::Bool(true) => {
                let value = self.expression(yes)?;
                Ok(if no.is_some() { value } else { Value::Unit })
            }
            Value::Bool(false) => no.map_or(Ok(Value::Unit), |expr| self.expression(expr)),
            _ => Err(fault("invalid condition")),
        }
    }
    /// Evaluate arguments once from left to right.
    fn arguments(&mut self, args: &[ir::Expr]) -> Eval<Vec<Value>> {
        args.iter().map(|arg| self.expression(arg)).collect()
    }
    /// Evaluate an incremental statement suffix, keeping its top-level bindings.
    fn statements(&mut self, statements: &[ir::Stmt]) -> Eval<Value> {
        let mut result = Value::Unit;
        for statement in statements {
            result = match statement {
                ir::Stmt::Let { id, value } => {
                    let value = self.expression(value)?;
                    self.locals.insert(id.0, value);
                    Value::Unit
                }
                ir::Stmt::Expr(value) => self.expression(value)?,
            };
        }
        Ok(result)
    }
    /// Respect short-circuit control flow before evaluating a binary right operand.
    fn binary(&mut self, op: ast::BinaryOp, left: &ir::Expr, right: &ir::Expr) -> Eval<Value> {
        let left = self.expression(left)?;
        if (op == ast::BinaryOp::And && left == Value::Bool(false))
            || (op == ast::BinaryOp::Or && left == Value::Bool(true))
        {
            return Ok(left);
        }
        binary(op, left, self.expression(right)?)
    }
    /// Run a resolved function in its own locals and catch Result propagation at its boundary.
    fn call(&mut self, target: ir::CallTarget, args: Vec<Value>) -> Eval<Value> {
        match target {
            ir::CallTarget::Builtin(builtin) => self.builtin(builtin, args),
            ir::CallTarget::Runtime(id) => self.runtime(id, args),
            ir::CallTarget::Function(id) => self.function(self.program.clone(), id, &[], args),
        }
    }
    /// Invoke captured code from its immutable originating program, never a rebuilt ID table.
    fn invoke(&mut self, value: &Value, args: Vec<Value>) -> Eval<Value> {
        let Value::Closure(closure) = value else {
            return Err(fault("value is not callable"));
        };
        self.function(
            closure.program.clone(),
            closure.function,
            &closure.captures,
            args,
        )
    }
    /// Restore caller code and locals on success, error, or Result propagation.
    fn function(
        &mut self,
        program: Rc<ir::Program>,
        id: ir::FunctionId,
        captures: &[Value],
        args: Vec<Value>,
    ) -> Eval<Value> {
        let function = program
            .functions
            .iter()
            .find(|f| f.id == id)
            .ok_or_else(|| fault("unknown function"))?;
        if function.params.len() != args.len() || function.captures.len() != captures.len() {
            return Err(fault("invalid function environment or argument count"));
        }
        let previous_program = std::mem::replace(&mut self.program, program.clone());
        let previous = std::mem::take(&mut self.locals);
        self.locals
            .extend(function.params.iter().zip(args).map(|(p, v)| (p.id.0, v)));
        self.locals.extend(
            function
                .captures
                .iter()
                .zip(captures)
                .map(|(p, v)| (p.id.0, v.clone())),
        );
        let result = self.expression(&function.body);
        self.locals = previous;
        self.program = previous_program;
        match result {
            Err(Failure::Return(value)) => Ok(value),
            other => other,
        }
    }
    /// Match nested tags before binding their fields; failed guards restore the arm scope.
    fn matching(&mut self, value: &Value, arms: &[ir::MatchArm]) -> Eval<Value> {
        for arm in arms {
            let previous = self.locals.clone();
            let matched = pattern(&arm.pattern, value, &mut self.locals);
            let guard = if matched {
                arm.guard
                    .as_ref()
                    .map_or(Ok(Value::Bool(true)), |g| self.expression(g))
            } else {
                Ok(Value::Bool(false))
            };
            let result = match guard {
                Ok(Value::Bool(true)) => Some(self.expression(&arm.body)),
                Ok(_) => None,
                Err(e) => Some(Err(e)),
            };
            self.locals = previous;
            if let Some(result) = result {
                return result;
            }
        }
        Err(fault("nonexhaustive interactive match"))
    }
}

/// Match only types already validated by the checker.
fn pattern(pattern: &ir::Pattern, value: &Value, locals: &mut HashMap<usize, Value>) -> bool {
    use ir::Pattern::*;
    match (pattern, value) {
        (Wildcard, _) => true,
        (Bind(id), value) => {
            locals.insert(id.0, value.clone());
            true
        }
        (Int(a), Value::Int(b)) => a == b,
        (Bool(a), Value::Bool(b)) => a == b,
        (String(a), Value::String(b)) => a == b.as_ref(),
        (Tuple(fields), Value::Sum(0, values)) => {
            fields.len() == values.len()
                && fields
                    .iter()
                    .zip(values.iter())
                    .all(|(p, v)| self::pattern(p, v, locals))
        }
        (Variant { tag, fields }, Value::Sum(actual, values)) => {
            tag == actual
                && fields.len() == values.len()
                && fields
                    .iter()
                    .zip(values.iter())
                    .all(|(p, v)| self::pattern(p, v, locals))
        }
        (
            Constructor {
                constructor,
                binding,
            },
            Value::Sum(tag, fields),
        ) => {
            let expected = usize::from(matches!(
                constructor,
                crate::Constructor::None | crate::Constructor::Err
            ));
            if expected != *tag {
                return false;
            }
            if let Some(id) = binding {
                if let Some(value) = fields.first() {
                    locals.insert(id.0, value.clone());
                } else {
                    return false;
                }
            }
            true
        }
        _ => false,
    }
}
/// Evaluate scalar unary operators with Fern's numeric domains.
fn unary(op: ast::UnaryOp, value: Value) -> Eval<Value> {
    match (op, value) {
        (ast::UnaryOp::Negate, Value::Int(n)) => Ok(Value::Int(n.wrapping_neg())),
        (ast::UnaryOp::Negate, Value::Float(n)) => Ok(Value::Float(-n)),
        (ast::UnaryOp::Not, Value::Bool(v)) => Ok(Value::Bool(!v)),
        _ => Err(fault("invalid unary operands")),
    }
}
/// Evaluate scalar arithmetic; integer zero division is an interactive diagnostic.
fn binary(op: ast::BinaryOp, left: Value, right: Value) -> Eval<Value> {
    use ast::BinaryOp::*;
    if matches!(op, Eq | Ne) {
        let equal = left == right;
        return Ok(Value::Bool(if op == Eq { equal } else { !equal }));
    }
    match (left, right) {
        (Value::Int(a), Value::Int(b)) => Ok(match op {
            Add => Value::Int(a.wrapping_add(b)),
            Subtract => Value::Int(a.wrapping_sub(b)),
            Multiply => Value::Int(a.wrapping_mul(b)),
            Divide | Remainder if b == 0 => return Err(fault("integer division by zero")),
            Divide => Value::Int(a.wrapping_div(b)),
            Remainder => Value::Int(a.wrapping_rem(b)),
            Lt => Value::Bool(a < b),
            Le => Value::Bool(a <= b),
            Gt => Value::Bool(a > b),
            Ge => Value::Bool(a >= b),
            _ => return Err(fault("invalid integer operation")),
        }),
        (Value::Float(a), Value::Float(b)) => Ok(match op {
            Add => Value::Float(a + b),
            Subtract => Value::Float(a - b),
            Multiply => Value::Float(a * b),
            Divide => Value::Float(a / b),
            Lt => Value::Bool(a < b),
            Le => Value::Bool(a <= b),
            Gt => Value::Bool(a > b),
            Ge => Value::Bool(a >= b),
            _ => return Err(fault("invalid Float operation")),
        }),
        (Value::String(a), Value::String(b)) if op == Add => {
            builtins::concat_strings(a.as_str(), b.as_str())
        }
        (Value::Bool(a), Value::Bool(b)) => {
            Ok(Value::Bool(if op == And { a && b } else { a || b }))
        }
        _ => Err(fault("invalid binary operands")),
    }
}
/// Render interactive values without exposing internal pointers.
fn display(value: &Value) -> String {
    match value {
        Value::Int(n) => n.to_string(),
        Value::Float(n) => float_text(*n),
        Value::Bool(v) => v.to_string(),
        Value::String(s) => format!("{s:?}"),
        Value::Unit => "()".into(),
        Value::Closure(_) => "<function>".into(),
        Value::List(values) => format!(
            "[{}]",
            values.iter().map(display).collect::<Vec<_>>().join(", ")
        ),
        Value::Sum(tag, fields) => format!(
            "variant {tag}({})",
            fields.iter().map(display).collect::<Vec<_>>().join(", ")
        ),
    }
}
/// Spell common semantic types for interactive results.
fn type_name(ty: &Type) -> String {
    match ty {
        Type::Function(args, result) => format!(
            "({}) -> {}",
            args.iter().map(type_name).collect::<Vec<_>>().join(", "),
            type_name(result)
        ),
        Type::List(a) => format!("List({})", type_name(a)),
        Type::Option(a) => format!("Option({})", type_name(a)),
        Type::Result(a, b) => format!("Result({}, {})", type_name(a), type_name(b)),
        Type::Tuple(args) => format!(
            "({})",
            args.iter().map(type_name).collect::<Vec<_>>().join(", ")
        ),
        Type::Named(name, args) => {
            if args.is_empty() {
                name.clone()
            } else {
                format!(
                    "{name}({})",
                    args.iter().map(type_name).collect::<Vec<_>>().join(", ")
                )
            }
        }
        Type::Native(native) => format!("Tui.{native:?}"),
        _ => format!("{ty:?}"),
    }
}

#[path = "repl/builtins.rs"]
mod builtins;
#[path = "repl/functions.rs"]
mod functions;
#[path = "repl/storage.rs"]
mod storage;

/// Bound retained immutable graphs using unique Rc identities, including shared subtrees.
fn graph_budget<'a>(values: impl Iterator<Item = &'a Value>) -> Result<(), String> {
    let mut pending: Vec<_> = values.collect();
    let mut seen = std::collections::HashSet::new();
    let mut programs = std::collections::HashSet::new();
    let mut bytes = 0usize;
    let mut count = 0usize;
    while let Some(value) = pending.pop() {
        count += 1;
        match value {
            Value::String(s) => {
                if seen.insert(Rc::as_ptr(s) as usize) {
                    bytes = bytes.saturating_add(s.len());
                }
            }
            Value::List(xs) | Value::Sum(_, xs) => {
                if seen.insert(Rc::as_ptr(xs) as usize) {
                    bytes = bytes.saturating_add(xs.len() * std::mem::size_of::<Value>());
                    pending.extend(xs.iter());
                }
            }
            Value::Closure(closure) => {
                if seen.insert(Rc::as_ptr(closure) as usize) {
                    bytes =
                        bytes.saturating_add(closure.captures.len() * std::mem::size_of::<Value>());
                    pending.extend(closure.captures.iter());
                }
                if programs.insert(Rc::as_ptr(&closure.program) as usize) {
                    let (code_bytes, code_nodes) = storage::program_size(&closure.program)?;
                    bytes = bytes.saturating_add(code_bytes);
                    count = count.saturating_add(code_nodes);
                }
            }
            _ => {}
        }
        if bytes > 16 * 1024 * 1024 || count > 200_000 {
            return Err("interactive value storage limit exceeded".into());
        }
    }
    Ok(())
}
/// Render semantic constructors and bounded previews of compound interactive values.
fn display_typed(value: &Value, ty: &Type, syntax: &ast::Program, budget: &mut usize) -> String {
    if *budget == 0 {
        return "…".into();
    }
    *budget -= 1;
    match (value, ty) {
        (Value::List(values), Type::List(ty)) => {
            let mut shown = values
                .iter()
                .take(64)
                .map(|v| display_typed(v, ty, syntax, budget))
                .collect::<Vec<_>>();
            if values.len() > 64 {
                shown.push("…".into());
            }
            format!("[{}]", shown.join(", "))
        }
        (Value::Sum(tag, fields), _) => {
            let (name, types) = constructor_types(*tag, ty, syntax);
            let shown = fields
                .iter()
                .zip(types)
                .take(64)
                .map(|(v, t)| display_typed(v, &t, syntax, budget))
                .collect::<Vec<_>>()
                .join(", ");
            if matches!(ty, Type::Tuple(_)) {
                format!("({shown}{})", if fields.len() == 1 { "," } else { "" })
            } else if fields.is_empty() {
                name
            } else {
                format!("{name}({shown})")
            }
        }
        _ => display(value),
    }
}
/// Recover constructor spellings from semantic types and the parsed declaration registry.
fn constructor_types(tag: usize, ty: &Type, syntax: &ast::Program) -> (String, Vec<Type>) {
    match ty {
        Type::Option(a) => {
            if tag == 0 {
                ("Some".into(), vec![*a.clone()])
            } else {
                ("None".into(), vec![])
            }
        }
        Type::Result(a, b) => {
            if tag == 0 {
                ("Ok".into(), vec![*a.clone()])
            } else {
                ("Err".into(), vec![*b.clone()])
            }
        }
        Type::Tuple(types) => (String::new(), types.clone()),
        Type::Named(name, args) => {
            if let Some(decl) = syntax.types.iter().find(|d| &d.name == name) {
                if let Some(variant) = decl.variants.get(tag) {
                    let names: HashMap<_, _> = decl
                        .parameters
                        .iter()
                        .cloned()
                        .zip(args.iter().cloned())
                        .collect();
                    return (
                        variant.name.clone(),
                        variant
                            .fields
                            .iter()
                            .map(|f| substitute_type(&f.ty, &names))
                            .collect(),
                    );
                }
            }
            (name.clone(), vec![])
        }
        _ => (type_name(ty), vec![]),
    }
}
/// Substitute only source generic parameters when displaying a concrete nominal value.
fn substitute_type(ty: &Type, names: &HashMap<String, Type>) -> Type {
    match ty {
        Type::Generic(name) => names.get(name).cloned().unwrap_or_else(|| ty.clone()),
        Type::List(a) => Type::List(Box::new(substitute_type(a, names))),
        Type::Option(a) => Type::Option(Box::new(substitute_type(a, names))),
        Type::Result(a, b) => Type::Result(
            Box::new(substitute_type(a, names)),
            Box::new(substitute_type(b, names)),
        ),
        Type::Function(args, result) => Type::Function(
            args.iter().map(|a| substitute_type(a, names)).collect(),
            Box::new(substitute_type(result, names)),
        ),
        Type::Tuple(args) => Type::Tuple(args.iter().map(|a| substitute_type(a, names)).collect()),
        Type::Named(name, args) => Type::Named(
            name.clone(),
            args.iter().map(|a| substitute_type(a, names)).collect(),
        ),
        _ => ty.clone(),
    }
}

/// Match the native %.17g double rendering using bounded, safe Rust formatting.
fn float_text(value: f64) -> String {
    if value.is_nan() {
        return "nan".into();
    }
    if value.is_infinite() {
        return if value.is_sign_negative() {
            "-inf"
        } else {
            "inf"
        }
        .into();
    }
    let scientific = format!("{value:.16e}");
    let (mantissa, exponent) = scientific.split_once('e').unwrap();
    let exponent: i32 = exponent.parse().unwrap();
    if !(-4..17).contains(&exponent) {
        let mantissa = mantissa.trim_end_matches('0').trim_end_matches('.');
        format!(
            "{mantissa}e{}{number:02}",
            if exponent < 0 { "-" } else { "+" },
            number = exponent.unsigned_abs()
        )
    } else {
        let places = (16 - exponent).max(0) as usize;
        let text = format!("{value:.places$}");
        if text.contains('.') {
            text.trim_end_matches('0').trim_end_matches('.').into()
        } else {
            text
        }
    }
}

/// Run a bounded line-oriented session; a blank line submits an indented definition.
/// Prompts appear only for a terminal, keeping piped sessions deterministic.
pub fn serve(
    mut input: impl std::io::BufRead,
    mut output: impl std::io::Write,
    interactive: bool,
) -> Result<(), String> {
    let mut session = Session::default();
    let mut pending = String::new();
    if interactive {
        writeln!(
            output,
            "Fern interactive session. :help for commands; blank line submits a block."
        )
        .map_err(|e| e.to_string())?;
    }
    loop {
        if interactive {
            write!(
                output,
                "{}",
                if pending.is_empty() {
                    "fern> "
                } else {
                    "...   "
                }
            )
            .and_then(|()| output.flush())
            .map_err(|e| e.to_string())?;
        }
        let mut line = String::new();
        let read = std::io::Read::take(input.by_ref(), 1024 * 1024 + 1)
            .read_line(&mut line)
            .map_err(|e| e.to_string())?;
        if line.len() + pending.len() > 1024 * 1024 {
            return Err("interactive input limit exceeded".into());
        }
        let entry = line.trim_end_matches(['\r', '\n']);
        if pending.is_empty() && read != 0 && entry.starts_with(':') {
            match entry {
                ":quit"|":q"=>break,":reset"=>session=Session::default(),
                ":help"=>writeln!(output,"Enter expressions, let bindings, or typed functions. Commands: :help :reset :quit. Native-only APIs report a diagnostic here.").map_err(|e|e.to_string())?,
                _=>writeln!(output,"error: unknown interactive command").map_err(|e|e.to_string())?,
            }
            continue;
        }
        if !pending.is_empty() || parse::line_continues(entry) {
            if !entry.trim().is_empty() {
                pending.push_str(entry);
                pending.push('\n');
                continue;
            }
        } else {
            pending.push_str(entry);
        }
        if !pending.trim().is_empty() {
            match session.evaluate(&pending) {
                Ok(text) => write!(output, "{text}").map_err(|e| e.to_string())?,
                Err(message) => writeln!(output, "error: {message}").map_err(|e| e.to_string())?,
            }
        }
        pending.clear();
        if read == 0 {
            break;
        }
    }
    Ok(())
}
