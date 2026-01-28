/* AST Helper Functions */

#include "ast.h"
#include <assert.h>
#include <string.h>

/* Create integer literal expression */
Expr* expr_int_lit(Arena* arena, int64_t value, SourceLoc loc) {
    assert(arena != NULL);
    
    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_INT_LIT;
    expr->loc = loc;
    expr->data.int_lit.value = value;
    
    return expr;
}

/* Create float literal expression */
Expr* expr_float_lit(Arena* arena, double value, SourceLoc loc) {
    assert(arena != NULL);

    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_FLOAT_LIT;
    expr->loc = loc;
    expr->data.float_lit.value = value;

    return expr;
}

/* Create string literal expression */
Expr* expr_string_lit(Arena* arena, String* value, SourceLoc loc) {
    assert(arena != NULL);
    assert(value != NULL);
    
    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_STRING_LIT;
    expr->loc = loc;
    expr->data.string_lit.value = value;
    
    return expr;
}

/* Create boolean literal expression */
Expr* expr_bool_lit(Arena* arena, bool value, SourceLoc loc) {
    assert(arena != NULL);
    
    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_BOOL_LIT;
    expr->loc = loc;
    expr->data.bool_lit.value = value;
    
    return expr;
}

/* Create identifier expression */
Expr* expr_ident(Arena* arena, String* name, SourceLoc loc) {
    assert(arena != NULL);
    assert(name != NULL);
    
    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_IDENT;
    expr->loc = loc;
    expr->data.ident.name = name;
    
    return expr;
}

/* Create binary expression */
Expr* expr_binary(Arena* arena, BinaryOp op, Expr* left, Expr* right, SourceLoc loc) {
    assert(arena != NULL);
    assert(left != NULL);
    assert(right != NULL);
    
    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_BINARY;
    expr->loc = loc;
    expr->data.binary.op = op;
    expr->data.binary.left = left;
    expr->data.binary.right = right;
    
    return expr;
}

/* Create unary expression */
Expr* expr_unary(Arena* arena, UnaryOp op, Expr* operand, SourceLoc loc) {
    assert(arena != NULL);
    assert(operand != NULL);
    
    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_UNARY;
    expr->loc = loc;
    expr->data.unary.op = op;
    expr->data.unary.operand = operand;
    
    return expr;
}

/* Create call expression */
Expr* expr_call(Arena* arena, Expr* func, Expr** args, size_t num_args, SourceLoc loc) {
    assert(arena != NULL);
    assert(func != NULL);
    
    // Create CallArgVec and populate with positional arguments
    CallArgVec* arg_vec = CallArgVec_new(arena);
    for (size_t i = 0; i < num_args; i++) {
        CallArg arg;
        arg.label = NULL;  // Positional argument
        arg.value = args[i];
        CallArgVec_push(arena, arg_vec, arg);
    }
    
    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_CALL;
    expr->loc = loc;
    expr->data.call.func = func;
    expr->data.call.args = arg_vec;
    
    return expr;
}

/* Create if expression */
Expr* expr_if(Arena* arena, Expr* condition, Expr* then_branch, Expr* else_branch, SourceLoc loc) {
    assert(arena != NULL);
    assert(condition != NULL);
    assert(then_branch != NULL);
    // else_branch can be NULL
    
    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_IF;
    expr->loc = loc;
    expr->data.if_expr.condition = condition;
    expr->data.if_expr.then_branch = then_branch;
    expr->data.if_expr.else_branch = else_branch;
    
    return expr;
}

/* Create match expression */
Expr* expr_match(Arena* arena, Expr* value, MatchArmVec* arms, SourceLoc loc) {
    assert(arena != NULL);
    assert(arms != NULL);
    
    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_MATCH;
    expr->loc = loc;
    expr->data.match_expr.value = value;
    expr->data.match_expr.arms = arms;
    
    return expr;
}

/* Create block expression */
Expr* expr_block(Arena* arena, StmtVec* stmts, Expr* final_expr, SourceLoc loc) {
    assert(arena != NULL);
    assert(stmts != NULL);
    
    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_BLOCK;
    expr->loc = loc;
    expr->data.block.stmts = stmts;
    expr->data.block.final_expr = final_expr;  // Can be NULL
    
    return expr;
}

