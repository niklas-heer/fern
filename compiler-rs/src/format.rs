//! Canonical AST formatting with source-anchored comments and structural validation.
use crate::ast::{self, BinaryOp, Expr, ExprKind, Pattern, PatternKind, Stmt, UnaryOp};
use crate::{parse, Constructor, Diagnostic, Span, Type};

type Result<T> = std::result::Result<T, Diagnostic>;

/// Format supported Fern source using four-space indentation and canonical spacing.
/// Preserves comments in order and validates the rendered syntax tree before returning.
/// Invalid/unsupported source returns a diagnostic; no C formatter is invoked.
pub fn format(source: &str) -> Result<String> {
    let program = parse::parse(source)?;
    let lines = Renderer { source }.program(&program)?;
    let comments = comments(source);
    let output = attach_comments(source, lines, &comments);
    let formatted = parse::parse(&output).map_err(|error| {
        Diagnostic::new(
            Span::default(),
            format!(
                "cannot safely format this expression layout: {}",
                error.message
            ),
        )
    })?;
    if structural(program) != structural(formatted) {
        return Err(Diagnostic::new(
            Span::default(),
            "formatting would change the syntax tree; original source was preserved",
        ));
    }
    if comments
        .iter()
        .map(|comment| &comment.text)
        .ne(self::comments(&output).iter().map(|comment| &comment.text))
    {
        return Err(Diagnostic::new(
            Span::default(),
            "cannot preserve comment order in this layout",
        ));
    }
    Ok(output)
}

struct Line {
    indent: usize,
    text: String,
    anchor: usize,
}

/// Construct a rendered line with its original source anchor.
fn line(indent: usize, text: impl Into<String>, anchor: usize) -> Line {
    Line {
        indent,
        text: text.into(),
        anchor,
    }
}

struct Renderer<'a> {
    source: &'a str,
}

