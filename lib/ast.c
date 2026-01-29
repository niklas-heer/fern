/* AST Helper Functions */

#include "ast.h"
#include <assert.h>
#include <string.h>

/**
 * Create integer literal expression.
 * @param arena The arena to allocate from.
 * @param value The integer value.
 * @param loc The source location.
 * @return The new expression node.
 */
Expr* expr_int_lit(Arena* arena, int64_t value, SourceLoc loc) {
    assert(arena != NULL);
    assert(loc.line >= 0);
    
    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_INT_LIT;
    expr->loc = loc;
    expr->data.int_lit.value = value;
    
    return expr;
}

/**
 * Create float literal expression.
 * @param arena The arena to allocate from.
 * @param value The float value.
 * @param loc The source location.
 * @return The new expression node.
 */
Expr* expr_float_lit(Arena* arena, double value, SourceLoc loc) {
    assert(arena != NULL);
    assert(loc.line >= 0);

    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_FLOAT_LIT;
    expr->loc = loc;
    expr->data.float_lit.value = value;

    return expr;
}

/**
 * Create string literal expression.
 * @param arena The arena to allocate from.
 * @param value The string value.
 * @param loc The source location.
 * @return The new expression node.
 */
Expr* expr_string_lit(Arena* arena, String* value, SourceLoc loc) {
    assert(arena != NULL);
    assert(value != NULL);
    
    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_STRING_LIT;
    expr->loc = loc;
    expr->data.string_lit.value = value;
    
    return expr;
}

/**
 * Create boolean literal expression.
 * @param arena The arena to allocate from.
 * @param value The boolean value.
 * @param loc The source location.
 * @return The new expression node.
 */
Expr* expr_bool_lit(Arena* arena, bool value, SourceLoc loc) {
    assert(arena != NULL);
    assert(loc.line >= 0);
    
    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_BOOL_LIT;
    expr->loc = loc;
    expr->data.bool_lit.value = value;
    
    return expr;
}

/**
 * Create identifier expression.
 * @param arena The arena to allocate from.
 * @param name The identifier name.
 * @param loc The source location.
 * @return The new expression node.
 */
Expr* expr_ident(Arena* arena, String* name, SourceLoc loc) {
    assert(arena != NULL);
    assert(name != NULL);
    
    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_IDENT;
    expr->loc = loc;
    expr->data.ident.name = name;
    
    return expr;
}

/**
 * Create binary expression.
 * @param arena The arena to allocate from.
 * @param op The binary operator.
 * @param left The left operand.
 * @param right The right operand.
 * @param loc The source location.
 * @return The new expression node.
 */
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

/**
 * Create unary expression.
 * @param arena The arena to allocate from.
 * @param op The unary operator.
 * @param operand The operand expression.
 * @param loc The source location.
 * @return The new expression node.
 */
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

/**
 * Create call expression.
 * @param arena The arena to allocate from.
 * @param func The function expression.
 * @param args Array of argument expressions.
 * @param num_args Number of arguments.
 * @param loc The source location.
 * @return The new expression node.
 */
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

/**
 * Create if expression.
 * @param arena The arena to allocate from.
 * @param condition The condition expression.
 * @param then_branch The then branch expression.
 * @param else_branch The else branch expression (may be NULL).
 * @param loc The source location.
 * @return The new expression node.
 */
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

/**
 * Create match expression.
 * @param arena The arena to allocate from.
 * @param value The value to match against.
 * @param arms The match arms.
 * @param loc The source location.
 * @return The new expression node.
 */
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

/**
 * Create block expression.
 * @param arena The arena to allocate from.
 * @param stmts The statements in the block.
 * @param final_expr The final expression (may be NULL).
 * @param loc The source location.
 * @return The new expression node.
 */
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

/**
 * Create list expression.
 * @param arena The arena to allocate from.
 * @param elements The list elements.
 * @param loc The source location.
 * @return The new expression node.
 */
Expr* expr_list(Arena* arena, ExprVec* elements, SourceLoc loc) {
    assert(arena != NULL);
    assert(elements != NULL);
    
    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_LIST;
    expr->loc = loc;
    expr->data.list.elements = elements;
    
    return expr;
}

/**
 * Create bind expression (name <- value).
 * @param arena The arena to allocate from.
 * @param name The binding name.
 * @param value The value expression.
 * @param loc The source location.
 * @return The new expression node.
 */
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

/**
 * Create with expression.
 * @param arena The arena to allocate from.
 * @param bindings The with bindings.
 * @param body The body expression.
 * @param else_arms The else arms (may be NULL).
 * @param loc The source location.
 * @return The new expression node.
 */
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