/* Create list expression */
Expr* expr_list(Arena* arena, ExprVec* elements, SourceLoc loc) {
    assert(arena != NULL);
    assert(elements != NULL);
    
    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_LIST;
    expr->loc = loc;
    expr->data.list.elements = elements;
    
    return expr;
}

/* Create bind expression (name <- value) */
Expr* expr_bind(Arena* arena, String* name, Expr* value, SourceLoc loc) {
    assert(arena != NULL);
    assert(name != NULL);
    assert(value != NULL);

    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_BIND;
    expr->loc = loc;
    expr->data.bind.name = name;
    expr->data.bind.value = value;

    return expr;
}

/* Create with expression */
Expr* expr_with(Arena* arena, WithBindingVec* bindings, Expr* body, MatchArmVec* else_arms, SourceLoc loc) {
    assert(arena != NULL);
    assert(bindings != NULL);
    assert(body != NULL);

    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_WITH;
    expr->loc = loc;
    expr->data.with_expr.bindings = bindings;
    expr->data.with_expr.body = body;
    expr->data.with_expr.else_arms = else_arms;  // Can be NULL

    return expr;
}

/* Create dot access expression */
Expr* expr_dot(Arena* arena, Expr* object, String* field, SourceLoc loc) {
    assert(arena != NULL);
    assert(object != NULL);
    assert(field != NULL);

    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_DOT;
    expr->loc = loc;
    expr->data.dot.object = object;
    expr->data.dot.field = field;

    return expr;
}

/* Create range expression */
Expr* expr_range(Arena* arena, Expr* start, Expr* end, bool inclusive, SourceLoc loc) {
    assert(arena != NULL);
    assert(start != NULL);
    assert(end != NULL);

    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_RANGE;
    expr->loc = loc;
    expr->data.range.start = start;
    expr->data.range.end = end;
    expr->data.range.inclusive = inclusive;

    return expr;
}

/* Create for loop expression */
Expr* expr_for(Arena* arena, String* var_name, Expr* iterable, Expr* body, SourceLoc loc) {
    assert(arena != NULL);
    assert(var_name != NULL);
    assert(iterable != NULL);
    assert(body != NULL);

    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_FOR;
    expr->loc = loc;
    expr->data.for_loop.var_name = var_name;
    expr->data.for_loop.iterable = iterable;
    expr->data.for_loop.body = body;

    return expr;
}

/* Create while loop expression */
Expr* expr_while(Arena* arena, Expr* condition, Expr* body, SourceLoc loc) {
    assert(arena != NULL);
    assert(condition != NULL);
    assert(body != NULL);

    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_WHILE;
    expr->loc = loc;
    expr->data.while_loop.condition = condition;
    expr->data.while_loop.body = body;

    return expr;
}

/* Create infinite loop expression */
Expr* expr_loop(Arena* arena, Expr* body, SourceLoc loc) {
    assert(arena != NULL);
    assert(body != NULL);

    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_LOOP;
    expr->loc = loc;
    expr->data.loop.body = body;

    return expr;
}

Expr* expr_lambda(Arena* arena, StringVec* params, Expr* body, SourceLoc loc) {
    assert(arena != NULL);
    assert(params != NULL);
    assert(body != NULL);

    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_LAMBDA;
    expr->loc = loc;
    expr->data.lambda.params = params;
    expr->data.lambda.body = body;

    return expr;
}

Expr* expr_tuple(Arena* arena, ExprVec* elements, SourceLoc loc) {
    assert(arena != NULL);
    assert(elements != NULL);

    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_TUPLE;
    expr->loc = loc;
    expr->data.tuple.elements = elements;

    return expr;
}

Expr* expr_record_update(Arena* arena, Expr* base, RecordFieldVec* fields, SourceLoc loc) {
    assert(arena != NULL);
    assert(base != NULL);
    assert(fields != NULL);

    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_RECORD_UPDATE;
    expr->loc = loc;
    expr->data.record_update.base = base;
    expr->data.record_update.fields = fields;

    return expr;
}