impl Renderer<'_> {
    /// Render declarations in source order so neighboring comments retain context.
    fn program(&self, program: &ast::Program) -> Result<Vec<Line>> {
        let mut declarations = Vec::new();
        if let Some(module) = &program.module {
            let anchor = module_anchor(self.source);
            declarations.push((anchor, vec![line(0, format!("module {module}"), anchor)]));
        }
        for import in &program.imports {
            let mut text = if import.public {
                "pub import ".to_owned()
            } else {
                "import ".to_owned()
            };
            text.push_str(&import.module);
            if let Some(items) = &import.items {
                if items == &["*"] {
                    text.push_str(".*");
                } else {
                    text.push_str(&format!(".{{{}}}", items.join(", ")));
                }
            }
            if let Some(alias) = &import.alias {
                text.push_str(&format!(" as {alias}"));
            }
            declarations.push((import.span.start, vec![line(0, text, import.span.start)]));
        }
        for declaration in &program.types {
            declarations.push((
                declaration.span.start,
                self.declaration(declaration, program.exports.contains(&declaration.name))?,
            ));
        }
        for function in &program.functions {
            declarations.push((
                function.span.start,
                self.function(function, program.exports.contains(&function.name))?,
            ));
        }
        declarations.sort_by_key(|(anchor, _)| *anchor);
        let mut lines = Vec::new();
        for (anchor, declaration) in declarations {
            if !lines.is_empty() {
                lines.push(line(0, "", anchor));
            }
            lines.extend(declaration);
        }
        Ok(lines)
    }

    /// Render a record's named fields or a sum's variant payload declarations.
    fn declaration(&self, declaration: &ast::TypeDecl, public: bool) -> Result<Vec<Line>> {
        let mut header = format!(
            "{}type {}",
            if public { "pub " } else { "" },
            declaration.name
        );
        if !declaration.parameters.is_empty() {
            header.push_str(&format!("({})", declaration.parameters.join(", ")));
        }
        header.push(':');
        let mut lines = vec![line(0, header, declaration.span.start)];
        for variant in &declaration.variants {
            if declaration.record {
                for field in &variant.fields {
                    lines.push(line(1, field_text(field)?, field.span.start));
                }
            } else {
                let mut text = variant.name.clone();
                if !variant.fields.is_empty() {
                    let fields = variant
                        .fields
                        .iter()
                        .map(field_text)
                        .collect::<Result<Vec<_>>>()?;
                    text.push_str(&format!("({})", fields.join(", ")));
                }
                lines.push(line(1, text, variant.span.start));
            }
        }
        Ok(lines)
    }

    /// Render a signature while preserving omitted return annotations and body shape.
    fn function(&self, function: &ast::Function, public: bool) -> Result<Vec<Line>> {
        let params = function
            .params
            .iter()
            .map(|param| Ok(format!("{}: {}", param.name, type_text(&param.ty)?)))
            .collect::<Result<Vec<_>>>()?;
        let mut header = format!(
            "{}fn {}({})",
            if public { "pub " } else { "" },
            function.name,
            params.join(", ")
        );
        if let Some(ty) = &function.return_type {
            header.push_str(&format!(" -> {}", type_text(ty)?));
        }
        header.push(':');
        self.suite(header, function.span.start, &function.body, 0)
    }

    /// Keep inline suites inline and indent block suites one canonical level.
    fn suite(
        &self,
        header: String,
        anchor: usize,
        body: &Expr,
        indent: usize,
    ) -> Result<Vec<Line>> {
        if let ExprKind::Block(statements) = &body.kind {
            let mut lines = vec![line(indent, header, anchor)];
            lines.extend(self.statements(statements, indent + 1)?);
            Ok(lines)
        } else {
            let mut lines = self.expression(body, indent)?;
            if let Some(first) = lines.first_mut() {
                first.text = format!("{header} {}", first.text);
                first.anchor = anchor;
            }
            Ok(lines)
        }
    }

    /// Preserve text and value boundaries while formatting nested scalar expressions.
    fn interpolation(&self, parts: &[ast::StringPart], indent: usize) -> Result<String> {
        let mut text = String::from("\"");
        for part in parts {
            match part {
                ast::StringPart::Text(value) => {
                    let escaped = quote(value);
                    text.push_str(&escaped[1..escaped.len() - 1]);
                }
                ast::StringPart::Value(value) => {
                    text.push('{');
                    text.push_str(&self.inline(value, indent)?);
                    text.push('}');
                }
            }
        }
        text.push('"');
        Ok(text)
    }

    /// Render immutable bindings and expression statements without merging scopes.
    fn statements(&self, statements: &[Stmt], indent: usize) -> Result<Vec<Line>> {
        let mut lines = Vec::new();
        for statement in statements {
            match statement {
                Stmt::LetPattern {
                    pattern,
                    annotation,
                    value,
                    span,
                } => {
                    let mut prefix = format!("let {}", pattern_text(pattern));
                    if let Some(ty) = annotation {
                        prefix.push_str(&format!(": {}", type_text(ty)?));
                    }
                    let mut value = self.expression(value, indent)?;
                    if let Some(first) = value.first_mut() {
                        first.text = format!("{prefix} = {}", first.text);
                        first.anchor = span.start;
                    }
                    lines.extend(value);
                }
                Stmt::Expr(value) => lines.extend(self.expression(value, indent)?),
                Stmt::Let {
                    name,
                    annotation,
                    value,
                    span,
                } => {
                    let mut prefix = format!("let {name}");
                    if let Some(ty) = annotation {
                        prefix.push_str(&format!(": {}", type_text(ty)?));
                    }
                    let mut value = self.expression(value, indent)?;
                    if let Some(first) = value.first_mut() {
                        first.text = format!("{prefix} = {}", first.text);
                        first.anchor = span.start;
                    }
                    lines.extend(value);
                }
            }
        }
        Ok(lines)
    }

    /// Render expressions with explicit grouping where it does not interfere with layout.
    fn expression(&self, expression: &Expr, indent: usize) -> Result<Vec<Line>> {
        let text = match &expression.kind {
            ExprKind::Int(value) => value.to_string(),
            ExprKind::Float(value) => format!("{value:?}"),
            ExprKind::Bool(value) => value.to_string(),
            ExprKind::String(value) => quote(value),
            ExprKind::Interpolate(parts) => self.interpolation(parts, indent)?,
            ExprKind::Name(name) => name.clone(),
            ExprKind::Unit => "()".into(),
            ExprKind::Tuple(items) => tuple_text(self.arguments(items, indent)?, items.len()),
            ExprKind::List(items) => format!("[{}]", self.arguments(items, indent)?),
            ExprKind::Call { name, args } => return self.call(name, args, indent, expression.span),
            ExprKind::Apply { callee, args } => {
                return self.apply(callee, args, indent, expression.span)
            }
            ExprKind::Lambda { params, body } => {
                return self.lambda(params, body, indent, expression.span)
            }
            ExprKind::Pipe {
                value,
                name,
                args,
                position,
            } => return self.pipe(value, name, args, *position, indent, expression.span),
            ExprKind::Unary { op, value } => {
                return self.unary(*op, value, indent, expression.span)
            }
            ExprKind::Binary { op, left, right } => {
                return self.binary(*op, left, right, indent, expression.span)
            }
            ExprKind::Try(value) => return self.postfix(value, "?", indent, expression.span),
            ExprKind::Field { value, name } => {
                return self.postfix(value, &format!(".{name}"), indent, expression.span)
            }
            ExprKind::If {
                condition,
                then_branch,
                else_branch,
            } => {
                return self.conditional(
                    condition,
                    then_branch,
                    else_branch.as_deref(),
                    indent,
                    expression.span,
                )
            }
            ExprKind::Match { value, arms } => {
                return self.matching(value, arms, indent, expression.span)
            }
            ExprKind::Block(statements) => return self.statements(statements, indent),
        };
        Ok(vec![line(indent, text, expression.span.start)])
    }

    /// Require a delimiter-safe inline expression for arguments and guards.
    fn inline(&self, expression: &Expr, indent: usize) -> Result<String> {
        let lines = self.expression(expression, indent)?;
        if lines.len() != 1 {
            return Err(Diagnostic::new(
                expression.span,
                "cannot format a block expression inside delimiters",
            ));
        }
        Ok(lines
            .into_iter()
            .next()
            .map(|line| line.text)
            .unwrap_or_default())
    }

    /// Canonicalize both lambda spellings while preserving optional parameter annotations.
    fn lambda(
        &self,
        params: &[ast::LambdaParam],
        body: &Expr,
        indent: usize,
        span: Span,
    ) -> Result<Vec<Line>> {
        let params = params
            .iter()
            .map(|p| {
                Ok(match &p.annotation {
                    Some(ty) => format!("{}: {}", p.name, type_text(ty)?),
                    None => p.name.clone(),
                })
            })
            .collect::<Result<Vec<_>>>()?
            .join(", ");
        self.suite(format!("({params}) ->"), span.start, body, indent)
    }

    /// Render pipe placeholders through ordinary call layout, including callback blocks.
    fn pipe(
        &self,
        value: &Expr,
        name: &str,
        args: &[Expr],
        position: usize,
        indent: usize,
        span: Span,
    ) -> Result<Vec<Line>> {
        let mut args = args.to_vec();
        if position > args.len() {
            return Err(Diagnostic::new(span, "invalid pipe placeholder position"));
        }
        args.insert(
            position,
            Expr {
                kind: ExprKind::Name("_".into()),
                span,
            },
        );
        let callee = format!("({} |> {name}", self.inline(value, indent)?);
        let mut lines = self.call(&callee, &args, indent, span)?;
        if let Some(last) = lines.last_mut() {
            last.text.push(')');
        }
        Ok(lines)
    }

    /// Keep compact calls inline and place callback blocks within their own argument layout.
    fn call(&self, callee: &str, args: &[Expr], indent: usize, span: Span) -> Result<Vec<Line>> {
        let arguments = args
            .iter()
            .map(|arg| self.expression(arg, indent + 1))
            .collect::<Result<Vec<_>>>()?;
        if arguments.iter().all(|lines| lines.len() == 1) {
            let text = arguments
                .iter()
                .map(|lines| lines[0].text.clone())
                .collect::<Vec<_>>()
                .join(", ");
            return Ok(vec![line(indent, format!("{callee}({text})"), span.start)]);
        }
        let mut lines = vec![line(indent, format!("{callee}("), span.start)];
        let count = arguments.len();
        for (index, mut argument) in arguments.into_iter().enumerate() {
            if index + 1 < count {
                if argument.len() == 1 {
                    argument[0].text.push(',');
                } else {
                    argument.push(line(indent + 1, ",", args[index].span.end));
                }
            }
            lines.extend(argument);
        }
        lines.push(line(indent, ")", span.end));
        Ok(lines)
    }

    /// Parenthesize arbitrary callees so lambda bodies cannot capture the invocation suffix.
    fn apply(&self, callee: &Expr, args: &[Expr], indent: usize, span: Span) -> Result<Vec<Line>> {
        let callee = self.expression(callee, indent + 1)?;
        if callee.len() == 1 {
            return self.call(&format!("({})", callee[0].text), args, indent, span);
        }
        let mut lines = vec![line(indent, "(", span.start)];
        lines.extend(callee);
        lines.extend(self.call(")", args, indent, span)?);
        Ok(lines)
    }

    /// Render comma-separated positional values from their checked source syntax.
    fn arguments(&self, arguments: &[Expr], indent: usize) -> Result<String> {
        Ok(arguments
            .iter()
            .map(|argument| self.inline(argument, indent))
            .collect::<Result<Vec<_>>>()?
            .join(", "))
    }

    /// Preserve binary grouping and keep continuation operators outside nested blocks.
    fn binary(
        &self,
        op: BinaryOp,
        left: &Expr,
        right: &Expr,
        indent: usize,
        span: Span,
    ) -> Result<Vec<Line>> {
        let mut left = self.expression(left, indent)?;
        let mut right = self.expression(right, indent)?;
        let operator = binary_text(op);
        if left.len() == 1 && right.len() == 1 {
            return Ok(vec![line(
                indent,
                format!("({} {operator} {})", left[0].text, right[0].text),
                span.start,
            )]);
        }
        if left.len() == 1 {
            if let Some(first) = right.first_mut() {
                first.text = format!("{} {operator} {}", left[0].text, first.text);
                first.anchor = span.start;
            }
            Ok(right)
        } else {
            if let Some(first) = right.first_mut() {
                first.text = format!("{operator} {}", first.text);
            }
            left.extend(right);
            Ok(left)
        }
    }

    /// Keep unary expressions grouped while preserving block operand indentation.
    fn unary(&self, op: UnaryOp, value: &Expr, indent: usize, span: Span) -> Result<Vec<Line>> {
        let mut lines = self.expression(value, indent)?;
        let operator = if op == UnaryOp::Not { "not " } else { "-" };
        let inline = lines.len() == 1;
        if let Some(first) = lines.first_mut() {
            first.text = if inline {
                format!("{operator}({})", first.text)
            } else {
                format!("{operator}{}", first.text)
            };
            first.anchor = span.start;
        }
        Ok(lines)
    }

    /// Append fields/propagation after their complete operand rather than its last arm.
    fn postfix(&self, value: &Expr, suffix: &str, indent: usize, span: Span) -> Result<Vec<Line>> {
        let mut lines = self.expression(value, indent)?;
        if lines.len() == 1 {
            if let Some(first) = lines.first_mut() {
                first.text.push_str(suffix);
                first.anchor = span.start;
            }
        } else {
            lines.push(line(indent, suffix, span.end));
        }
        Ok(lines)
    }

    /// Render if/else expressions with their original inline versus block branch shapes.
    fn conditional(
        &self,
        condition: &Expr,
        then_branch: &Expr,
        else_branch: Option<&Expr>,
        indent: usize,
        span: Span,
    ) -> Result<Vec<Line>> {
        let condition = self.inline(condition, indent)?;
        let mut lines = self.suite(format!("if {condition}:"), span.start, then_branch, indent)?;
        if let Some(otherwise) = else_branch {
            let anchor = self.else_anchor(then_branch.span.end, otherwise.span.start);
            let other = self.suite("else:".into(), anchor, otherwise, indent)?;
            if lines.len() == 1 && other.len() == 1 {
                lines[0].text.push_str(&format!(" {}", other[0].text));
            } else {
                lines.extend(other);
            }
        }
        if lines.len() == 1 {
            lines[0].text = format!("({})", lines[0].text);
        }
        Ok(lines)
    }

    /// Find the else keyword between checked branch spans for inline comment anchoring.
    fn else_anchor(&self, start: usize, end: usize) -> usize {
        self.source
            .get(start..end)
            .and_then(|gap| gap.rfind("else").map(|offset| start + offset))
            .unwrap_or(end)
    }

    /// Render match arms with guard and nested-constructor spelling preserved.
    fn matching(
        &self,
        value: &Expr,
        arms: &[ast::MatchArm],
        indent: usize,
        span: Span,
    ) -> Result<Vec<Line>> {
        let mut lines = vec![line(
            indent,
            format!("match {}:", self.inline(value, indent)?),
            span.start,
        )];
        for arm in arms {
            let mut header = pattern_text(&arm.pattern);
            if let Some(guard) = &arm.guard {
                header.push_str(&format!(" if {}", self.inline(guard, indent + 1)?));
            }
            header.push_str(" ->");
            lines.extend(self.suite(header, arm.pattern.span.start, &arm.body, indent + 1)?);
        }
        Ok(lines)
    }
}

