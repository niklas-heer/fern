//! Checked expressions retain semantic types and resolved identities for lowering.
use crate::{
    ast::{BinaryOp, UnaryOp},
    Constructor, Span, Type,
};
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct LocalId(pub usize);
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct FunctionId(pub usize);
#[derive(Clone, Debug, Default)]
pub struct Program {
    pub functions: Vec<Function>,
    pub types: Vec<TypeLayout>,
}

#[derive(Clone, Debug)]
pub struct TypeLayout {
    pub ty: Type,
    pub variants: Vec<Vec<Type>>,
    pub fields: Vec<String>,
}
#[derive(Clone, Debug)]
pub struct Function {
    pub id: FunctionId,
    pub name: String,
    pub params: Vec<Param>,
    pub captures: Vec<Param>,
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
pub struct Capture {
    pub param: Param,
    pub value: Expr,
}
#[derive(Clone, Debug)]
pub struct Expr {
    pub kind: ExprKind,
    pub ty: Type,
    pub span: Span,
}
#[derive(Clone, Debug)]
pub enum ExprKind {
    Return(Box<Expr>),
    Defer(Box<Expr>),
    /// Temporary checked lambda; eliminated by specialization/lifting before lowering.
    Lambda {
        params: Vec<Param>,
        captures: Vec<Capture>,
        body: Box<Expr>,
        local_count: usize,
    },
    /// Temporary callable reference; its complete function type determines specialization.
    FunctionValue {
        target: CallTarget,
    },
    Closure {
        function: FunctionId,
        captures: Vec<Expr>,
    },
    Invoke {
        callee: Box<Expr>,
        args: Vec<Expr>,
    },
    Int(i64),
    Float(f64),
    Bool(bool),
    String(String),
    Interpolate(Vec<Expr>),
    Local(LocalId),
    Unit,
    List(Vec<Expr>),
    Map(Vec<(Expr, Expr)>),
    Tuple(Vec<Expr>),
    Try(Box<Expr>),
    CustomConstruct {
        tag: usize,
        fields: Vec<Expr>,
    },
    Field {
        value: Box<Expr>,
        index: usize,
    },
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
    pub guard: Option<Expr>,
    pub body: Expr,
    pub span: Span,
}

#[derive(Clone, Debug)]
pub enum Pattern {
    Tuple(Vec<Pattern>),
    Wildcard,
    Bind(LocalId),
    Int(i64),
    Bool(bool),
    String(String),
    Variant {
        tag: usize,
        fields: Vec<Pattern>,
    },
    Constructor {
        constructor: Constructor,
        binding: Option<LocalId>,
    },
}
#[derive(Clone, Debug)]
pub enum Stmt {
    LetElse {
        pattern: Pattern,
        value: Expr,
        else_branch: Expr,
    },
    Let {
        id: LocalId,
        value: Expr,
    },
    Expr(Expr),
}
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum CallTarget {
    Function(FunctionId),
    Builtin(Builtin),
    Runtime(usize),
}
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Builtin {
    MapNew,
    MapGet,
    MapPut,
    MapDelete,
    MapLen,
    MapIsEmpty,
    MapContains,
    MapKeys,
    MapValues,
    ListMap,
    ListFold,
    ListFilter,
    ListFind,
    ListAny,
    ListAll,
    OptionMap,
    ResultMap,
    ResultAndThen,
    ResultUnwrapOrElse,
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
