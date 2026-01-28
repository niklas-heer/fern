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

/* Forward-declared vector types (needed before their typical definition site) */
VEC_TYPE(TypeExprVec, TypeExpr*)
VEC_TYPE(StringVec, String*)

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
    EXPR_DOT,           // object.field
    EXPR_RANGE,         // 0..10 or 0..=10
    EXPR_FOR,           // for item in iterable: body
    EXPR_WHILE,         // while condition: body
    EXPR_LOOP,          // loop: body
    EXPR_LAMBDA,        // (x, y) -> expr
    EXPR_INTERP_STRING, // "Hello, {name}!"
    EXPR_MAP,           // %{ key: value, ... }
    EXPR_TUPLE,         // (a, b, c)
    EXPR_RECORD_UPDATE, // { user | age: 31 }
    EXPR_LIST_COMP,     // [expr for var in iterable if condition]
    EXPR_SPAWN,         // spawn(expr)
    EXPR_SEND,          // send(pid, msg)
    EXPR_RECEIVE,       // receive: pattern -> body ...
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
    Expr* guard;        // NULL if no guard; otherwise the `if` condition
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

/* Dot access expression: object.field */
typedef struct {
    Expr* object;
    String* field;
} DotExpr;

/* Range expression: start..end or start..=end */
typedef struct {
    Expr* start;
    Expr* end;
    bool inclusive;      // false for .., true for ..=
} RangeExpr;

/* For loop expression: for var_name in iterable: body */
typedef struct {
    String* var_name;
    Expr* iterable;
    Expr* body;
} ForExpr;

/* While loop expression: while condition: body */
typedef struct {
    Expr* condition;
    Expr* body;
} WhileExpr;

/* Loop expression (infinite): loop: body */
typedef struct {
    Expr* body;
} LoopExpr;

/* Lambda expression: (params) -> body */
typedef struct {
    StringVec* params;
    Expr* body;
} LambdaExpr;

/* Interpolated string: "Hello, {name}! You are {age} years old."
 * Parts alternate between string literals and expressions.
 * E.g. ["Hello, ", name, "! You are ", age, " years old."]
 */
typedef struct {
    ExprVec* parts;  // Mix of EXPR_STRING_LIT and other expressions
} InterpStringExpr;

/* Tuple literal: (a, b, c) */
typedef struct {
    ExprVec* elements;
} TupleExpr;

/* Record field update: name: value */
typedef struct {
    String* name;
    Expr* value;
} RecordField;

VEC_TYPE(RecordFieldVec, RecordField)

/* Record update expression: { base | field: value, ... } */
typedef struct {
    Expr* base;
    RecordFieldVec* fields;
} RecordUpdateExpr;

/* List comprehension: [expr for var in iterable if condition] */
typedef struct {
    Expr* body;         // The expression to evaluate per element
    String* var_name;   // Loop variable name
    Expr* iterable;     // Collection to iterate over
    Expr* condition;    // Optional filter condition (NULL if no 'if')
} ListCompExpr;

/* Spawn expression: spawn(expr) */
typedef struct {
    Expr* func;
} SpawnExpr;

/* Send expression: send(pid, msg) */
typedef struct {
    Expr* pid;
    Expr* message;
} SendExpr;

/* Receive expression: receive: pattern -> body ... */
typedef struct {
    MatchArmVec* arms;
    Expr* after_timeout;    // NULL if no after clause
    Expr* after_body;       // NULL if no after clause
} ReceiveExpr;

/* Map entry: key: value */
typedef struct {
    Expr* key;
    Expr* value;
} MapEntry;

VEC_TYPE(MapEntryVec, MapEntry)

/* Map literal: %{ key: value, ... } */
typedef struct {
    MapEntryVec* entries;
} MapExpr;

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
        DotExpr dot;
        RangeExpr range;
        ForExpr for_loop;
        WhileExpr while_loop;
        LoopExpr loop;
        LambdaExpr lambda;
        InterpStringExpr interp_string;
        MapExpr map;
        TupleExpr tuple;
        RecordUpdateExpr record_update;
        ListCompExpr list_comp;
        SpawnExpr spawn_expr;
        SendExpr send_expr;
        ReceiveExpr receive_expr;
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
    bool is_public;                  // true if preceded by 'pub' keyword
    ParameterVec* params;            // Typed params (single-clause)
    TypeExpr* return_type;           // NULL if no return type annotation
    TypeExprVec* where_clauses;      // NULL if no where clause; e.g., where Ord(a), Show(a)
    Expr* body;                      // Body (single-clause)
    FunctionClauseVec* clauses;      // NULL for single-clause functions
} FunctionDef;