Expr* expr_list_comp(Arena* arena, Expr* body, String* var_name, Expr* iterable, Expr* condition, SourceLoc loc) {
    assert(arena != NULL);
    assert(body != NULL);
    assert(var_name != NULL);
    assert(iterable != NULL);

    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_LIST_COMP;
    expr->loc = loc;
    expr->data.list_comp.body = body;
    expr->data.list_comp.var_name = var_name;
    expr->data.list_comp.iterable = iterable;
    expr->data.list_comp.condition = condition;

    return expr;
}

Expr* expr_index(Arena* arena, Expr* object, Expr* index, SourceLoc loc) {
    assert(arena != NULL);
    assert(object != NULL);
    assert(index != NULL);

    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_INDEX;
    expr->loc = loc;
    expr->data.index_expr.object = object;
    expr->data.index_expr.index = index;

    return expr;
}

Expr* expr_spawn(Arena* arena, Expr* func, SourceLoc loc) {
    assert(arena != NULL);
    assert(func != NULL);

    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_SPAWN;
    expr->loc = loc;
    expr->data.spawn_expr.func = func;

    return expr;
}

Expr* expr_send(Arena* arena, Expr* pid, Expr* message, SourceLoc loc) {
    assert(arena != NULL);
    assert(pid != NULL);
    assert(message != NULL);

    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_SEND;
    expr->loc = loc;
    expr->data.send_expr.pid = pid;
    expr->data.send_expr.message = message;

    return expr;
}

Expr* expr_receive(Arena* arena, MatchArmVec* arms, Expr* after_timeout, Expr* after_body, SourceLoc loc) {
    assert(arena != NULL);
    assert(arms != NULL);

    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_RECEIVE;
    expr->loc = loc;
    expr->data.receive_expr.arms = arms;
    expr->data.receive_expr.after_timeout = after_timeout;
    expr->data.receive_expr.after_body = after_body;

    return expr;
}

Expr* expr_map(Arena* arena, MapEntryVec* entries, SourceLoc loc) {
    assert(arena != NULL);
    assert(entries != NULL);

    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_MAP;
    expr->loc = loc;
    expr->data.map.entries = entries;

    return expr;
}

Expr* expr_interp_string(Arena* arena, ExprVec* parts, SourceLoc loc) {
    assert(arena != NULL);
    assert(parts != NULL);

    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_INTERP_STRING;
    expr->loc = loc;
    expr->data.interp_string.parts = parts;

    return expr;
}

/* Create named type expression */
TypeExpr* type_named(Arena* arena, String* name, TypeExprVec* args, SourceLoc loc) {
    assert(arena != NULL);
    assert(name != NULL);

    TypeExpr* type = arena_alloc(arena, sizeof(TypeExpr));
    type->kind = TYPE_NAMED;
    type->loc = loc;
    type->data.named.name = name;
    type->data.named.args = args;  // Can be NULL for simple types

    return type;
}

/* Create function type expression */
TypeExpr* type_function(Arena* arena, TypeExprVec* params, TypeExpr* return_type, SourceLoc loc) {
    assert(arena != NULL);
    assert(params != NULL);
    assert(return_type != NULL);

    TypeExpr* type = arena_alloc(arena, sizeof(TypeExpr));
    type->kind = TYPE_FUNCTION;
    type->loc = loc;
    type->data.func.params = params;
    type->data.func.return_type = return_type;

    return type;
}

/* Create function definition statement */
Stmt* stmt_fn(Arena* arena, String* name, bool is_public, ParameterVec* params, TypeExpr* return_type, Expr* body, SourceLoc loc) {
    assert(arena != NULL);
    assert(name != NULL);
    assert(params != NULL);
    assert(body != NULL);

    Stmt* stmt = arena_alloc(arena, sizeof(Stmt));
    stmt->type = STMT_FN;
    stmt->loc = loc;
    stmt->data.fn.name = name;
    stmt->data.fn.is_public = is_public;
    stmt->data.fn.params = params;
    stmt->data.fn.return_type = return_type;
    stmt->data.fn.where_clauses = NULL;
    stmt->data.fn.body = body;
    stmt->data.fn.clauses = NULL;

    return stmt;
}