/// Locate a real module declaration instead of matching its name inside a comment.
fn module_anchor(source: &str) -> usize {
    let mut offset = 0;
    for line in source.split_inclusive('\n') {
        let trimmed = line.trim_start();
        if trimmed
            .strip_prefix("module")
            .is_some_and(|rest| rest.starts_with(char::is_whitespace))
        {
            return offset + line.len() - trimmed.len();
        }
        offset += line.len();
    }
    0
}

/// Render concrete/generic source type syntax; inference variables never originate in parsing.
fn type_text(ty: &Type) -> Result<String> {
    Ok(match ty {
        Type::Int => "Int".into(),
        Type::Float => "Float".into(),
        Type::Bool => "Bool".into(),
        Type::String => "String".into(),
        Type::Unit => "()".into(),
        Type::Native(ty) => ty.name().into(),
        Type::Function(params, result) => format!(
            "({}) -> {}",
            params
                .iter()
                .map(type_text)
                .collect::<Result<Vec<_>>>()?
                .join(", "),
            type_text(result)?
        ),
        Type::Tuple(fields) => tuple_text(
            fields
                .iter()
                .map(type_text)
                .collect::<Result<Vec<_>>>()?
                .join(", "),
            fields.len(),
        ),
        Type::List(value) => format!("List({})", type_text(value)?),
        Type::Option(value) => format!("Option({})", type_text(value)?),
        Type::Result(value, error) => {
            format!("Result({}, {})", type_text(value)?, type_text(error)?)
        }
        Type::Generic(name) => name.clone(),
        Type::Named(name, arguments) => {
            if arguments.is_empty() {
                name.clone()
            } else {
                format!(
                    "{name}({})",
                    arguments
                        .iter()
                        .map(type_text)
                        .collect::<Result<Vec<_>>>()?
                        .join(", ")
                )
            }
        }
        Type::Infer(_) => {
            return Err(Diagnostic::new(
                Span::default(),
                "cannot format an unresolved internal type",
            ))
        }
    })
}