/* Type variant field (name: Type) — used in sum type variants and record types */
typedef struct {
    String* name;
    TypeExpr* type_ann;
} TypeField;

VEC_TYPE(TypeFieldVec, TypeField)

/* Type variant — a constructor in a sum type (e.g., Some(a), None, Circle(radius: Float)) */
typedef struct {
    String* name;
    TypeFieldVec* fields; // NULL if no fields (e.g., None, Active)
} TypeVariant;

VEC_TYPE(TypeVariantVec, TypeVariant)

/* Type definition: type Name(params): variants/fields */
typedef struct {
    String* name;
    bool is_public;
    StringVec* type_params;      // NULL if no type params (e.g., Option(a) has ["a"])
    StringVec* derives;          // NULL if no derive clause (e.g., derive(Show, Eq))
    TypeVariantVec* variants;    // Sum type variants (NULL for pure record types)
    TypeFieldVec* record_fields; // Record fields (NULL for sum types)
} TypeDef;

/* Statement types */
typedef enum {
    STMT_LET,           // let x = expr
    STMT_RETURN,        // return expr
    STMT_EXPR,          // expression statement
    STMT_FN,            // fn name(params) -> type: body
    STMT_IMPORT,        // import path.to.module
    STMT_DEFER,         // defer expr
    STMT_TYPE_DEF,      // type Name: variants/fields
    STMT_BREAK,         // break [expr]
    STMT_CONTINUE,      // continue
    STMT_TRAIT,         // trait Name(params): methods
    STMT_IMPL,          // impl Trait(Type): methods
    STMT_NEWTYPE,       // newtype Name = Constructor(InnerType)
} StmtType;

/* Let statement */
typedef struct {
    Pattern* pattern;
    TypeExpr* type_ann;  // NULL if no type annotation
    Expr* value;
    Expr* else_expr;     // NULL unless let-else: let Some(x) = val else: fallback
} LetStmt;

/* Return statement */
typedef struct {
    Expr* value;        // NULL for bare return
    Expr* condition;    // NULL unless postfix guard: return x if cond
} ReturnStmt;

/* Expression statement */
typedef struct {
    Expr* expr;
} ExprStmt;

/* Import declaration: import path.to.module [.{items}] [as alias] */
typedef struct {
    StringVec* path;        // Module path segments (e.g., ["math", "geometry"])
    StringVec* items;       // NULL if no specific items; otherwise ["cors", "auth"]
    String* alias;          // NULL if no alias; otherwise "geo"
} ImportDecl;

/* Defer statement: defer <expression> */
typedef struct {
    Expr* expr;
} DeferStmt;

/* Trait definition: trait Name(params): methods */
typedef struct {
    String* name;
    StringVec* type_params;     // e.g., (a) in trait Show(a)
    StmtVec* methods;           // List of STMT_FN
} TraitDef;

/* Impl block: impl Trait(Type): methods */
typedef struct {
    String* trait_name;
    TypeExprVec* type_args;     // e.g., (Point) in impl Show(Point)
    StmtVec* methods;           // List of STMT_FN
} ImplDef;

/* Newtype definition: newtype Name = Constructor(InnerType) */
typedef struct {
    String* name;
    bool is_public;
    String* constructor;     // Constructor name (e.g., UserId in newtype UserId = UserId(Int))
    TypeExpr* inner_type;    // The wrapped type
} NewtypeDef;

/* Break statement: break [value] */
typedef struct {
    Expr* value;        // NULL for bare break
} BreakStmt;

/* Statement node */
struct Stmt {
    StmtType type;
    SourceLoc loc;
    union {
        LetStmt let;
        ReturnStmt return_stmt;
        ExprStmt expr;
        FunctionDef fn;
        ImportDecl import;
        DeferStmt defer_stmt;
        TypeDef type_def;
        BreakStmt break_stmt;
        TraitDef trait_def;
        ImplDef impl_def;
        NewtypeDef newtype_def;
    } data;
};

/* Pattern types */
typedef enum {
    PATTERN_IDENT,      // x
    PATTERN_WILDCARD,   // _
    PATTERN_LIT,        // 42, "hello", true
    PATTERN_CONSTRUCTOR,// Some(x), Ok(value), Err(msg)
    PATTERN_TUPLE,      // (x, y, z)
    PATTERN_REST,       // ..rest or .._
} PatternType;

