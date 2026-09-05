//! Checked expressions retain semantic types and resolved identities for lowering.
use crate::{
    ast::{BinaryOp, UnaryOp},
    Span, Type,
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
}
