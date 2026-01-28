/* Fern AST - Abstract Syntax Tree
 * 
 * Represents the parsed structure of Fern programs.
 * Uses arena allocation for memory safety.
 */

#ifndef FERN_AST_H
#define FERN_AST_H

#include "arena.h"
#include "fern_string.h"
#include "token.h"
#include "vec.h"
#include <stdbool.h>

/* Forward declarations */
typedef struct Expr Expr;
typedef struct Stmt Stmt;
typedef struct Pattern Pattern;
typedef struct TypeExpr TypeExpr;

/* Expression types */
typedef enum {
    EXPR_INT_LIT,       // 42
    EXPR_FLOAT_LIT,     // 3.14
    EXPR_STRING_LIT,    // "hello"
    EXPR_BOOL_LIT,      // true, false
    EXPR_IDENT,         // variable_name
    EXPR_BINARY,        // a + b
    EXPR_UNARY,         // -x, not y
    EXPR_CALL,          // func(args)
    EXPR_IF,            // if condition: body else: other
    EXPR_MATCH,         // match value: cases
    EXPR_BLOCK,         // Sequence of statements with final expression
    EXPR_LIST,          // [1, 2, 3]
    EXPR_BIND,          // x <- operation()
    EXPR_WITH,          // with x <- f(), y <- g(x) do Ok(y) else Err(e) -> e
} ExprType;

/* Binary operators */
typedef enum {
    BINOP_ADD,          // +
    BINOP_SUB,          // -
    BINOP_MUL,          // *
    BINOP_DIV,          // /
    BINOP_MOD,          // %
    BINOP_POW,          // **
    BINOP_EQ,           // ==
    BINOP_NE,           // !=
    BINOP_LT,           // <
    BINOP_LE,           // <=
    BINOP_GT,           // >
    BINOP_GE,           // >=
    BINOP_AND,          // and
    BINOP_OR,           // or
    BINOP_PIPE,         // |>
} BinaryOp;

/* Unary operators */
typedef enum {
    UNOP_NEG,           // -
    UNOP_NOT,           // not
} UnaryOp;

/* Integer literal */
typedef struct {
    int64_t value;
} IntLit;

/* Float literal */
typedef struct {
    double value;
} FloatLit;

/* String literal */
typedef struct {
    String* value;
} StringLit;

/* Boolean literal */
typedef struct {
    bool value;
} BoolLit;

/* Identifier */
typedef struct {
    String* name;
} Ident;

/* Binary expression */
typedef struct {
    BinaryOp op;
    Expr* left;
    Expr* right;
} BinaryExpr;

/* Unary expression */
typedef struct {
    UnaryOp op;
    Expr* operand;
} UnaryExpr;

/* Function call argument */
typedef struct {
    String* label;      // NULL if positional
    Expr* value;
} CallArg;

VEC_TYPE(CallArgVec, CallArg)

/* Function call */
typedef struct {
    Expr* func;         // Usually EXPR_IDENT, but could be any expression
    CallArgVec* args;
} CallExpr;

/* If expression */
typedef struct {
    Expr* condition;
    Expr* then_branch;
    Expr* else_branch;  // NULL if no else
} IfExpr;

/* Match arm */
typedef struct {
    Pattern* pattern;
    Expr* body;
} MatchArm;

VEC_TYPE(MatchArmVec, MatchArm)

/* Match expression */
typedef struct {
    Expr* value;
    MatchArmVec* arms;
} MatchExpr;

/* Block (sequence of statements + optional final expression) */
VEC_TYPE(StmtVec, Stmt*)

typedef struct {
    StmtVec* stmts;
    Expr* final_expr;   // NULL if block doesn't produce value
} BlockExpr;

/* List literal */
VEC_TYPE(ExprVec, Expr*)

typedef struct {
    ExprVec* elements;
} ListExpr;

/* Bind expression (Result binding with <-) */
typedef struct {
    String* name;
    Expr* value;
} BindExpr;

/* With binding (name <- expr) used inside with expressions */
typedef struct {
    String* name;
    Expr* value;
} WithBinding;

VEC_TYPE(WithBindingVec, WithBinding)

/* With expression: with bindings do body [else arms] */
typedef struct {
    WithBindingVec* bindings;
    Expr* body;
    MatchArmVec* else_arms;  // NULL if no else clause
} WithExpr;

/* Expression node */
struct Expr {
    ExprType type;
    SourceLoc loc;
    union {
        IntLit int_lit;
        FloatLit float_lit;
        StringLit string_lit;
        BoolLit bool_lit;
        Ident ident;
        BinaryExpr binary;
        UnaryExpr unary;
        CallExpr call;
        IfExpr if_expr;
        MatchExpr match_expr;
        BlockExpr block;
        ListExpr list;
        BindExpr bind;
        WithExpr with_expr;
    } data;
};

/* Function parameter: name with type annotation */
typedef struct {
    String* name;
    TypeExpr* type_ann;
} Parameter;

VEC_TYPE(ParameterVec, Parameter)

/* Pattern vector (for multi-clause function parameters) */
VEC_TYPE(PatternVec, Pattern*)

/* Function clause: a single clause in a multi-clause function definition.
 * Each clause has pattern parameters and a body expression.
 * Example: fn factorial(0) -> 1
 *          fn factorial(n) -> n * factorial(n - 1)
 */
