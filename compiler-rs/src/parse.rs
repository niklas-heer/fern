//! Independent, bounded lexer and recursive-descent parser for the prototype.
use crate::ast::{
    BinaryOp, Expr, ExprKind, Field, Function, Import, MatchArm, Param, Pattern, PatternKind,
    Program, Stmt, TypeDecl, UnaryOp, Variant,
};
use crate::{Constructor, Diagnostic, Span, Type};

const MAX_SOURCE: usize = 1024 * 1024;
const MAX_TOKENS: usize = 65_536;
const MAX_DEPTH: usize = 128;
type ParseResult<T> = Result<T, Diagnostic>;

#[derive(Clone, Debug, PartialEq)]
enum Kind {
    Name(String),
    Number(String),
    Text(String),
    StringOpen,
    StringClose,
    HoleOpen,
    HoleClose,
    Left,
    Right,
    LeftBracket,
    RightBracket,
    LeftBrace,
    RightBrace,
    Colon,
    Comma,
    Dot,
    Arrow,
    Pipe,
    Assign,
    Plus,
    Minus,
    Star,
    Slash,
    Percent,
    Question,
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
    let mut tokens: Vec<Token> = Vec::new();
    let mut levels = vec![0];
    let mut delimiters = Vec::new();
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
            if delimiters.is_empty() {
                if rest.starts_with("|>") && indent >= *levels.last().unwrap() {
                    if tokens
                        .last()
                        .is_some_and(|token| token.kind == Kind::Newline)
                    {
                        tokens.pop();
                    }
                } else {
                    layout(&mut tokens, &mut levels, indent, offset)?;
                }
            }
            let from = tokens.len();
            lex_line(content, indent, offset, &mut tokens)?;
            track_delimiters(&tokens[from..], &mut delimiters)?;
            if delimiters.is_empty() {
                push(
                    &mut tokens,
                    Kind::Newline,
                    offset + content.len(),
                    offset + content.len(),
                )?;
            }
        }
        offset += line.len() + usize::from(offset + line.len() < source.len());
    }
    if let Some(token) = delimiters.last() {
        return Err(Diagnostic::new(token.span, "unclosed delimiter"));
    }
    for _ in 1..levels.len() {
        push(&mut tokens, Kind::Dedent, source.len(), source.len())?;
    }
    push(&mut tokens, Kind::End, source.len(), source.len())?;
    Ok(tokens)
}

/// Validate delimiter pairing and suspend layout inside parenthesized/list syntax.
fn track_delimiters(tokens: &[Token], stack: &mut Vec<Token>) -> ParseResult<()> {
    for token in tokens {
        match token.kind {
            Kind::Left | Kind::LeftBracket | Kind::LeftBrace => {
                if stack.len() >= MAX_DEPTH {
                    return Err(Diagnostic::new(
                        token.span,
                        "delimiter depth limit exceeded",
                    ));
                }
                stack.push(token.clone());
            }
            Kind::Right | Kind::RightBracket | Kind::RightBrace => {
                let expected = if token.kind == Kind::Right {
                    Kind::Left
                } else if token.kind == Kind::RightBracket {
                    Kind::LeftBracket
                } else {
                    Kind::LeftBrace
                };
                if stack.pop().map(|opening| opening.kind) != Some(expected) {
                    return Err(Diagnostic::new(token.span, "mismatched closing delimiter"));
                }
            }
            _ => {}
        }
    }
    Ok(())
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
    while at < line.len() {
        if line.as_bytes()[at] == b'#' {
            break;
        }
        if line.as_bytes()[at] == b' ' {
            at += 1;
            continue;
        }
        at = lex_token(line, at, offset, tokens, 0)?;
    }
    Ok(())
}

/// Scan one expression token; embedded strings share the caller's token budget.
fn lex_token(
    line: &str,
    mut at: usize,
    offset: usize,
    tokens: &mut Vec<Token>,
    depth: usize,
) -> ParseResult<usize> {
    let bytes = line.as_bytes();
    let start = at;
    let kind = match bytes[at] {
        b'"' => return scan_string(line, at, offset, tokens, depth),
        b'0'..=b'9' => {
            at = if tokens.last().is_some_and(|t| t.kind == Kind::Dot) {
                let mut end = at;
                while end < bytes.len() && bytes[end].is_ascii_digit() {
                    end += 1;
                }
                end
            } else {
                number_end(line, at, offset)?
            };
            Kind::Number(line[start..at].into())
        }
        b'a'..=b'z' | b'A'..=b'Z' | b'_' => {
            at += 1;
            while at < bytes.len() && (bytes[at].is_ascii_alphanumeric() || bytes[at] == b'_') {
                at += 1;
            }
            Kind::Name(line[start..at].into())
        }
        _ => {
            let (kind, width) = punctuation(&line[at..], offset + at)?;
            at += width;
            kind
        }
    };
    push(tokens, kind, offset + start, offset + at)?;
    Ok(at)
}

