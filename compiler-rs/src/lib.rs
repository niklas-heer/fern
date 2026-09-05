//! Experimental independent Fern frontend. The C compiler remains the reference.
#![forbid(unsafe_code)]
pub mod ast;
pub mod check;
pub mod ir;
pub mod parse;
pub mod qbe;

/// Source byte range, with an exclusive end offset.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Span {
    pub start: usize,
    pub end: usize,
}

/// A user-facing error with source location, never a compiler panic.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Diagnostic {
    pub span: Span,
    pub message: String,
}

impl Diagnostic {
    /// Construct an error at a source range.
    pub fn new(span: Span, message: impl Into<String>) -> Self {
        Self {
            span,
            message: message.into(),
        }
    }
}

/// Semantic types; inference variables are eliminated before QBE lowering.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Type {
    Int,
    Bool,
    String,
    Unit,
    List(Box<Type>),
    Option(Box<Type>),
    Result(Box<Type>, Box<Type>),
    Infer(u32),
}

/// Built-in sum constructors, independent of runtime representation.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Constructor {
    Some,
    None,
    Ok,
    Err,
}