typedef struct {
    PatternVec* params;       // Pattern parameters for this clause
    TypeExpr* return_type;    // NULL if no return type annotation
    Expr* body;               // Body expression
} FunctionClause;

VEC_TYPE(FunctionClauseVec, FunctionClause)

/* Function definition.
 * Supports both single-clause (with typed parameters) and multi-clause
 * (with pattern parameters) function definitions.
 *
 * Single-clause: fn add(x: Int, y: Int) -> Int: x + y
 *   - params is set, clauses is NULL
 *
 * Multi-clause: fn fact(0) -> 1 / fn fact(n) -> n * fact(n - 1)
 *   - clauses is set, params may be NULL
 */
typedef struct {
    String* name;
    ParameterVec* params;            // Typed params (single-clause)
    TypeExpr* return_type;           // NULL if no return type annotation
    Expr* body;                      // Body (single-clause)
    FunctionClauseVec* clauses;      // NULL for single-clause functions
} FunctionDef;

/* Statement types */
typedef enum {
    STMT_LET,           // let x = expr
    STMT_RETURN,        // return expr
    STMT_EXPR,          // expression statement
    STMT_FN,            // fn name(params) -> type: body
} StmtType;

/* Let statement */
typedef struct {
    Pattern* pattern;
    TypeExpr* type_ann;  // NULL if no type annotation
    Expr* value;
} LetStmt;

/* Return statement */
typedef struct {
    Expr* value;        // NULL for bare return
} ReturnStmt;

/* Expression statement */
typedef struct {
    Expr* expr;
} ExprStmt;

/* Statement node */
struct Stmt {
    StmtType type;
    SourceLoc loc;
    union {
        LetStmt let;
        ReturnStmt return_stmt;
        ExprStmt expr;
        FunctionDef fn;
    } data;
};

/* Pattern types (for now, just basics) */
typedef enum {
    PATTERN_IDENT,      // x
    PATTERN_WILDCARD,   // _
    PATTERN_LIT,        // 42, "hello", true
} PatternType;

struct Pattern {
    PatternType type;
    SourceLoc loc;
    union {
        String* ident;
        Expr* literal;
    } data;
};

/* Type expression kinds */
typedef enum {
    TYPE_NAMED,         // Int, String, Result(String, Error), List(Int)
    TYPE_FUNCTION,      // (Int, String) -> Bool
} TypeExprKind;

/* Vector of type expressions (for type arguments and function params) */
VEC_TYPE(TypeExprVec, TypeExpr*)

/* Named type (with optional type arguments) */
typedef struct {
    String* name;
    TypeExprVec* args;  // NULL if no type arguments
} NamedType;

/* Function type: (params) -> return_type */
typedef struct {
    TypeExprVec* params;
    TypeExpr* return_type;
} FunctionType;

/* Type expression node */
struct TypeExpr {
    TypeExprKind kind;
    SourceLoc loc;
    union {
        NamedType named;
        FunctionType func;
    } data;
};

/* Helper functions */

/* Create expressions */
Expr* expr_int_lit(Arena* arena, int64_t value, SourceLoc loc);
Expr* expr_string_lit(Arena* arena, String* value, SourceLoc loc);
Expr* expr_bool_lit(Arena* arena, bool value, SourceLoc loc);
Expr* expr_ident(Arena* arena, String* name, SourceLoc loc);
Expr* expr_binary(Arena* arena, BinaryOp op, Expr* left, Expr* right, SourceLoc loc);
Expr* expr_unary(Arena* arena, UnaryOp op, Expr* operand, SourceLoc loc);
Expr* expr_call(Arena* arena, Expr* func, Expr** args, size_t num_args, SourceLoc loc);
Expr* expr_if(Arena* arena, Expr* condition, Expr* then_branch, Expr* else_branch, SourceLoc loc);
Expr* expr_match(Arena* arena, Expr* value, MatchArmVec* arms, SourceLoc loc);
Expr* expr_block(Arena* arena, StmtVec* stmts, Expr* final_expr, SourceLoc loc);
Expr* expr_list(Arena* arena, ExprVec* elements, SourceLoc loc);
Expr* expr_bind(Arena* arena, String* name, Expr* value, SourceLoc loc);
Expr* expr_with(Arena* arena, WithBindingVec* bindings, Expr* body, MatchArmVec* else_arms, SourceLoc loc);

/* Create statements */
Stmt* stmt_let(Arena* arena, Pattern* pattern, TypeExpr* type_ann, Expr* value, SourceLoc loc);
Stmt* stmt_return(Arena* arena, Expr* value, SourceLoc loc);
Stmt* stmt_expr(Arena* arena, Expr* expr, SourceLoc loc);
Stmt* stmt_fn(Arena* arena, String* name, ParameterVec* params, TypeExpr* return_type, Expr* body, SourceLoc loc);

/* Create patterns */
Pattern* pattern_ident(Arena* arena, String* name, SourceLoc loc);
Pattern* pattern_wildcard(Arena* arena, SourceLoc loc);

/* Create type expressions */
TypeExpr* type_named(Arena* arena, String* name, TypeExprVec* args, SourceLoc loc);
TypeExpr* type_function(Arena* arena, TypeExprVec* params, TypeExpr* return_type, SourceLoc loc);

#endif /* FERN_AST_H */