/* Create import declaration statement */
Stmt* stmt_import(Arena* arena, StringVec* path, StringVec* items, String* alias, SourceLoc loc) {
    assert(arena != NULL);
    assert(path != NULL);

    Stmt* stmt = arena_alloc(arena, sizeof(Stmt));
    stmt->type = STMT_IMPORT;
    stmt->loc = loc;
    stmt->data.import.path = path;
    stmt->data.import.items = items;    // Can be NULL
    stmt->data.import.alias = alias;    // Can be NULL

    return stmt;
}

/* Create defer statement */
Stmt* stmt_defer(Arena* arena, Expr* expr, SourceLoc loc) {
    assert(arena != NULL);
    assert(expr != NULL);

    Stmt* stmt = arena_alloc(arena, sizeof(Stmt));
    stmt->type = STMT_DEFER;
    stmt->loc = loc;
    stmt->data.defer_stmt.expr = expr;

    return stmt;
}

/* Create type definition statement */
Stmt* stmt_type_def(Arena* arena, String* name, bool is_public, StringVec* type_params,
                    StringVec* derives, TypeVariantVec* variants, TypeFieldVec* record_fields, SourceLoc loc) {
    assert(arena != NULL);
    assert(name != NULL);

    Stmt* stmt = arena_alloc(arena, sizeof(Stmt));
    stmt->type = STMT_TYPE_DEF;
    stmt->loc = loc;
    stmt->data.type_def.name = name;
    stmt->data.type_def.is_public = is_public;
    stmt->data.type_def.type_params = type_params;
    stmt->data.type_def.derives = derives;
    stmt->data.type_def.variants = variants;
    stmt->data.type_def.record_fields = record_fields;

    return stmt;
}

/* Create break statement */
Stmt* stmt_newtype(Arena* arena, String* name, bool is_public, String* constructor, TypeExpr* inner_type, SourceLoc loc) {
    assert(arena != NULL);
    assert(name != NULL);
    assert(constructor != NULL);
    assert(inner_type != NULL);

    Stmt* stmt = arena_alloc(arena, sizeof(Stmt));
    stmt->type = STMT_NEWTYPE;
    stmt->loc = loc;
    stmt->data.newtype_def.name = name;
    stmt->data.newtype_def.is_public = is_public;
    stmt->data.newtype_def.constructor = constructor;
    stmt->data.newtype_def.inner_type = inner_type;

    return stmt;
}

Stmt* stmt_module(Arena* arena, StringVec* path, SourceLoc loc) {
    assert(arena != NULL);
    assert(path != NULL);

    Stmt* stmt = arena_alloc(arena, sizeof(Stmt));
    stmt->type = STMT_MODULE;
    stmt->loc = loc;
    stmt->data.module_decl.path = path;

    return stmt;
}

Stmt* stmt_break(Arena* arena, Expr* value, SourceLoc loc) {
    assert(arena != NULL);

    Stmt* stmt = arena_alloc(arena, sizeof(Stmt));
    stmt->type = STMT_BREAK;
    stmt->loc = loc;
    stmt->data.break_stmt.value = value; // Can be NULL

    return stmt;
}

/* Create continue statement */
Stmt* stmt_continue(Arena* arena, SourceLoc loc) {
    assert(arena != NULL);

    Stmt* stmt = arena_alloc(arena, sizeof(Stmt));
    stmt->type = STMT_CONTINUE;
    stmt->loc = loc;

    return stmt;
}

/* Create trait definition */
Stmt* stmt_trait(Arena* arena, String* name, StringVec* type_params, TypeExprVec* constraints, StmtVec* methods, SourceLoc loc) {
    assert(arena != NULL);
    assert(name != NULL);
    assert(methods != NULL);

    Stmt* stmt = arena_alloc(arena, sizeof(Stmt));
    stmt->type = STMT_TRAIT;
    stmt->loc = loc;
    stmt->data.trait_def.name = name;
    stmt->data.trait_def.type_params = type_params;
    stmt->data.trait_def.constraints = constraints;
    stmt->data.trait_def.methods = methods;

    return stmt;
}