/// Render a record or variant field with its optional label.
fn field_text(field: &ast::Field) -> Result<String> {
    let ty = type_text(&field.ty)?;
    Ok(field
        .name
        .as_ref()
        .map_or_else(|| ty.clone(), |name| format!("{name}: {ty}")))
}

/// Render literal, catchall and recursively nested constructor patterns.
fn pattern_text(pattern: &Pattern) -> String {
    match &pattern.kind {
        PatternKind::Tuple(fields) => tuple_text(
            fields
                .iter()
                .map(pattern_text)
                .collect::<Vec<_>>()
                .join(", "),
            fields.len(),
        ),
        PatternKind::Wildcard => "_".into(),
        PatternKind::Bind(name) => name.clone(),
        PatternKind::Int(value) => value.to_string(),
        PatternKind::Bool(value) => value.to_string(),
        PatternKind::String(value) => quote(value),
        PatternKind::NamedConstructor { name, fields } => {
            if fields.is_empty() {
                name.clone()
            } else {
                format!(
                    "{name}({})",
                    fields
                        .iter()
                        .map(pattern_text)
                        .collect::<Vec<_>>()
                        .join(", ")
                )
            }
        }
        PatternKind::Constructor {
            constructor,
            binding,
        } => {
            let name = match constructor {
                Constructor::Some => "Some",
                Constructor::None => "None",
                Constructor::Ok => "Ok",
                Constructor::Err => "Err",
            };
            if *constructor == Constructor::None {
                name.into()
            } else {
                format!("{name}({})", binding.as_deref().unwrap_or("_"))
            }
        }
    }
}

