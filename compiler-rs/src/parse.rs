//! Independent, bounded lexer and recursive-descent parser for the prototype.
use crate::ast::{BinaryOp, Expr, ExprKind, Function, Param, Program, Stmt, UnaryOp};
use crate::{Diagnostic, Span, Type};

const MAX_SOURCE: usize = 1024 * 1024;
const MAX_TOKENS: usize = 65_536;
const MAX_DEPTH: usize = 128;
type ParseResult<T> = Result<T, Diagnostic>;

#[derive(Clone, Debug, PartialEq)]
enum Kind {
    Name(String),
    Number(String),
    Text(String),
    Left,
    Right,
    Colon,
    Comma,
    Dot,
    Arrow,
    Assign,
    Plus,
    Minus,
    Star,
    Slash,
    Percent,
    Eq,
    Ne,
    Lt,
    Le,
    Gt,
    Ge,
    Newline,
    Indent,
    Dedent,
    End,
}

#[derive(Clone, Debug)]
struct Token {
    kind: Kind,
    span: Span,
}

/// Parse UTF-8 Fern source into syntax, without invoking the C compiler.
/// Returns a source-located diagnostic for unsupported/malformed syntax or limits.
/// Input has no preconditions; source is limited to 1 MiB and 65,536 tokens.
pub fn parse(source: &str) -> ParseResult<Program> {
    if source.len() > MAX_SOURCE {
        return Err(Diagnostic::new(
            Span::default(),
            "source size exceeds 1 MiB prototype limit",
        ));
    }
    let tokens = lex(source)?;
    Parser {
        tokens,
        position: 0,
        depth: 0,
    }
    .program()
}

/// Append a located token, rejecting excessive input before allocation grows.
fn push(tokens: &mut Vec<Token>, kind: Kind, start: usize, end: usize) -> ParseResult<()> {
    if tokens.len() >= MAX_TOKENS {
        return Err(Diagnostic::new(Span { start, end }, "token limit exceeded"));
    }
    tokens.push(Token {
        kind,
        span: Span { start, end },
    });
    Ok(())
}

/// Tokenize lines with explicit indentation; blank/comment lines have no layout.
fn lex(source: &str) -> ParseResult<Vec<Token>> {
    let mut tokens = Vec::new();
    let mut levels = vec![0];
    let mut offset = 0;
    for line in source.split_inclusive('\n') {
        let line = line.strip_suffix('\n').unwrap_or(line);
        let content = line.strip_suffix('\r').unwrap_or(line);
        let indent = content.bytes().take_while(|b| *b == b' ').count();
        let rest = &content[indent..];
        if rest.starts_with('\t') {
            return Err(Diagnostic::new(
                Span {
                    start: offset + indent,
                    end: offset + indent + 1,
                },
                "tabs are not allowed for indentation; use spaces",
            ));
        }
        if !rest.is_empty() && !rest.starts_with('#') {
            layout(&mut tokens, &mut levels, indent, offset)?;
            lex_line(content, indent, offset, &mut tokens)?;
            push(
                &mut tokens,
                Kind::Newline,
                offset + content.len(),
                offset + content.len(),
            )?;
        }
        offset += line.len() + usize::from(offset + line.len() < source.len());
    }
    for _ in 1..levels.len() {
        push(&mut tokens, Kind::Dedent, source.len(), source.len())?;
    }
    push(&mut tokens, Kind::End, source.len(), source.len())?;
    Ok(tokens)
}