/**
 * Create dot access expression.
 * @param arena The arena to allocate from.
 * @param object The object expression.
 * @param field The field name.
 * @param loc The source location.
 * @return The new expression node.
 */
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

/**
 * Create range expression.
 * @param arena The arena to allocate from.
 * @param start The start expression.
 * @param end The end expression.
 * @param inclusive Whether the range is inclusive.
 * @param loc The source location.
 * @return The new expression node.
 */
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

/**
 * Create for loop expression.
 * @param arena The arena to allocate from.
 * @param var_name The loop variable name.
 * @param iterable The iterable expression.
 * @param body The loop body.
 * @param loc The source location.
 * @return The new expression node.
 */
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

/**
 * Create a lambda expression node.
 * @param arena The arena to allocate from.
 * @param params The parameter names.
 * @param body The lambda body.
 * @param loc The source location.
 * @return The new expression node.
 */
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

/**
 * Create a tuple expression node.
 * @param arena The arena to allocate from.
 * @param elements The tuple elements.
 * @param loc The source location.
 * @return The new expression node.
 */
Expr* expr_tuple(Arena* arena, ExprVec* elements, SourceLoc loc) {
    assert(arena != NULL);
    assert(elements != NULL);

    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_TUPLE;
    expr->loc = loc;
    expr->data.tuple.elements = elements;

    return expr;
}

/**
 * Create a record update expression node.
 * @param arena The arena to allocate from.
 * @param base The base record expression.
 * @param fields The fields to update.
 * @param loc The source location.
 * @return The new expression node.
 */
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

/**
 * Create a list comprehension expression node.
 * @param arena The arena to allocate from.
 * @param body The body expression.
 * @param var_name The loop variable name.
 * @param iterable The iterable expression.
 * @param condition The filter condition (may be NULL).
 * @param loc The source location.
 * @return The new expression node.
 */
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

/**
 * Create an index expression node.
 * @param arena The arena to allocate from.
 * @param object The object to index.
 * @param index The index expression.
 * @param loc The source location.
 * @return The new expression node.
 */
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

/**
 * Create a spawn expression node.
 * @param arena The arena to allocate from.
 * @param func The function to spawn.
 * @param loc The source location.
 * @return The new expression node.
 */
Expr* expr_spawn(Arena* arena, Expr* func, SourceLoc loc) {
    assert(arena != NULL);
    assert(func != NULL);

    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_SPAWN;
    expr->loc = loc;
    expr->data.spawn_expr.func = func;

    return expr;
}

/**
 * Create a send expression node.
 * @param arena The arena to allocate from.
 * @param pid The process ID to send to.
 * @param message The message expression.
 * @param loc The source location.
 * @return The new expression node.
 */
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

/**
 * Create a receive expression node.
 * @param arena The arena to allocate from.
 * @param arms The receive match arms.
 * @param after_timeout The timeout expression (may be NULL).
 * @param after_body The timeout body (may be NULL).
 * @param loc The source location.
 * @return The new expression node.
 */
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

/**
 * Create a try expression node (? operator).
 * @param arena The arena to allocate from.
 * @param operand The operand expression.
 * @param loc The source location.
 * @return The new expression node.
 */
Expr* expr_try(Arena* arena, Expr* operand, SourceLoc loc) {
    assert(arena != NULL);
    assert(operand != NULL);

    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_TRY;
    expr->loc = loc;
    expr->data.try_expr.operand = operand;

    return expr;
}

/**
 * Create a map literal expression node.
 * @param arena The arena to allocate from.
 * @param entries The map entries.
 * @param loc The source location.
 * @return The new expression node.
 */
Expr* expr_map(Arena* arena, MapEntryVec* entries, SourceLoc loc) {
    assert(arena != NULL);
    assert(entries != NULL);

    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_MAP;
    expr->loc = loc;
    expr->data.map.entries = entries;

    return expr;
}

/**
 * Create an interpolated string expression node.
 * @param arena The arena to allocate from.
 * @param parts The string parts.
 * @param loc The source location.
 * @return The new expression node.
 */
Expr* expr_interp_string(Arena* arena, ExprVec* parts, SourceLoc loc) {
    assert(arena != NULL);
    assert(parts != NULL);

    Expr* expr = arena_alloc(arena, sizeof(Expr));
    expr->type = EXPR_INTERP_STRING;
    expr->loc = loc;
    expr->data.interp_string.parts = parts;

    return expr;
}

/**
 * Create named type expression.
 * @param arena The arena to allocate from.
 * @param name The type name.
 * @param args The type arguments (may be NULL).
 * @param loc The source location.
 * @return The new type expression node.
 */
