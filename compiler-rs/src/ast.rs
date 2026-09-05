//! Source syntax for the bounded Rust prototype.
use crate::{Constructor, Span, Type};

#[derive(Clone, Debug, Default)]
pub struct Program {
    pub functions: Vec<Function>,
    pub types: Vec<TypeDecl>,
    pub module: Option<String>,
    pub imports: Vec<Import>,
    pub exports: Vec<String>,
}

#[derive(Clone, Debug)]
pub struct Import {
    pub module: String,
    pub alias: Option<String>,
    pub items: Option<Vec<String>>,
    pub public: bool,
    pub span: Span,
}

#[derive(Clone, Debug)]
pub struct TypeDecl {
    pub name: String,
    pub parameters: Vec<String>,
    pub variants: Vec<Variant>,
    pub record: bool,
    pub span: Span,
}

#[derive(Clone, Debug)]
pub struct Variant {
    pub name: String,
    pub fields: Vec<Field>,
    pub span: Span,
}

#[derive(Clone, Debug)]
pub struct Field {
    pub name: Option<String>,
    pub ty: Type,
    pub span: Span,
}
#[derive(Clone, Debug)]
pub struct Function {
    pub name: String,
    pub params: Vec<Param>,
    pub return_type: Option<Type>,
    pub body: Expr,
    pub span: Span,
}
#[derive(Clone, Debug)]
pub struct Param {
    pub name: String,
    pub ty: Type,
    pub span: Span,
}
/// Anonymous parameter with optional context-inferred type annotation.
#[derive(Clone, Debug)]
pub struct LambdaParam {
    pub name: String,
    pub annotation: Option<Type>,
    pub span: Span,
}
#[derive(Clone, Debug)]
pub struct Expr {
    pub kind: ExprKind,
    pub span: Span,
}
#[derive(Clone, Debug)]
pub enum ExprKind {
    Int(i64),
    Float(f64),
    Bool(bool),
    String(String),
    Interpolate(Vec<StringPart>),
    Name(String),
    Unit,
    List(Vec<Expr>),
    Tuple(Vec<Expr>),
    Try(Box<Expr>),
    Pipe {
        value: Box<Expr>,
        name: String,
        args: Vec<Expr>,
        position: usize,
    },
    Field {
        value: Box<Expr>,
        name: String,
    },
    Match {
        value: Box<Expr>,
        arms: Vec<MatchArm>,
    },
    Unary {
        op: UnaryOp,
        value: Box<Expr>,
    },
    Binary {
        op: BinaryOp,
        left: Box<Expr>,
        right: Box<Expr>,
    },
    Lambda {
        params: Vec<LambdaParam>,
        body: Box<Expr>,
    },
    Apply {
        callee: Box<Expr>,
        args: Vec<Expr>,
    },
    Call {
        name: String,
        args: Vec<Expr>,
    },
    If {
        condition: Box<Expr>,
        then_branch: Box<Expr>,
        else_branch: Option<Box<Expr>>,
    },
    Block(Vec<Stmt>),
}

/// Preserve literal segments separately from embedded expressions for formatting.
#[derive(Clone, Debug)]
pub enum StringPart {
    Text(String),
    Value(Expr),
}

#[derive(Clone, Debug)]
pub struct MatchArm {
    pub pattern: Pattern,
    pub guard: Option<Expr>,
    pub body: Expr,
    pub span: Span,
}

#[derive(Clone, Debug)]
pub struct Pattern {
    pub kind: PatternKind,
    pub span: Span,
}

#[derive(Clone, Debug)]
pub enum PatternKind {
    Tuple(Vec<Pattern>),
    Wildcard,
    Bind(String),
    Int(i64),
    Bool(bool),
    String(String),
    NamedConstructor {
        name: String,
        fields: Vec<Pattern>,
    },
    Constructor {
        constructor: Constructor,
        binding: Option<String>,
    },
}
#[derive(Clone, Debug)]
pub enum Stmt {
    LetPattern {
        pattern: Pattern,
        annotation: Option<Type>,
        value: Expr,
        span: Span,
    },
    Let {
        name: String,
        annotation: Option<Type>,
        value: Expr,
        span: Span,
    },
    Expr(Expr),
}
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum UnaryOp {
    Negate,
    Not,
}
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum BinaryOp {
    Add,
    Subtract,
    Multiply,
    Divide,
    Remainder,
    Eq,
    Ne,
    Lt,
    Le,
    Gt,
    Ge,
    And,
    Or,
}