/// Emit indentation changes, requiring dedents to match an earlier level.
fn layout(
    tokens: &mut Vec<Token>,
    levels: &mut Vec<usize>,
    indent: usize,
    offset: usize,
) -> ParseResult<()> {
    let current = levels.last().copied().unwrap_or(0);
    match indent.cmp(&current) {
        std::cmp::Ordering::Greater => {
            if levels.len() >= MAX_DEPTH {
                return Err(Diagnostic::new(
                    Span {
                        start: offset,
                        end: offset + indent,
                    },
                    "indentation depth limit exceeded",
                ));
            }
            levels.push(indent);
            push(tokens, Kind::Indent, offset, offset + indent)?;
        }
        std::cmp::Ordering::Less => {
            for _ in 0..levels.len() {
                if levels.last().copied().unwrap_or(0) <= indent {
                    break;
                }
                levels.pop();
                push(tokens, Kind::Dedent, offset, offset + indent)?;
            }
            if levels.last().copied() != Some(indent) {
                return Err(Diagnostic::new(
                    Span {
                        start: offset,
                        end: offset + indent,
                    },
                    "inconsistent indentation",
                ));
            }
        }
        std::cmp::Ordering::Equal => {}
    }
    Ok(())
}

/// Scan one physical line with byte offsets; strings retain UTF-8 content.
fn lex_line(line: &str, mut at: usize, offset: usize, tokens: &mut Vec<Token>) -> ParseResult<()> {
    let bytes = line.as_bytes();
    for _ in 0..=bytes.len() {
        if at >= bytes.len() || bytes[at] == b'#' {
            return Ok(());
        }
        if bytes[at] == b' ' {
            at += 1;
            continue;
        }
        let start = at;
        let kind = match bytes[at] {
            b'"' => {
                let (text, end) = string(line, at, offset)?;
                at = end;
                Kind::Text(text)
            }
            b'0'..=b'9' => {
                at += 1;
                while at < bytes.len() && bytes[at].is_ascii_digit() {
                    at += 1;
                }
                if at < bytes.len()
                    && (bytes[at].is_ascii_alphabetic() || matches!(bytes[at], b'_' | b'.'))
                {
                    return Err(Diagnostic::new(
                        Span {
                            start: offset + start,
                            end: offset + at + 1,
                        },
                        "unsupported numeric literal; expected decimal Int",
                    ));
                }
                Kind::Number(line[start..at].to_owned())
            }
            b'a'..=b'z' | b'A'..=b'Z' | b'_' => {
                at += 1;
                while at < bytes.len() && (bytes[at].is_ascii_alphanumeric() || bytes[at] == b'_') {
                    at += 1;
                }
                Kind::Name(line[start..at].to_owned())
            }
            _ => {
                let (kind, width) = punctuation(&line[at..], offset + at)?;
                at += width;
                kind
            }
        };
        push(tokens, kind, offset + start, offset + at)?;
    }
    Ok(())
}

/// Decode supported escapes; reject interpolation and multiline strings explicitly.
fn string(line: &str, start: usize, offset: usize) -> ParseResult<(String, usize)> {
    let span = Span {
        start: offset + start,
        end: offset + line.len(),
    };
    if line[start..].starts_with("\"\"\"") {
        return Err(Diagnostic::new(
            span,
            "multiline strings are unsupported in the Rust prototype",
        ));
    }
    let mut text = String::new();
    let mut chars = line[start + 1..].char_indices();
    while let Some((i, c)) = chars.next() {
        match c {
            '"' => return Ok((text, start + 1 + i + 1)),
            '{' | '}' => {
                return Err(Diagnostic::new(
                    span,
                    "string interpolation is unsupported in the Rust prototype",
                ))
            }
            '\\' => {
                let escaped = match chars.next().map(|(_, c)| c) {
                    Some('n') => '\n',
                    Some('r') => '\r',
                    Some('t') => '\t',
                    Some('"') => '"',
                    Some('\\') => '\\',
                    _ => {
                        return Err(Diagnostic::new(
                            span,
                            "invalid or unsupported string escape",
                        ))
                    }
                };
                text.push(escaped);
            }
            '\0' => {
                return Err(Diagnostic::new(
                    span,
                    "NUL is unsupported in prototype strings",
                ))
            }
            _ => text.push(c),
        }
    }
    Err(Diagnostic::new(span, "unterminated string literal"))
}