/// Escape supported string characters without altering Unicode or treating # as comments.
fn quote(value: &str) -> String {
    let mut output = String::from("\"");
    for character in value.chars() {
        match character {
            '\n' => output.push_str("\\n"),
            '\r' => output.push_str("\\r"),
            '\t' => output.push_str("\\t"),
            '"' => output.push_str("\\\""),
            '\\' => output.push_str("\\\\"),
            '{' => output.push_str("\\{"),
            '}' => output.push_str("\\}"),
            _ => output.push(character),
        }
    }
    output.push('"');
    output
}

/// Map an AST operator to its canonical Fern token.
fn binary_text(op: BinaryOp) -> &'static str {
    match op {
        BinaryOp::Add => "+",
        BinaryOp::Subtract => "-",
        BinaryOp::Multiply => "*",
        BinaryOp::Divide => "/",
        BinaryOp::Remainder => "%",
        BinaryOp::Eq => "==",
        BinaryOp::Ne => "!=",
        BinaryOp::Lt => "<",
        BinaryOp::Le => "<=",
        BinaryOp::Gt => ">",
        BinaryOp::Ge => ">=",
        BinaryOp::And => "and",
        BinaryOp::Or => "or",
    }
}

struct Comment {
    offset: usize,
    line: usize,
    indent: usize,
    inline: bool,
    text: String,
}