TypeExpr* type_named(Arena* arena, String* name, TypeExprVec* args, SourceLoc loc) {
    assert(arena != NULL);
    assert(name != NULL);

    TypeExpr* type = arena_alloc(arena, sizeof(TypeExpr));
    type->kind = TYPEEXPR_NAMED;
    type->loc = loc;
    type->data.named.name = name;
    type->data.named.args = args;  // Can be NULL for simple types

    return type;
}

/**
 * Create function type expression.
 * @param arena The arena to allocate from.
 * @param params The parameter types.
 * @param return_type The return type.
 * @param loc The source location.
 * @return The new type expression node.
 */
TypeExpr* type_function(Arena* arena, TypeExprVec* params, TypeExpr* return_type, SourceLoc loc) {
    assert(arena != NULL);
    assert(params != NULL);
    assert(return_type != NULL);

    TypeExpr* type = arena_alloc(arena, sizeof(TypeExpr));
    type->kind = TYPEEXPR_FUNCTION;
    type->loc = loc;
    type->data.func.params = params;
    type->data.func.return_type = return_type;

    return type;
}

/**
 * Create tuple type expression.
 * @param arena The arena to allocate from.
 * @param elements The element types.
 * @param loc The source location.
 * @return The new type expression node.
 */
TypeExpr* type_tuple_expr(Arena* arena, TypeExprVec* elements, SourceLoc loc) {
    assert(arena != NULL);
    assert(elements != NULL);

    TypeExpr* type = arena_alloc(arena, sizeof(TypeExpr));
    type->kind = TYPEEXPR_TUPLE;
    type->loc = loc;
    type->data.tuple.elements = elements;

    return type;
}

/**
 * Create function definition statement.
 * @param arena The arena to allocate from.
 * @param name The function name.
 * @param is_public Whether the function is public.
 * @param params The function parameters.
 * @param return_type The return type (may be NULL).
 * @param body The function body.
 * @param loc The source location.
 * @return The new statement node.
 */
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

/**
 * Create import declaration statement.
 * @param arena The arena to allocate from.
 * @param path The import path.
 * @param items The imported items (may be NULL).
 * @param alias The import alias (may be NULL).
 * @param loc The source location.
 * @return The new statement node.
 */
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

/**
 * Create defer statement.
 * @param arena The arena to allocate from.
 * @param expr The deferred expression.
 * @param loc The source location.
 * @return The new statement node.
 */
Stmt* stmt_defer(Arena* arena, Expr* expr, SourceLoc loc) {
    assert(arena != NULL);
    assert(expr != NULL);

    Stmt* stmt = arena_alloc(arena, sizeof(Stmt));
    stmt->type = STMT_DEFER;
    stmt->loc = loc;
    stmt->data.defer_stmt.expr = expr;

    return stmt;
}

/**
 * Create type definition statement.
 * @param arena The arena to allocate from.
 * @param name The type name.
 * @param is_public Whether the type is public.
 * @param type_params The type parameters (may be NULL).
 * @param derives The derived traits (may be NULL).
 * @param variants The type variants (may be NULL).
 * @param record_fields The record fields (may be NULL).
 * @param loc The source location.
 * @return The new statement node.
 */
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

/**
 * Create newtype statement.
 * @param arena The arena to allocate from.
 * @param name The newtype name.
 * @param is_public Whether the newtype is public.
 * @param constructor The constructor name.
 * @param inner_type The inner type.
 * @param loc The source location.
 * @return The new statement node.
 */
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

/**
 * Create a module declaration statement node.
 * @param arena The arena to allocate from.
 * @param path The module path.
 * @param loc The source location.
 * @return The new statement node.
 */
Stmt* stmt_module(Arena* arena, StringVec* path, SourceLoc loc) {
    assert(arena != NULL);
    assert(path != NULL);

    Stmt* stmt = arena_alloc(arena, sizeof(Stmt));
    stmt->type = STMT_MODULE;
    stmt->loc = loc;
    stmt->data.module_decl.path = path;

    return stmt;
}

/**
 * Create a break statement node.
 * @param arena The arena to allocate from.
 * @param value The break value (may be NULL).
 * @param loc The source location.
 * @return The new statement node.
 */
Stmt* stmt_break(Arena* arena, Expr* value, SourceLoc loc) {
    assert(arena != NULL);
    assert(loc.line >= 0);

    Stmt* stmt = arena_alloc(arena, sizeof(Stmt));
    stmt->type = STMT_BREAK;
    stmt->loc = loc;
    stmt->data.break_stmt.value = value;  /* Can be NULL. */

    return stmt;
}

/**
 * Create continue statement.
 * @param arena The arena to allocate from.
 * @param loc The source location.
 * @return The new statement node.
 */