/// Recognize punctuation or report unsupported characters at a UTF-8 boundary.
fn punctuation(rest: &str, start: usize) -> ParseResult<(Kind, usize)> {
    let pairs = [
        ("->", Kind::Arrow),
        ("==", Kind::Eq),
        ("!=", Kind::Ne),
        ("<=", Kind::Le),
        (">=", Kind::Ge),
    ];
    for (text, kind) in pairs {
        if rest.starts_with(text) {
            return Ok((kind, 2));
        }
    }
    let c = rest.chars().next().unwrap_or('\0');
    let kind = match c {
        '(' => Kind::Left,
        ')' => Kind::Right,
        ':' => Kind::Colon,
        ',' => Kind::Comma,
        '.' => Kind::Dot,
        '=' => Kind::Assign,
        '+' => Kind::Plus,
        '-' => Kind::Minus,
        '*' => Kind::Star,
        '/' => Kind::Slash,
        '%' => Kind::Percent,
        '<' => Kind::Lt,
        '>' => Kind::Gt,
        _ => {
            return Err(Diagnostic::new(
                Span {
                    start,
                    end: start + c.len_utf8(),
                },
                format!("unsupported character {c:?} in Rust prototype"),
            ))
        }
    };
    Ok((kind, 1))
}

struct Parsed {
    node: Expr,
    depth: usize,
}

/// Construct syntax only when its eventual recursive traversal/drop is bounded.
fn expression(kind: ExprKind, span: Span, depth: usize) -> ParseResult<Parsed> {
    if depth > MAX_DEPTH {
        return Err(Diagnostic::new(span, "expression depth limit exceeded"));
    }
    Ok(Parsed {
        node: Expr { kind, span },
        depth,
    })
}

struct Parser {
    tokens: Vec<Token>,
    position: usize,
    depth: usize,
}

impl Parser {
    /// Borrow the current token; construction always appends an End sentinel.
    fn current(&self) -> &Token {
        &self.tokens[self.position]
    }

    /// Advance without moving past the sentinel; return the consumed token.
    fn take(&mut self) -> Token {
        let token = self.current().clone();
        if self.position + 1 < self.tokens.len() {
            self.position += 1;
        }
        token
    }

    /// Consume a punctuation token when it matches.
    fn eat(&mut self, kind: &Kind) -> bool {
        if &self.current().kind == kind {
            self.take();
            true
        } else {
            false
        }
    }

    /// Check a keyword without treating arbitrary identifiers as syntax.
    fn word(&self, word: &str) -> bool {
        matches!(&self.current().kind, Kind::Name(name) if name == word)
    }

    /// Require punctuation and report an actionable diagnostic on mismatch.
    fn expect(&mut self, kind: Kind, message: &str) -> ParseResult<Token> {
        if self.current().kind == kind {
            Ok(self.take())
        } else {
            Err(self.error(message))
        }
    }

    /// Locate a parser error at the token that could not be consumed.
    fn error(&self, message: &str) -> Diagnostic {
        Diagnostic::new(self.current().span, message)
    }

    /// Parse all top-level functions, rejecting unsupported declarations.
    fn program(&mut self) -> ParseResult<Program> {
        let mut functions = Vec::new();
        for _ in 0..self.tokens.len() {
            if self.eat(&Kind::Newline) {
                continue;
            }
            if self.current().kind == Kind::End {
                return Ok(Program { functions });
            }
            if !self.word("fn") {
                return Err(self.error("expected fn; other top-level declarations are unsupported in the Rust prototype"));
            }
            functions.push(self.function()?);
        }
        Err(self.error("parser token limit exceeded"))
    }