struct StringScan {
    start: usize,
    at: usize,
    segment: usize,
    text: String,
    interpolated: bool,
}

/// Decode one quoted string, flattening interpolation tokens with exact byte locations.
fn scan_string(
    line: &str,
    start: usize,
    offset: usize,
    tokens: &mut Vec<Token>,
    depth: usize,
) -> ParseResult<usize> {
    let span = Span {
        start: offset + start,
        end: offset + line.len(),
    };
    if depth >= MAX_DEPTH {
        return Err(Diagnostic::new(span, "string nesting limit exceeded"));
    }
    if line[start..].starts_with("\"\"\"") {
        return Err(Diagnostic::new(
            span,
            "multiline strings are unsupported in the Rust prototype",
        ));
    }
    let mut state = StringScan {
        start,
        at: start + 1,
        segment: start + 1,
        text: String::new(),
        interpolated: false,
    };
    while state.at < line.len() {
        let c = line[state.at..]
            .chars()
            .next()
            .expect("remaining UTF-8 character");
        match c {
            '"' => return close_string(&mut state, offset, tokens),
            '{' => string_hole(&mut state, line, offset, tokens, depth)?,
            '}' => {
                return Err(Diagnostic::new(
                    Span {
                        start: offset + state.at,
                        end: offset + state.at + 1,
                    },
                    "unmatched '}' in string; escape literal braces with a backslash",
                ))
            }
            '\\' => scan_escape(&mut state, line, offset, span)?,
            '\0' => {
                return Err(Diagnostic::new(
                    span,
                    "NUL is unsupported in prototype strings",
                ))
            }
            _ => {
                state.text.push(c);
                state.at += c.len_utf8();
            }
        }
    }
    Err(Diagnostic::new(span, "unterminated string literal"))
}

/// Consume one escape at the current string cursor, retaining the original byte span.
fn scan_escape(state: &mut StringScan, line: &str, offset: usize, span: Span) -> ParseResult<()> {
    state.at += 1;
    let c = line
        .get(state.at..)
        .and_then(|rest| rest.chars().next())
        .ok_or_else(|| Diagnostic::new(span, "unterminated string escape"))?;
    state.text.push(string_escape(
        c,
        Span {
            start: offset + state.at - 1,
            end: offset + state.at + c.len_utf8(),
        },
    )?);
    state.at += c.len_utf8();
    Ok(())
}

/// Finish either a plain literal token or the final interpolated text segment.
fn close_string(
    state: &mut StringScan,
    offset: usize,
    tokens: &mut Vec<Token>,
) -> ParseResult<usize> {
    let text = Kind::Text(std::mem::take(&mut state.text));
    if state.interpolated {
        push(tokens, text, offset + state.segment, offset + state.at)?;
        push(
            tokens,
            Kind::StringClose,
            offset + state.at,
            offset + state.at + 1,
        )?;
    } else {
        push(tokens, text, offset + state.start, offset + state.at + 1)?;
    }
    Ok(state.at + 1)
}

/// Flush literal text before parsing a hole and resume immediately after its closing brace.
fn string_hole(
    state: &mut StringScan,
    line: &str,
    offset: usize,
    tokens: &mut Vec<Token>,
    depth: usize,
) -> ParseResult<()> {
    if !state.interpolated {
        push(
            tokens,
            Kind::StringOpen,
            offset + state.start,
            offset + state.start + 1,
        )?;
        state.interpolated = true;
    }
    push(
        tokens,
        Kind::Text(std::mem::take(&mut state.text)),
        offset + state.segment,
        offset + state.at,
    )?;
    push(
        tokens,
        Kind::HoleOpen,
        offset + state.at,
        offset + state.at + 1,
    )?;
    state.at = scan_hole(line, state.at + 1, offset, tokens, depth + 1)?;
    state.segment = state.at;
    Ok(())
}

/// Scan one interpolation expression, balancing braces without entering nested strings twice.
fn scan_hole(
    line: &str,
    mut at: usize,
    offset: usize,
    tokens: &mut Vec<Token>,
    depth: usize,
) -> ParseResult<usize> {
    let start = at;
    let mut braces = 0;
    while at < line.len() {
        match line.as_bytes()[at] {
            b' ' => {
                at += 1;
                continue;
            }
            b'}' if braces == 0 => {
                push(tokens, Kind::HoleClose, offset + at, offset + at + 1)?;
                return Ok(at + 1);
            }
            b'}' => braces -= 1,
            b'{' => {
                braces += 1;
                if braces >= MAX_DEPTH {
                    return Err(Diagnostic::new(
                        Span {
                            start: offset + start,
                            end: offset + at,
                        },
                        "interpolation nesting limit exceeded",
                    ));
                }
            }
            b'#' => break,
            _ => {}
        }
        at = lex_token(line, at, offset, tokens, depth)?;
    }
    Err(Diagnostic::new(
        Span {
            start: offset + start,
            end: offset + at,
        },
        "unterminated interpolation; expected '}'",
    ))
}