Stmt* stmt_continue(Arena* arena, SourceLoc loc) {
    assert(arena != NULL);
    assert(loc.line >= 0);

    Stmt* stmt = arena_alloc(arena, sizeof(Stmt));
    stmt->type = STMT_CONTINUE;
    stmt->loc = loc;

    return stmt;
}

/**
 * Create trait definition.
 * @param arena The arena to allocate from.
 * @param name The trait name.
 * @param type_params The type parameters (may be NULL).
 * @param constraints The trait constraints (may be NULL).
 * @param methods The trait methods.
 * @param loc The source location.
 * @return The new statement node.
 */
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

/**
 * Create impl block.
 * @param arena The arena to allocate from.
 * @param trait_name The trait name.
 * @param type_args The type arguments (may be NULL).
 * @param methods The implementation methods.
 * @param loc The source location.
 * @return The new statement node.
 */
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

/**
 * Create let statement.
 * @param arena The arena to allocate from.
 * @param pattern The binding pattern.
 * @param type_ann The type annotation (may be NULL).
 * @param value The value expression.
 * @param loc The source location.
 * @return The new statement node.
 */
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

/**
 * Create return statement.
 * @param arena The arena to allocate from.
 * @param value The return value (may be NULL).
 * @param loc The source location.
 * @return The new statement node.
 */
Stmt* stmt_return(Arena* arena, Expr* value, SourceLoc loc) {
    assert(arena != NULL);
    assert(loc.line >= 0);
    
    Stmt* stmt = arena_alloc(arena, sizeof(Stmt));
    stmt->type = STMT_RETURN;
    stmt->loc = loc;
    stmt->data.return_stmt.value = value;  /* Can be NULL. */
    stmt->data.return_stmt.condition = NULL;
    
    return stmt;
}

/**
 * Create expression statement.
 * @param arena The arena to allocate from.
 * @param expr The expression.
 * @param loc The source location.
 * @return The new statement node.
 */
Stmt* stmt_expr(Arena* arena, Expr* expr, SourceLoc loc) {
    assert(arena != NULL);
    assert(expr != NULL);
    
    Stmt* stmt = arena_alloc(arena, sizeof(Stmt));
    stmt->type = STMT_EXPR;
    stmt->loc = loc;
    stmt->data.expr.expr = expr;
    
    return stmt;
}

/**
 * Create identifier pattern.
 * @param arena The arena to allocate from.
 * @param name The identifier name.
 * @param loc The source location.
 * @return The new pattern node.
 */
Pattern* pattern_ident(Arena* arena, String* name, SourceLoc loc) {
    assert(arena != NULL);
    assert(name != NULL);
    
    Pattern* pat = arena_alloc(arena, sizeof(Pattern));
    pat->type = PATTERN_IDENT;
    pat->loc = loc;
    pat->data.ident = name;
    
    return pat;
}

/**
 * Create wildcard pattern.
 * @param arena The arena to allocate from.
 * @param loc The source location.
 * @return The new pattern node.
 */
Pattern* pattern_wildcard(Arena* arena, SourceLoc loc) {
    assert(arena != NULL);
    assert(loc.line >= 0);
    
    Pattern* pat = arena_alloc(arena, sizeof(Pattern));
    pat->type = PATTERN_WILDCARD;
    pat->loc = loc;
    
    return pat;
}

/**
 * Create a tuple pattern node.
 * @param arena The arena to allocate from.
 * @param elements The tuple elements.
 * @param loc The source location.
 * @return The new pattern node.
 */
Pattern* pattern_tuple(Arena* arena, PatternVec* elements, SourceLoc loc) {
    assert(arena != NULL);
    assert(elements != NULL);

    Pattern* pat = arena_alloc(arena, sizeof(Pattern));
    pat->type = PATTERN_TUPLE;
    pat->loc = loc;
    pat->data.tuple = elements;

    return pat;
}

/**
 * Create a rest pattern node (..name or .._).
 * @param arena The arena to allocate from.
 * @param name The rest binding name (may be NULL for .._).
 * @param loc The source location.
 * @return The new pattern node.
 */
Pattern* pattern_rest(Arena* arena, String* name, SourceLoc loc) {
    assert(arena != NULL);
    assert(loc.line >= 0);
    /* name can be NULL for .._ */

    Pattern* pat = arena_alloc(arena, sizeof(Pattern));
    pat->type = PATTERN_REST;
    pat->loc = loc;
    pat->data.rest_name = name;

    return pat;
}

/**
 * Create a constructor pattern node.
 * @param arena The arena to allocate from.
 * @param name The constructor name.
 * @param args The constructor arguments.
 * @param loc The source location.
 * @return The new pattern node.
 */
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