    /// Parse a function with explicit parameter types and optional return type.
    fn function(&mut self) -> ParseResult<Function> {
        let start = self.take().span.start;
        let (name, _) = self.name()?;
        self.expect(Kind::Left, "expected '(' after function name")?;
        let mut params = Vec::new();
        for _ in 0..self.tokens.len() {
            if self.eat(&Kind::Right) {
                break;
            }
            let (name, span) = self.name()?;
            self.expect(Kind::Colon, "prototype parameters require explicit types")?;
            let ty = self.ty()?;
            params.push(Param { name, ty, span });
            if !self.eat(&Kind::Comma) {
                self.expect(Kind::Right, "expected ',' or ')' after parameter")?;
                break;
            }
        }
        let return_type = if self.eat(&Kind::Arrow) {
            Some(self.ty()?)
        } else {
            None
        };
        self.expect(Kind::Colon, "expected ':' before function body")?;
        let body = self.suite()?;
        if self.current().kind != Kind::End
            && self.current().kind != Kind::Dedent
            && self.current().kind != Kind::Newline
            && !self.previous_dedent()
        {
            return Err(self.error("expected end of line after function body"));
        }
        Ok(Function {
            name,
            params,
            return_type,
            span: Span {
                start,
                end: body.node.span.end,
            },
            body: body.node,
        })
    }

    /// Read a non-reserved identifier for a binding/function/parameter.
    fn name(&mut self) -> ParseResult<(String, Span)> {
        match self.current().kind.clone() {
            Kind::Name(name) if !reserved(&name) => {
                let span = self.take().span;
                Ok((name, span))
            }
            _ => {
                Err(self
                    .error("expected identifier; this syntax is unsupported in the Rust prototype"))
            }
        }
    }

    /// Parse supported primitive type annotations, including Fern's unit ().
    fn ty(&mut self) -> ParseResult<Type> {
        if self.eat(&Kind::Left) {
            self.expect(Kind::Right, "only unit () type is supported here")?;
            return Ok(Type::Unit);
        }
        let token = self.take();
        match &token.kind {
            Kind::Name(name) => match name.as_str() {
                "Int" => Ok(Type::Int),
                "Bool" => Ok(Type::Bool),
                "String" => Ok(Type::String),
                _ => Err(Diagnostic::new(
                    token.span,
                    "unsupported type; prototype supports Int, Bool, String, ()",
                )),
            },
            _ => Err(Diagnostic::new(token.span, "expected type annotation")),
        }
    }

    /// Parse one inline expression or an indented block of bindings/expressions.
    fn suite(&mut self) -> ParseResult<Parsed> {
        if !self.eat(&Kind::Newline) {
            return self.expr(0);
        }
        let start = self
            .expect(Kind::Indent, "expected indented block")?
            .span
            .start;
        let mut statements = Vec::new();
        let mut depth = 1;
        let mut end = start;
        for _ in 0..self.tokens.len() {
            if self.eat(&Kind::Dedent) {
                if statements.is_empty() {
                    return Err(self.error("expected expression in indented block"));
                }
                return expression(ExprKind::Block(statements), Span { start, end }, depth);
            }
            let (statement, statement_depth, statement_end) = self.statement()?;
            end = statement_end;
            depth = depth.max(statement_depth + 1);
            statements.push(statement);
            if !self.eat(&Kind::Newline)
                && self.current().kind != Kind::Dedent
                && !self.previous_dedent()
            {
                return Err(self.error("expected end of line after statement"));
            }
        }
        Err(self.error("unterminated block"))
    }

    /// Recognize layout already consumed by a nested expression's block.
    fn previous_dedent(&self) -> bool {
        self.position > 0 && self.tokens[self.position - 1].kind == Kind::Dedent
    }

    /// Parse an immutable binding or value statement and retain its tree depth.
    fn statement(&mut self) -> ParseResult<(Stmt, usize, usize)> {
        if self.word("let") {
            let start = self.take().span.start;
            let (name, _) = self.name()?;
            let annotation = if self.eat(&Kind::Colon) {
                Some(self.ty()?)
            } else {
                None
            };
            self.expect(Kind::Assign, "expected '=' in let binding")?;
            let value = self.expr(0)?;
            let end = value.node.span.end;
            Ok((
                Stmt::Let {
                    name,
                    annotation,
                    value: value.node,
                    span: Span { start, end },
                },
                value.depth,
                end,
            ))
        } else {
            let value = self.expr(0)?;
            let end = value.node.span.end;
            Ok((Stmt::Expr(value.node), value.depth, end))
        }
    }