/// Collect real line comments, respecting escaped quotes and # within string literals.
fn comments(source: &str) -> Vec<Comment> {
    let mut output = Vec::new();
    let mut start = 0;
    for line in source.split_inclusive('\n') {
        let content = line.trim_end_matches(['\n', '\r']);
        if let Some(index) = parse::comment_offset(content) {
            output.push(Comment {
                offset: start + index,
                line: start,
                indent: line.bytes().take_while(|byte| *byte == b' ').count() / 4,
                inline: !line[..index].trim().is_empty(),
                text: line[index..].trim_end().into(),
            });
        }
        start += line.len();
    }
    output
}

/// Index source lines and rendered anchors once rather than rescanning them per comment.
fn comment_slots(source: &str, lines: &[Line], comments: &[Comment]) -> Vec<usize> {
    let mut starts = vec![0];
    starts.extend(
        source
            .bytes()
            .enumerate()
            .filter_map(|(index, byte)| (byte == b'\n').then_some(index + 1)),
    );
    let mut same_line = std::collections::BTreeMap::new();
    let mut anchors = Vec::with_capacity(lines.len());
    for (index, line) in lines.iter().enumerate() {
        let source_line = starts[starts
            .partition_point(|start| *start <= line.anchor)
            .saturating_sub(1)];
        if !line.text.is_empty() {
            same_line.insert(source_line, index);
        }
        anchors.push((line.anchor, index));
    }
    anchors.sort_unstable();
    let mut following = vec![lines.len(); anchors.len() + 1];
    for index in (0..anchors.len()).rev() {
        following[index] = following[index + 1].min(anchors[index].1);
    }
    comments
        .iter()
        .map(|comment| {
            let next =
                following[anchors.partition_point(|(anchor, _)| *anchor <= comment.offset)] * 2;
            if comment.inline {
                same_line
                    .get(&comment.line)
                    .map_or(next, |index| index * 2 + 1)
            } else {
                next
            }
        })
        .collect()
}

/// Attach comments monotonically to original line anchors, preserving their total order.
fn attach_comments(source: &str, lines: Vec<Line>, comments: &[Comment]) -> String {
    let mut before: Vec<Vec<(usize, String)>> = (0..=lines.len()).map(|_| Vec::new()).collect();
    let mut suffix: Vec<Option<String>> = (0..lines.len()).map(|_| None).collect();
    let mut previous_slot = 0;
    for (comment, desired) in comments.iter().zip(comment_slots(source, &lines, comments)) {
        let mut slot = desired.max(previous_slot);
        if slot % 2 == 1 && suffix[slot / 2].is_some() {
            slot += 1;
        }
        if slot % 2 == 1 {
            suffix[slot / 2] = Some(comment.text.clone());
        } else {
            let index = slot / 2;
            let indent = lines.get(index).map_or(comment.indent, |line| line.indent);
            before[index].push((indent, comment.text.clone()));
        }
        previous_slot = slot;
    }
    let mut output = String::new();
    for (index, line) in lines.iter().enumerate() {
        for (indent, text) in &before[index] {
            output.push_str(&format!("{}{text}\n", "    ".repeat(*indent)));
        }
        if !line.text.is_empty() {
            output.push_str(&"    ".repeat(line.indent));
            output.push_str(&line.text);
        }
        if let Some(comment) = &suffix[index] {
            output.push_str("  ");
            output.push_str(comment);
        }
        output.push('\n');
    }
    for (indent, text) in &before[lines.len()] {
        output.push_str(&format!("{}{text}\n", "    ".repeat(*indent)));
    }
    output
}