/// Decode Fern's supported escapes, including literal interpolation delimiters.
fn string_escape(c: char, span: Span) -> ParseResult<char> {
    Ok(match c {
        'n' => '\n',
        'r' => '\r',
        't' => '\t',
        '"' => '"',
        '\\' => '\\',
        '{' => '{',
        '}' => '}',
        _ => {
            return Err(Diagnostic::new(
                span,
                "invalid or unsupported string escape",
            ))
        }
    })
}

/// Return the trailing comment boundary using the same string grammar as the parser.
pub(crate) fn comment_offset(line: &str) -> Option<usize> {
    let mut tokens = Vec::new();
    lex_line(line, 0, 0, &mut tokens).ok()?;
    let end = tokens.last().map_or(0, |t| t.span.end);
    line.get(end..)?.find('#').map(|at| at + end)
}

/// Recognize punctuation or report unsupported characters at a UTF-8 boundary.
fn punctuation(rest: &str, start: usize) -> ParseResult<(Kind, usize)> {
    let pairs = [
        ("->", Kind::Arrow),
        ("|>", Kind::Pipe),
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
        '[' => Kind::LeftBracket,
        ']' => Kind::RightBracket,
        '{' => Kind::LeftBrace,
        '}' => Kind::RightBrace,
        ':' => Kind::Colon,
        ',' => Kind::Comma,
        '.' => Kind::Dot,
        '=' => Kind::Assign,
        '+' => Kind::Plus,
        '-' => Kind::Minus,
        '*' => Kind::Star,
        '/' => Kind::Slash,
        '%' => Kind::Percent,
        '?' => Kind::Question,
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

    /// Parse module declarations, imports, public exports, functions and custom types.
    fn program(&mut self) -> ParseResult<Program> {
        let mut program = Program::default();
        for _ in 0..self.tokens.len() {
            if self.eat(&Kind::Newline) {
                continue;
            }
            if self.current().kind == Kind::End {
                return Ok(program);
            }
            let public = if self.word("pub") {
                self.take();
                true
            } else {
                false
            };
            if self.word("fn") {
                let function = self.function()?;
                if public {
                    program.exports.push(function.name.clone());
                }
                program.functions.push(function);
            } else if self.word("type") {
                let declaration = self.type_declaration()?;
                if public {
                    program.exports.push(declaration.name.clone());
                }
                program.types.push(declaration);
            } else if self.word("import") {
                program.imports.push(self.import(public)?);
            } else if self.word("module") && !public {
                if program.module.is_some() {
                    return Err(self.error("duplicate module declaration"));
                }
                self.take();
                program.module = Some(self.qualified_name()?.0);
                self.line_end()?;
            } else {
                return Err(self.error("expected fn, type, import, or module declaration"));
            }
        }
        Err(self.error("parser token limit exceeded"))
    }

    /// Require a physical declaration boundary without consuming enclosing layout.
    fn line_end(&mut self) -> ParseResult<()> {
        if self.eat(&Kind::Newline) || self.current().kind == Kind::End {
            Ok(())
        } else {
            Err(self.error("expected end of line after declaration"))
        }
    }

    /// Read a dotted name without consuming import selectors after the final dot.
    fn qualified_name(&mut self) -> ParseResult<(String, Span)> {
        let (mut name, mut span) = self.name()?;
        for _ in 0..self.tokens.len() {
            if self.current().kind != Kind::Dot
                || !self
                    .tokens
                    .get(self.position + 1)
                    .is_some_and(|t| matches!(t.kind, Kind::Name(_)))
            {
                break;
            }
            self.take();
            let (part, next) = self.name()?;
            name.push('.');
            name.push_str(&part);
            span.end = next.end;
        }
        Ok((name, span))
    }

    /// Capture full/selective/wildcard imports and aliases without loading modules.
    fn import(&mut self, public: bool) -> ParseResult<Import> {
        let start = self.take().span.start;
        let (module, _) = self.qualified_name()?;
        let items = if self.eat(&Kind::Dot) {
            if self.eat(&Kind::Star) {
                Some(vec!["*".into()])
            } else {
                self.expect(
                    Kind::LeftBrace,
                    "expected '{items}' or '*' after import module",
                )?;
                let mut names = Vec::new();
                for _ in 0..self.tokens.len() {
                    if self.eat(&Kind::RightBrace) {
                        break;
                    }
                    names.push(self.name()?.0);
                    if !self.eat(&Kind::Comma) {
                        self.expect(Kind::RightBrace, "expected ',' or '}' after import item")?;
                        break;
                    }
                }
                if names.is_empty() {
                    return Err(self.error("selective import requires at least one item"));
                }
                Some(names)
            }
        } else {
            None
        };
        let alias = if self.word("as") {
            self.take();
            Some(self.name()?.0)
        } else {
            None
        };
        let end = self.tokens[self.position.saturating_sub(1)].span.end;
        self.line_end()?;
        Ok(Import {
            module,
            alias,
            items,
            public,
            span: Span { start, end },
        })
    }

    /// Parse generic parameter names and either record fields or sum constructors.
    fn type_declaration(&mut self) -> ParseResult<TypeDecl> {
        let start = self.take().span.start;
        let (name, _) = self.name()?;
        let mut parameters = Vec::new();
        if self.eat(&Kind::Left) {
            for _ in 0..self.tokens.len() {
                if self.eat(&Kind::Right) {
                    break;
                }
                parameters.push(self.name()?.0);
                if !self.eat(&Kind::Comma) {
                    self.expect(Kind::Right, "expected ',' or ')' after type parameter")?;
                    break;
                }
            }
        }
        self.expect(Kind::Colon, "expected ':' before type body")?;
        self.expect(Kind::Newline, "type declarations require an indented body")?;
        self.expect(Kind::Indent, "type declarations require an indented body")?;
        let record = self
            .tokens
            .get(self.position + 1)
            .is_some_and(|t| t.kind == Kind::Colon);
        let mut variants = Vec::new();
        let mut fields = Vec::new();
        for _ in 0..self.tokens.len() {
            if self.eat(&Kind::Dedent) {
                break;
            }
            if record {
                fields.push(self.declaration_field(true)?);
            } else {
                variants.push(self.variant()?);
            }
            self.line_end()?;
        }
        let end = self.tokens[self.position.saturating_sub(1)].span.end;
        let span = Span { start, end };
        if record {
            variants.push(Variant {
                name: name.clone(),
                fields,
                span,
            });
        }
        if variants.is_empty() {
            return Err(Diagnostic::new(span, "type requires fields or variants"));
        }
        Ok(TypeDecl {
            name,
            parameters,
            variants,
            record,
            span,
        })
    }

    /// Parse one optional-name field and retain its complete type source range.
    fn declaration_field(&mut self, require_name: bool) -> ParseResult<Field> {
        let start = self.current().span.start;
        let named = self
            .tokens
            .get(self.position + 1)
            .is_some_and(|t| t.kind == Kind::Colon);
        let name = if named || require_name {
            let name = self.name()?.0;
            self.expect(Kind::Colon, "expected ':' after record field name")?;
            Some(name)
        } else {
            None
        };
        let ty = self.ty()?;
        let end = self.tokens[self.position.saturating_sub(1)].span.end;
        Ok(Field {
            name,
            ty,
            span: Span { start, end },
        })
    }

    /// Parse a named sum constructor with positional or named payload fields.
    fn variant(&mut self) -> ParseResult<Variant> {
        let (name, mut span) = self.name()?;
        let mut fields = Vec::new();
        if self.eat(&Kind::Left) {
            for _ in 0..self.tokens.len() {
                if self.current().kind == Kind::Right {
                    span.end = self.take().span.end;
                    break;
                }
                fields.push(self.declaration_field(false)?);
                if !self.eat(&Kind::Comma) {
                    span.end = self
                        .expect(Kind::Right, "expected ',' or ')' after variant field")?
                        .span
                        .end;
                    break;
                }
            }
        }
        Ok(Variant { name, fields, span })
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

    /// Bound recursive concrete types independently of expression parsing.
    fn ty(&mut self) -> ParseResult<Type> {
        if self.depth >= MAX_DEPTH {
            return Err(self.error("type depth limit exceeded"));
        }
        self.depth += 1;
        let result = self.type_value();
        self.depth -= 1;
        result
    }

    /// Parse primitive, nominal, and generic types without resolving declarations.
    fn type_value(&mut self) -> ParseResult<Type> {
        if self.eat(&Kind::Left) {
            return self.tuple_type();
        }
        let (name, span) = self
            .qualified_name()
            .map_err(|_| self.error("expected type annotation"))?;
        let mut arguments = Vec::new();
        if self.eat(&Kind::Left) {
            for _ in 0..self.tokens.len() {
                if self.eat(&Kind::Right) {
                    break;
                }
                arguments.push(self.ty()?);
                if !self.eat(&Kind::Comma) {
                    self.expect(Kind::Right, "expected ',' or ')' after type argument")?;
                    break;
                }
            }
        }
        let arity = match name.as_str() {
            "List" | "Option" => Some(1),
            "Result" => Some(2),
            "Int" | "Float" | "Bool" | "String" | "Unit" => Some(0),
            _ => None,
        };
        if arity.is_some_and(|arity| arguments.len() != arity) {
            return Err(Diagnostic::new(
                span,
                format!("wrong type argument count for {name}; separate arguments with ','"),
            ));
        }
        Ok(match name.as_str() {
            "Int" => Type::Int,
            "Float" => Type::Float,
            "Unit" => Type::Unit,
            "Bool" => Type::Bool,
            "String" => Type::String,
            "List" => Type::List(Box::new(arguments.remove(0))),
            "Option" => Type::Option(Box::new(arguments.remove(0))),
            "Result" => {
                let error = arguments.remove(1);
                Type::Result(Box::new(arguments.remove(0)), Box::new(error))
            }
            _ if arguments.is_empty()
                && !name.contains('.')
                && name.starts_with(|c: char| c.is_ascii_lowercase()) =>
            {
                Type::Generic(name)
            }
            _ if crate::runtime::native_type(&name).is_some() && arguments.is_empty() => {
                Type::Native(crate::runtime::native_type(&name).expect("checked native name"))
            }
            _ => Type::Named(name, arguments),
        })
    }

    /// Parse Unit and comma-separated structural types after their opening delimiter.
    fn tuple_type(&mut self) -> ParseResult<Type> {
        if self.eat(&Kind::Right) {
            return Ok(Type::Unit);
        }
        let first = self.ty()?;
        if !self.eat(&Kind::Comma) {
            self.expect(Kind::Right, "expected ',' or ')' after type")?;
            return Ok(first);
        }
        let mut fields = vec![first];
        for _ in 0..self.tokens.len() {
            if self.eat(&Kind::Right) {
                break;
            }
            fields.push(self.ty()?);
            if !self.eat(&Kind::Comma) {
                self.expect(Kind::Right, "expected ',' or ')' after tuple type")?;
                break;
            }
        }
        Ok(Type::Tuple(fields))
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
            let pattern = if self.current().kind == Kind::Left {
                self.pattern()?
            } else {
                let (name, span) = self.name()?;
                Pattern {
                    kind: PatternKind::Bind(name),
                    span,
                }
            };
            let annotation = if self.eat(&Kind::Colon) {
                Some(self.ty()?)
            } else {
                None
            };
            self.expect(Kind::Assign, "expected '=' in let binding")?;
            let value = self.expr(0)?;
            let end = value.node.span.end;
            Ok((
                match pattern.kind {
                    PatternKind::Bind(name) => Stmt::Let {
                        name,
                        annotation,
                        value: value.node,
                        span: Span { start, end },
                    },
                    PatternKind::Wildcard => Stmt::Let {
                        name: "_".into(),
                        annotation,
                        value: value.node,
                        span: Span { start, end },
                    },
                    _ => Stmt::LetPattern {
                        pattern,
                        annotation,
                        value: value.node,
                        span: Span { start, end },
                    },
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
        let mut left = self.postfix()?;
        for _ in 0..self.tokens.len() {
            if minimum == 0 && self.eat(&Kind::Pipe) {
                let right = self.expr(1)?;
                left = pipe(left, right)?;
                continue;
            }
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

    /// Wrap postfix Result propagation before applying surrounding binary operators.
    fn postfix(&mut self) -> ParseResult<Parsed> {
        let mut value = self.prefix()?;
        for _ in 0..self.tokens.len() {
            if self.current().kind == Kind::Question {
                let span = Span {
                    start: value.node.span.start,
                    end: self.take().span.end,
                };
                value = expression(ExprKind::Try(Box::new(value.node)), span, value.depth + 1)?;
            } else if self.eat(&Kind::Dot) {
                let (name, end) = if let Kind::Number(number) = self.current().kind.clone() {
                    let token = self.take();
                    if !number.bytes().all(|b| b.is_ascii_digit()) {
                        return Err(Diagnostic::new(
                            token.span,
                            "tuple index must be a nonnegative integer",
                        ));
                    }
                    (number, token.span)
                } else {
                    self.name()?
                };
                let span = Span {
                    start: value.node.span.start,
                    end: end.end,
                };
                value = expression(
                    ExprKind::Field {
                        value: Box::new(value.node),
                        name,
                    },
                    span,
                    value.depth + 1,
                )?;
            } else {
                break;
            }
        }
        Ok(value)
    }

    /// Parse unary operators, literals, calls, grouping, and conditionals.
    fn prefix(&mut self) -> ParseResult<Parsed> {
        if self.word("if") {
            return self.conditional();
        }
        if self.word("match") {
            return self.match_expression();
        }
        if self.word("not") || self.current().kind == Kind::Minus {
            return self.unary();
        }
        let token = self.take();
        match token.kind {
            Kind::Number(text) => number(&text, token.span),
            Kind::Text(text) => expression(ExprKind::String(text), token.span, 1),
            Kind::StringOpen => self.interpolation(token.span),
            Kind::Name(name) if name == "true" || name == "false" => {
                expression(ExprKind::Bool(name == "true"), token.span, 1)
            }
            Kind::Name(name) if !reserved(&name) => self.named(name, token.span),
            Kind::LeftBracket => self.list(token.span),
            Kind::Left => self.parenthesized(token.span),
            _ => Err(Diagnostic::new(
                token.span,
                "expected expression; this syntax is unsupported in the Rust prototype",
            )),
        }
    }

    /// Parse embedded expressions using the ordinary grammar and one shared depth bound.
    fn interpolation(&mut self, mut span: Span) -> ParseResult<Parsed> {
        let mut parts = Vec::new();
        let mut depth = 1;
        for _ in 0..self.tokens.len() {
            let token = self.take();
            match token.kind {
                Kind::Text(text) => parts.push(crate::ast::StringPart::Text(text)),
                Kind::HoleOpen => {
                    let value = self.expr(0)?;
                    depth = depth.max(value.depth + 1);
                    self.expect(
                        Kind::HoleClose,
                        "expected '}' after interpolation expression",
                    )?;
                    parts.push(crate::ast::StringPart::Value(value.node));
                }
                Kind::StringClose => {
                    span.end = token.span.end;
                    return expression(ExprKind::Interpolate(parts), span, depth);
                }
                _ => {
                    return Err(Diagnostic::new(
                        token.span,
                        "expected string segment or interpolation",
                    ))
                }
            }
        }
        Err(Diagnostic::new(span, "unterminated interpolated string"))
    }

    /// Distinguish Unit, grouping, and structural tuples by a comma.
    fn parenthesized(&mut self, mut span: Span) -> ParseResult<Parsed> {
        if self.current().kind == Kind::Right {
            span.end = self.take().span.end;
            return expression(ExprKind::Unit, span, 1);
        }
        let first = self.expr(0)?;
        if !self.eat(&Kind::Comma) {
            self.expect(Kind::Right, "expected ',' or ')' after expression")?;
            return Ok(first);
        }
        let mut depth = first.depth + 1;
        let mut fields = vec![first.node];
        for _ in 0..self.tokens.len() {
            if self.current().kind == Kind::Right {
                span.end = self.take().span.end;
                break;
            }
            let value = self.expr(0)?;
            depth = depth.max(value.depth + 1);
            fields.push(value.node);
            if !self.eat(&Kind::Comma) {
                span.end = self
                    .expect(Kind::Right, "expected ',' or ')' after tuple field")?
                    .span
                    .end;
                break;
            }
        }
        expression(ExprKind::Tuple(fields), span, depth)
    }

    /// Parse immutable list literals with optional trailing commas.
    fn list(&mut self, mut span: Span) -> ParseResult<Parsed> {
        let mut items = Vec::new();
        let mut depth = 1;
        for _ in 0..self.tokens.len() {
            if self.current().kind == Kind::RightBracket {
                span.end = self.take().span.end;
                break;
            }
            let item = self.expr(0)?;
            depth = depth.max(item.depth + 1);
            items.push(item.node);
            if !self.eat(&Kind::Comma) {
                span.end = self
                    .expect(Kind::RightBracket, "expected ',' or ']' after list element")?
                    .span
                    .end;
                break;
            }
        }
        expression(ExprKind::List(items), span, depth)
    }

    /// Parse an indented sequence of pattern arms and bound its resulting tree.
    fn match_expression(&mut self) -> ParseResult<Parsed> {
        let start = self.take().span.start;
        let value = self.expr(0)?;
        self.expect(Kind::Colon, "expected ':' after match value")?;
        self.expect(Kind::Newline, "match requires an indented arm block")?;
        self.expect(Kind::Indent, "match requires an indented arm block")?;
        let mut arms = Vec::new();
        let mut depth = value.depth + 1;
        let mut end = value.node.span.end;
        for _ in 0..self.tokens.len() {
            if self.eat(&Kind::Dedent) {
                break;
            }
            let pattern = self.pattern()?;
            let guard = if self.word("if") {
                self.take();
                Some(self.expr(0)?)
            } else {
                None
            };
            if let Some(guard) = &guard {
                depth = depth.max(guard.depth + 1);
            }
            self.expect(Kind::Arrow, "expected '->' after match pattern")?;
            let body = self.suite()?;
            end = body.node.span.end;
            depth = depth.max(body.depth + 1);
            let span = Span {
                start: pattern.span.start,
                end,
            };
            arms.push(MatchArm {
                guard: guard.map(|value| value.node),
                pattern,
                body: body.node,
                span,
            });
            if !self.eat(&Kind::Newline)
                && self.current().kind != Kind::Dedent
                && !self.previous_dedent()
            {
                return Err(self.error("expected end of line after match arm"));
            }
        }
        if arms.is_empty() {
            return Err(self.error("match requires at least one arm"));
        }
        expression(
            ExprKind::Match {
                value: Box::new(value.node),
                arms,
            },
            Span { start, end },
            depth,
        )
    }

    /// Bound recursive patterns before parsing constructor payloads.
    fn pattern(&mut self) -> ParseResult<Pattern> {
        if self.depth >= MAX_DEPTH {
            return Err(self.error("pattern depth limit exceeded"));
        }
        self.depth += 1;
        let result = self.pattern_value();
        self.depth -= 1;
        result
    }

    /// Parse nested constructors, scalar literals, wildcard and lowercase bindings.
    fn pattern_value(&mut self) -> ParseResult<Pattern> {
        if self.current().kind == Kind::Left {
            return self.tuple_pattern();
        }
        if matches!(self.current().kind, Kind::Name(_)) && !self.word("true") && !self.word("false")
        {
            let (name, span) = self.qualified_name()?;
            if name == "_" {
                return Ok(Pattern {
                    kind: PatternKind::Wildcard,
                    span,
                });
            }
            if name
                .rsplit('.')
                .next()
                .is_some_and(|part| part.starts_with(|c: char| c.is_ascii_uppercase()))
            {
                return self.named_pattern(name, span);
            }
            return Ok(Pattern {
                kind: PatternKind::Bind(name),
                span,
            });
        }
        let token = self.take();
        let mut span = token.span;
        let kind = match token.kind {
            Kind::Number(text) => PatternKind::Int(pattern_integer(&text, span)?),
            Kind::Minus => {
                let number = self.take();
                span.end = number.span.end;
                let Kind::Number(text) = number.kind else {
                    return Err(Diagnostic::new(span, "expected integer pattern after '-'"));
                };
                PatternKind::Int(pattern_integer(&format!("-{text}"), span)?)
            }
            Kind::Text(text) => PatternKind::String(text),
            Kind::Name(name) if name == "true" || name == "false" => {
                PatternKind::Bool(name == "true")
            }
            _ => {
                return Err(Diagnostic::new(
                    span,
                    "unsupported pattern; expected literal, binding, wildcard or constructor",
                ))
            }
        };
        Ok(Pattern { kind, span })
    }

    /// Parse positional patterns, retaining singleton commas and grouping.
    fn tuple_pattern(&mut self) -> ParseResult<Pattern> {
        let mut span = self.take().span;
        let mut fields = Vec::new();
        if self.current().kind != Kind::Right {
            let first = self.pattern()?;
            if !self.eat(&Kind::Comma) {
                self.expect(Kind::Right, "expected ',' or ')' after pattern")?;
                return Ok(first);
            }
            fields.push(first);
            for _ in 0..self.tokens.len() {
                if self.current().kind == Kind::Right {
                    break;
                }
                fields.push(self.pattern()?);
                if !self.eat(&Kind::Comma) {
                    break;
                }
            }
        }
        span.end = self
            .expect(Kind::Right, "expected ',' or ')' after tuple pattern")?
            .span
            .end;
        Ok(Pattern {
            kind: PatternKind::Tuple(fields),
            span,
        })
    }

    /// Parse recursive constructor fields while retaining the compatible flat AST form.
    fn named_pattern(&mut self, name: String, mut span: Span) -> ParseResult<Pattern> {
        let mut fields = Vec::new();
        if self.eat(&Kind::Left) {
            for _ in 0..self.tokens.len() {
                if self.current().kind == Kind::Right {
                    span.end = self.take().span.end;
                    break;
                }
                fields.push(self.pattern()?);
                if !self.eat(&Kind::Comma) {
                    span.end = self
                        .expect(Kind::Right, "expected ',' or ')' after payload pattern")?
                        .span
                        .end;
                    break;
                }
            }
        }
        let kind = if let Some(constructor) = builtin_constructor(&name) {
            if constructor == Constructor::None && !fields.is_empty() {
                return Err(Diagnostic::new(
                    span,
                    "None patterns do not accept a payload",
                ));
            }
            if constructor == Constructor::None {
                PatternKind::Constructor {
                    constructor,
                    binding: None,
                }
            } else if fields.len() == 1
                && matches!(fields[0].kind, PatternKind::Bind(_) | PatternKind::Wildcard)
            {
                let binding = match &fields[0].kind {
                    PatternKind::Bind(name) => Some(name.clone()),
                    _ => None,
                };
                PatternKind::Constructor {
                    constructor,
                    binding,
                }
            } else {
                PatternKind::NamedConstructor { name, fields }
            }
        } else {
            PatternKind::NamedConstructor { name, fields }
        };
        Ok(Pattern { kind, span })
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
                return number(
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
            if self.current().kind != Kind::Dot
                || !matches!(
                    self.tokens.get(self.position + 1).map(|t| &t.kind),
                    Some(Kind::Name(_))
                )
            {
                break;
            }
            self.take();
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

/// Recognize only the built-in Option/Result constructors.
fn builtin_constructor(name: &str) -> Option<Constructor> {
    match name {
        "Some" => Some(Constructor::Some),
        "None" => Some(Constructor::None),
        "Ok" => Some(Constructor::Ok),
        "Err" => Some(Constructor::Err),
        _ => None,
    }
}

/// Parse a literal pattern with the same signed 64-bit range as expressions.
fn pattern_integer(text: &str, span: Span) -> ParseResult<i64> {
    text.parse::<i64>()
        .map_err(|_| Diagnostic::new(span, "integer pattern is outside Int range"))
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

/// Scan decimal integer/float tokens, rejecting malformed suffixes at their source.
fn number_end(line: &str, start: usize, offset: usize) -> ParseResult<usize> {
    let bytes = line.as_bytes();
    let mut at = start;
    while at < bytes.len() && bytes[at].is_ascii_digit() {
        at += 1;
    }
    if bytes.get(at) == Some(&b'.') {
        at += 1;
        let fraction = at;
        while at < bytes.len() && bytes[at].is_ascii_digit() {
            at += 1;
        }
        if at == fraction {
            return Err(Diagnostic::new(
                Span {
                    start: offset + start,
                    end: offset + at,
                },
                "expected digits after decimal point",
            ));
        }
    }
    if matches!(bytes.get(at), Some(b'e' | b'E')) {
        at += 1;
        if matches!(bytes.get(at), Some(b'+' | b'-')) {
            at += 1;
        }
        let exponent = at;
        while at < bytes.len() && bytes[at].is_ascii_digit() {
            at += 1;
        }
        if at == exponent {
            return Err(Diagnostic::new(
                Span {
                    start: offset + start,
                    end: offset + at,
                },
                "expected exponent digits",
            ));
        }
    }
    if bytes
        .get(at)
        .is_some_and(|b| b.is_ascii_alphabetic() || matches!(b, b'_' | b'.'))
    {
        return Err(Diagnostic::new(
            Span {
                start: offset + start,
                end: offset + at + 1,
            },
            "unsupported numeric literal suffix",
        ));
    }
    Ok(at)
}

/// Preserve decimal Float literals as IEEE doubles; overflowing literals are diagnosed.
fn number(text: &str, span: Span) -> ParseResult<Parsed> {
    if !text.contains(['.', 'e', 'E']) {
        return integer(text, span);
    }
    let value = text
        .parse::<f64>()
        .map_err(|_| Diagnostic::new(span, "invalid Float literal"))?;
    if !value.is_finite() {
        return Err(Diagnostic::new(span, "Float literal exceeds finite range"));
    }
    expression(ExprKind::Float(value), span, 1)
}

/// Record piped argument placement without reordering evaluation or duplicating its value.
fn pipe(left: Parsed, right: Parsed) -> ParseResult<Parsed> {
    let span = Span {
        start: left.node.span.start,
        end: right.node.span.end,
    };
    let depth = left.depth.max(right.depth) + 1;
    let (name, mut args) = match right.node.kind {
        ExprKind::Call { name, args } => (name, args),
        ExprKind::Name(name) => (name, Vec::new()),
        _ => {
            return Err(Diagnostic::new(
                span,
                "pipe target must be a function name or call",
            ))
        }
    };
    let positions: Vec<_> = args
        .iter()
        .enumerate()
        .filter_map(|(i, arg)| matches!(&arg.kind, ExprKind::Name(n) if n == "_").then_some(i))
        .collect();
    if positions.len() > 1 {
        return Err(Diagnostic::new(
            span,
            "pipe accepts at most one argument placeholder",
        ));
    }
    let position = positions.first().copied().unwrap_or(0);
    if !positions.is_empty() {
        args.remove(position);
    }
    expression(
        ExprKind::Pipe {
            value: Box::new(left.node),
            name,
            args,
            position,
        },
        span,
        depth,
    )
}