    /// Guard recursive parser entry before processing precedence or nested syntax.
    fn expr(&mut self, minimum: u8) -> ParseResult<Parsed> {
        if self.depth >= MAX_DEPTH {
            return Err(self.error("parser depth limit exceeded"));
        }
        self.depth += 1;
        let result = self.binary(minimum);
        self.depth -= 1;
        result
    }

    /// Parse left-associative operators with Fern precedence and bounded AST depth.
    fn binary(&mut self, minimum: u8) -> ParseResult<Parsed> {
        let mut left = self.prefix()?;
        for _ in 0..self.tokens.len() {
            let Some((op, precedence)) = operator(&self.current().kind) else {
                break;
            };
            if precedence < minimum {
                break;
            }
            self.take();
            let right = self.expr(precedence + 1)?;
            let span = Span {
                start: left.node.span.start,
                end: right.node.span.end,
            };
            let depth = left.depth.max(right.depth) + 1;
            left = expression(
                ExprKind::Binary {
                    op,
                    left: Box::new(left.node),
                    right: Box::new(right.node),
                },
                span,
                depth,
            )?;
        }
        Ok(left)
    }

    /// Parse unary operators, literals, calls, grouping, and conditionals.
    fn prefix(&mut self) -> ParseResult<Parsed> {
        if self.word("if") {
            return self.conditional();
        }
        if self.word("not") || self.current().kind == Kind::Minus {
            return self.unary();
        }
        let token = self.take();
        match token.kind {
            Kind::Number(text) => integer(&text, token.span),
            Kind::Text(text) => expression(ExprKind::String(text), token.span, 1),
            Kind::Name(name) if name == "true" || name == "false" => {
                expression(ExprKind::Bool(name == "true"), token.span, 1)
            }
            Kind::Name(name) if !reserved(&name) => self.named(name, token.span),
            Kind::Left => {
                let value = self.expr(0)?;
                self.expect(Kind::Right, "expected ')' after grouped expression")?;
                Ok(value)
            }
            _ => Err(Diagnostic::new(
                token.span,
                "expected expression; this syntax is unsupported in the Rust prototype",
            )),
        }
    }

    /// Parse unary operands, accepting the Int minimum without positive overflow.
    fn unary(&mut self) -> ParseResult<Parsed> {
        let token = self.take();
        let op = if token.kind == Kind::Minus {
            UnaryOp::Negate
        } else {
            UnaryOp::Not
        };
        if op == UnaryOp::Negate {
            if let Kind::Number(text) = &self.current().kind {
                let text = format!("-{text}");
                let end = self.take().span.end;
                return integer(
                    &text,
                    Span {
                        start: token.span.start,
                        end,
                    },
                );
            }
        }
        let value = self.expr(6)?;
        let span = Span {
            start: token.span.start,
            end: value.node.span.end,
        };
        expression(
            ExprKind::Unary {
                op,
                value: Box::new(value.node),
            },
            span,
            value.depth + 1,
        )
    }

    /// Parse a qualified name and optional positional argument list.
    fn named(&mut self, mut name: String, mut span: Span) -> ParseResult<Parsed> {
        for _ in 0..self.tokens.len() {
            if !self.eat(&Kind::Dot) {
                break;
            }
            let (part, part_span) = self.name()?;
            name.push('.');
            name.push_str(&part);
            span.end = part_span.end;
        }
        if !self.eat(&Kind::Left) {
            return expression(ExprKind::Name(name), span, 1);
        }
        let mut args = Vec::new();
        let mut depth = 1;
        for _ in 0..self.tokens.len() {
            if self.current().kind == Kind::Right {
                span.end = self.take().span.end;
                break;
            }
            let arg = self.expr(0)?;
            depth = depth.max(arg.depth + 1);
            args.push(arg.node);
            if self.current().kind == Kind::Colon {
                return Err(self.error("labeled arguments are unsupported in the Rust prototype"));
            }
            if !self.eat(&Kind::Comma) {
                span.end = self
                    .expect(Kind::Right, "expected ',' or ')' after argument")?
                    .span
                    .end;
                break;
            }
        }
        expression(ExprKind::Call { name, args }, span, depth)
    }