/* Constructor pattern: Name(sub_patterns) */
typedef struct {
    String* name;
    PatternVec* args;   // Sub-patterns (may be empty for nullary constructors)
} ConstructorPattern;

struct Pattern {
    PatternType type;
    SourceLoc loc;
    union {
        String* ident;
        Expr* literal;
        ConstructorPattern constructor;
        PatternVec* tuple;  // Sub-patterns for tuple destructuring
        String* rest_name;  // ..rest name (NULL for .._)
    } data;
};

/* Type expression kinds */
typedef enum {
    TYPE_NAMED,         // Int, String, Result(String, Error), List(Int)
    TYPE_FUNCTION,      // (Int, String) -> Bool
} TypeExprKind;

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
Expr* expr_float_lit(Arena* arena, double value, SourceLoc loc);
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
Expr* expr_dot(Arena* arena, Expr* object, String* field, SourceLoc loc);
Expr* expr_range(Arena* arena, Expr* start, Expr* end, bool inclusive, SourceLoc loc);
Expr* expr_for(Arena* arena, String* var_name, Expr* iterable, Expr* body, SourceLoc loc);
Expr* expr_while(Arena* arena, Expr* condition, Expr* body, SourceLoc loc);
Expr* expr_loop(Arena* arena, Expr* body, SourceLoc loc);
Expr* expr_lambda(Arena* arena, StringVec* params, Expr* body, SourceLoc loc);
Expr* expr_interp_string(Arena* arena, ExprVec* parts, SourceLoc loc);
Expr* expr_map(Arena* arena, MapEntryVec* entries, SourceLoc loc);
Expr* expr_tuple(Arena* arena, ExprVec* elements, SourceLoc loc);
Expr* expr_record_update(Arena* arena, Expr* base, RecordFieldVec* fields, SourceLoc loc);
Expr* expr_list_comp(Arena* arena, Expr* body, String* var_name, Expr* iterable, Expr* condition, SourceLoc loc);
Expr* expr_spawn(Arena* arena, Expr* func, SourceLoc loc);
Expr* expr_send(Arena* arena, Expr* pid, Expr* message, SourceLoc loc);
Expr* expr_receive(Arena* arena, MatchArmVec* arms, Expr* after_timeout, Expr* after_body, SourceLoc loc);

/* Create statements */
Stmt* stmt_let(Arena* arena, Pattern* pattern, TypeExpr* type_ann, Expr* value, SourceLoc loc);
Stmt* stmt_return(Arena* arena, Expr* value, SourceLoc loc);
Stmt* stmt_expr(Arena* arena, Expr* expr, SourceLoc loc);
Stmt* stmt_fn(Arena* arena, String* name, bool is_public, ParameterVec* params, TypeExpr* return_type, Expr* body, SourceLoc loc);
Stmt* stmt_import(Arena* arena, StringVec* path, StringVec* items, String* alias, SourceLoc loc);
Stmt* stmt_defer(Arena* arena, Expr* expr, SourceLoc loc);
Stmt* stmt_type_def(Arena* arena, String* name, bool is_public, StringVec* type_params,
                    StringVec* derives, TypeVariantVec* variants, TypeFieldVec* record_fields, SourceLoc loc);
Stmt* stmt_newtype(Arena* arena, String* name, bool is_public, String* constructor, TypeExpr* inner_type, SourceLoc loc);
Stmt* stmt_break(Arena* arena, Expr* value, SourceLoc loc);
Stmt* stmt_continue(Arena* arena, SourceLoc loc);
Stmt* stmt_trait(Arena* arena, String* name, StringVec* type_params, StmtVec* methods, SourceLoc loc);
Stmt* stmt_impl(Arena* arena, String* trait_name, TypeExprVec* type_args, StmtVec* methods, SourceLoc loc);

/* Create patterns */
Pattern* pattern_ident(Arena* arena, String* name, SourceLoc loc);
Pattern* pattern_wildcard(Arena* arena, SourceLoc loc);
Pattern* pattern_constructor(Arena* arena, String* name, PatternVec* args, SourceLoc loc);
Pattern* pattern_tuple(Arena* arena, PatternVec* elements, SourceLoc loc);
Pattern* pattern_rest(Arena* arena, String* name, SourceLoc loc);

/* Create type expressions */
TypeExpr* type_named(Arena* arena, String* name, TypeExprVec* args, SourceLoc loc);
TypeExpr* type_function(Arena* arena, TypeExprVec* params, TypeExpr* return_type, SourceLoc loc);

#endif /* FERN_AST_H */