/* Create impl block */
Stmt* stmt_impl(Arena* arena, String* trait_name, TypeExprVec* type_args, StmtVec* methods, SourceLoc loc) {
    assert(arena != NULL);
    assert(trait_name != NULL);
    assert(methods != NULL);

    Stmt* stmt = arena_alloc(arena, sizeof(Stmt));
    stmt->type = STMT_IMPL;
    stmt->loc = loc;
    stmt->data.impl_def.trait_name = trait_name;
    stmt->data.impl_def.type_args = type_args;
    stmt->data.impl_def.methods = methods;

    return stmt;
}

/* Create let statement */
Stmt* stmt_let(Arena* arena, Pattern* pattern, TypeExpr* type_ann, Expr* value, SourceLoc loc) {
    assert(arena != NULL);
    assert(pattern != NULL);
    assert(value != NULL);
    
    Stmt* stmt = arena_alloc(arena, sizeof(Stmt));
    stmt->type = STMT_LET;
    stmt->loc = loc;
    stmt->data.let.pattern = pattern;
    stmt->data.let.type_ann = type_ann;
    stmt->data.let.value = value;
    stmt->data.let.else_expr = NULL;
    
    return stmt;
}

/* Create return statement */
Stmt* stmt_return(Arena* arena, Expr* value, SourceLoc loc) {
    assert(arena != NULL);
    
    Stmt* stmt = arena_alloc(arena, sizeof(Stmt));
    stmt->type = STMT_RETURN;
    stmt->loc = loc;
    stmt->data.return_stmt.value = value;
    stmt->data.return_stmt.condition = NULL;
    
    return stmt;
}

/* Create expression statement */
Stmt* stmt_expr(Arena* arena, Expr* expr, SourceLoc loc) {
    assert(arena != NULL);
    assert(expr != NULL);
    
    Stmt* stmt = arena_alloc(arena, sizeof(Stmt));
    stmt->type = STMT_EXPR;
    stmt->loc = loc;
    stmt->data.expr.expr = expr;
    
    return stmt;
}

/* Create identifier pattern */
Pattern* pattern_ident(Arena* arena, String* name, SourceLoc loc) {
    assert(arena != NULL);
    assert(name != NULL);
    
    Pattern* pat = arena_alloc(arena, sizeof(Pattern));
    pat->type = PATTERN_IDENT;
    pat->loc = loc;
    pat->data.ident = name;
    
    return pat;
}

/* Create wildcard pattern */
Pattern* pattern_wildcard(Arena* arena, SourceLoc loc) {
    assert(arena != NULL);
    
    Pattern* pat = arena_alloc(arena, sizeof(Pattern));
    pat->type = PATTERN_WILDCARD;
    pat->loc = loc;
    
    return pat;
}

Pattern* pattern_tuple(Arena* arena, PatternVec* elements, SourceLoc loc) {
    assert(arena != NULL);
    assert(elements != NULL);

    Pattern* pat = arena_alloc(arena, sizeof(Pattern));
    pat->type = PATTERN_TUPLE;
    pat->loc = loc;
    pat->data.tuple = elements;

    return pat;
}

Pattern* pattern_rest(Arena* arena, String* name, SourceLoc loc) {
    assert(arena != NULL);
    // name can be NULL for .._

    Pattern* pat = arena_alloc(arena, sizeof(Pattern));
    pat->type = PATTERN_REST;
    pat->loc = loc;
    pat->data.rest_name = name;

    return pat;
}

Pattern* pattern_constructor(Arena* arena, String* name, PatternVec* args, SourceLoc loc) {
    assert(arena != NULL);
    assert(name != NULL);
    assert(args != NULL);

    Pattern* pat = arena_alloc(arena, sizeof(Pattern));
    pat->type = PATTERN_CONSTRUCTOR;
    pat->loc = loc;
    pat->data.constructor.name = name;
    pat->data.constructor.args = args;

    return pat;
}