    /// Parse inline or indented if/else expressions, including else: if chains.
    fn conditional(&mut self) -> ParseResult<Parsed> {
        let start = self.take().span.start;
        let condition = self.expr(0)?;
        self.expect(Kind::Colon, "expected ':' after if condition")?;
        let then_branch = self.suite()?;
        // A newline before else belongs to this conditional, not its enclosing block.
        if self.current().kind == Kind::Newline
            && self
                .tokens
                .get(self.position + 1)
                .is_some_and(|t| matches!(&t.kind, Kind::Name(n) if n == "else"))
        {
            self.take();
        }
        let mut end = then_branch.node.span.end;
        let mut depth = condition.depth.max(then_branch.depth) + 1;
        let else_branch = if self.word("else") {
            self.take();
            self.expect(Kind::Colon, "expected ':' after else")?;
            let value = self.suite()?;
            end = value.node.span.end;
            depth = depth.max(value.depth + 1);
            Some(Box::new(value.node))
        } else {
            None
        };
        expression(
            ExprKind::If {
                condition: Box::new(condition.node),
                then_branch: Box::new(then_branch.node),
                else_branch,
            },
            Span { start, end },
            depth,
        )
    }
}

/// Convert a decimal token to Fern's signed 64-bit Int with a stable diagnostic.
fn integer(text: &str, span: Span) -> ParseResult<Parsed> {
    let value = text.parse::<i64>().map_err(|_| {
        Diagnostic::new(
            span,
            "integer is outside Int range (-9223372036854775808..9223372036854775807)",
        )
    })?;
    expression(ExprKind::Int(value), span, 1)
}

/// Return the supported operator and its increasing binding strength.
fn operator(kind: &Kind) -> Option<(BinaryOp, u8)> {
    Some(match kind {
        Kind::Name(n) if n == "or" => (BinaryOp::Or, 0),
        Kind::Name(n) if n == "and" => (BinaryOp::And, 1),
        Kind::Eq => (BinaryOp::Eq, 2),
        Kind::Ne => (BinaryOp::Ne, 2),
        Kind::Lt => (BinaryOp::Lt, 3),
        Kind::Le => (BinaryOp::Le, 3),
        Kind::Gt => (BinaryOp::Gt, 3),
        Kind::Ge => (BinaryOp::Ge, 3),
        Kind::Plus => (BinaryOp::Add, 4),
        Kind::Minus => (BinaryOp::Subtract, 4),
        Kind::Star => (BinaryOp::Multiply, 5),
        Kind::Slash => (BinaryOp::Divide, 5),
        Kind::Percent => (BinaryOp::Remainder, 5),
        _ => return None,
    })
}

/// Keep Fern keywords out of bindings even when their syntax is unsupported.
fn reserved(name: &str) -> bool {
    matches!(
        name,
        "fn" | "let"
            | "if"
            | "else"
            | "true"
            | "false"
            | "and"
            | "or"
            | "not"
            | "pub"
            | "type"
            | "match"
            | "return"
            | "for"
            | "in"
            | "while"
            | "import"
            | "with"
            | "trait"
            | "impl"
            | "actor"
            | "receive"
            | "spawn"
            | "where"
            | "do"
            | "defer"
            | "as"
            | "module"
            | "break"
            | "continue"
            | "derive"
            | "newtype"
            | "send"
            | "after"
    )
}