/// Compare source syntax independently of locations after parsing the rendered artifact.
fn structural(mut program: ast::Program) -> String {
    for function in &mut program.functions {
        function.span = Span::default();
        for param in &mut function.params {
            param.span = Span::default();
        }
        clear_expression(&mut function.body);
    }
    for declaration in &mut program.types {
        declaration.span = Span::default();
        for variant in &mut declaration.variants {
            variant.span = Span::default();
            for field in &mut variant.fields {
                field.span = Span::default();
            }
        }
    }
    for import in &mut program.imports {
        import.span = Span::default();
    }
    format!("{program:?}")
}

/// Clear expression/statement spans recursively, retaining every semantic AST field.
fn clear_expression(expression: &mut Expr) {
    expression.span = Span::default();
    match &mut expression.kind {
        ExprKind::Unary { value, .. } | ExprKind::Try(value) | ExprKind::Field { value, .. } => {
            clear_expression(value)
        }
        ExprKind::Binary { left, right, .. } => {
            clear_expression(left);
            clear_expression(right);
        }
        ExprKind::Pipe { value, args, .. } => {
            clear_expression(value);
            for arg in args {
                clear_expression(arg);
            }
        }
        ExprKind::Lambda { params, body } => {
            for param in params {
                param.span = Span::default();
            }
            clear_expression(body);
        }
        ExprKind::Apply { callee, args } => {
            clear_expression(callee);
            for arg in args {
                clear_expression(arg);
            }
        }
        ExprKind::Interpolate(parts) => {
            for part in parts {
                if let ast::StringPart::Value(value) = part {
                    clear_expression(value);
                }
            }
        }
        ExprKind::Call { args, .. } | ExprKind::Tuple(args) | ExprKind::List(args) => {
            for argument in args {
                clear_expression(argument);
            }
        }
        ExprKind::If {
            condition,
            then_branch,
            else_branch,
        } => {
            clear_expression(condition);
            clear_expression(then_branch);
            if let Some(otherwise) = else_branch {
                clear_expression(otherwise);
            }
        }
        ExprKind::Match { value, arms } => clear_match(value, arms),
        ExprKind::Block(statements) => clear_statements(statements),
        ExprKind::Int(_)
        | ExprKind::Float(_)
        | ExprKind::Bool(_)
        | ExprKind::String(_)
        | ExprKind::Name(_)
        | ExprKind::Unit => {}
    }
}

/// Clear match locations and its nested guards, patterns and values together.
fn clear_match(value: &mut Expr, arms: &mut [ast::MatchArm]) {
    clear_expression(value);
    for arm in arms {
        arm.span = Span::default();
        clear_pattern(&mut arm.pattern);
        if let Some(guard) = &mut arm.guard {
            clear_expression(guard);
        }
        clear_expression(&mut arm.body);
    }
}

/// Clear block statement locations without changing binding patterns or initializers.
fn clear_statements(statements: &mut [Stmt]) {
    for statement in statements {
        match statement {
            Stmt::Expr(value) => clear_expression(value),
            Stmt::LetPattern {
                pattern,
                value,
                span,
                ..
            } => {
                *span = Span::default();
                clear_pattern(pattern);
                clear_expression(value);
            }
            Stmt::Let { value, span, .. } => {
                *span = Span::default();
                clear_expression(value);
            }
        }
    }
}

/// Remove pattern locations while keeping nested constructor structure intact.
fn clear_pattern(pattern: &mut Pattern) {
    pattern.span = Span::default();
    if let PatternKind::NamedConstructor { fields, .. } | PatternKind::Tuple(fields) =
        &mut pattern.kind
    {
        for field in fields {
            clear_pattern(field);
        }
    }
}

/// Preserve singleton tuple syntax while rendering positional fields.
fn tuple_text(fields: String, count: usize) -> String {
    format!("({fields}{})", if count == 1 { "," } else { "" })
}
