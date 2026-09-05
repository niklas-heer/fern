//! Checked expressions retain semantic types and resolved identities for lowering.
use crate::{
    ast::{BinaryOp, UnaryOp},
    Constructor, Span, Type,
};
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct LocalId(pub usize);
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct FunctionId(pub usize);
#[derive(Clone, Debug)]
pub struct Program {
    pub functions: Vec<Function>,
}
#[derive(Clone, Debug)]
pub struct Function {
    pub id: FunctionId,
    pub name: String,
    pub params: Vec<Param>,
    pub return_type: Type,
    pub body: Expr,
    pub local_count: usize,
}
#[derive(Clone, Debug)]
pub struct Param {
    pub id: LocalId,
    pub ty: Type,
}
#[derive(Clone, Debug)]
pub struct Expr {
    pub kind: ExprKind,
    pub ty: Type,
    pub span: Span,
}
#[derive(Clone, Debug)]
pub enum ExprKind {
    Int(i64),
    Bool(bool),
    String(String),
    Local(LocalId),
    Unit,
    List(Vec<Expr>),
    Try(Box<Expr>),
    Construct {
        constructor: Constructor,
        value: Option<Box<Expr>>,
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
    Call {
        target: CallTarget,
        args: Vec<Expr>,
    },
    If {
        condition: Box<Expr>,
        then_branch: Box<Expr>,
        else_branch: Option<Box<Expr>>,
    },
    Block(Vec<Stmt>),
}

#[derive(Clone, Debug)]
pub struct MatchArm {
    pub pattern: Pattern,
    pub body: Expr,
    pub span: Span,
}

#[derive(Clone, Debug)]
pub enum Pattern {
    Wildcard,
    Bind(LocalId),
    Int(i64),
    Bool(bool),
    String(String),
    Constructor {
        constructor: Constructor,
        binding: Option<LocalId>,
    },
}
#[derive(Clone, Debug)]
pub enum Stmt {
    Let { id: LocalId, value: Expr },
    Expr(Expr),
}
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum CallTarget {
    Function(FunctionId),
    Builtin(Builtin),
}
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Builtin {
    Print,
    Println,
    StringConcat,
    StringEq,
    StringLen,
    ListLen,
    ListGet,
    ListHead,
    ListTail,
    ListIsEmpty,
    ListPush,
    ListReverse,
    ListConcat,
    ListContains,
    OptionIsSome,
    OptionIsNone,
    OptionUnwrapOr,
    ResultIsOk,
    ResultIsErr,
    ResultUnwrapOr,
}
