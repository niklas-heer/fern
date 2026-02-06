/* AST Validation Implementation */

#include "ast_validate.h"
#include <assert.h>

/**
 * Populate a validation error and return false for convenience.
 * @param err The error output to populate.
 * @param loc The source location associated with the error.
 * @param message The error message string.
 * @return false to allow `return set_error(...)` usage.
 */
static bool set_error(AstValidationError* err, SourceLoc loc, const char* message) {
    assert(err != NULL);
    assert(message != NULL);
    err->message = message;
    err->loc = loc;
    return false;
}

static bool validate_expr_vec(const ExprVec* vec, AstValidationError* err);
static bool validate_pattern_vec(const PatternVec* vec, AstValidationError* err);
static bool validate_type_vec(const TypeExprVec* vec, AstValidationError* err);
static bool validate_stmt_vec(const StmtVec* vec, AstValidationError* err);
static bool validate_match_arms(const MatchArmVec* arms, AstValidationError* err);

/**
 * Validate a vector of expressions.
 * @param vec The expression vector.
 * @param err The error output to populate on failure.
 * @return true if all expressions are valid.
 */
static bool validate_expr_vec(const ExprVec* vec, AstValidationError* err) {
    assert(err != NULL);
    assert(vec != NULL);
    if (!vec) return set_error(err, (SourceLoc){0}, "expression list is NULL");
    for (size_t i = 0; i < vec->len; i++) {
        if (!ast_validate_expr(vec->data[i], err)) return false;
    }
    return true;
}

/**
 * Validate a vector of patterns.
 * @param vec The pattern vector.
 * @param err The error output to populate on failure.
 * @return true if all patterns are valid.
 */
static bool validate_pattern_vec(const PatternVec* vec, AstValidationError* err) {
    assert(err != NULL);
    assert(vec != NULL);
    if (!vec) return set_error(err, (SourceLoc){0}, "pattern list is NULL");
    for (size_t i = 0; i < vec->len; i++) {
        if (!ast_validate_pattern(vec->data[i], err)) return false;
    }
    return true;
}

/**
 * Validate a vector of type expressions.
 * @param vec The type expression vector.
 * @param err The error output to populate on failure.
 * @return true if all types are valid.
 */
static bool validate_type_vec(const TypeExprVec* vec, AstValidationError* err) {
    assert(err != NULL);
    assert(vec != NULL);
    if (!vec) return set_error(err, (SourceLoc){0}, "type list is NULL");
    for (size_t i = 0; i < vec->len; i++) {
        if (!ast_validate_type(vec->data[i], err)) return false;
    }
    return true;
}

/**
 * Validate a vector of statements.
 * @param vec The statement vector.
 * @param err The error output to populate on failure.
 * @return true if all statements are valid.
 */
static bool validate_stmt_vec(const StmtVec* vec, AstValidationError* err) {
    assert(err != NULL);
    assert(vec != NULL);
    if (!vec) return set_error(err, (SourceLoc){0}, "statement list is NULL");
    for (size_t i = 0; i < vec->len; i++) {
        if (!ast_validate_stmt(vec->data[i], err)) return false;
    }
    return true;
}

/**
 * Validate match arms and their patterns/expressions.
 * @param arms The match arm vector.
 * @param err The error output to populate on failure.
 * @return true if all arms are valid.
 */
static bool validate_match_arms(const MatchArmVec* arms, AstValidationError* err) {
    assert(err != NULL);
    assert(arms != NULL);
    if (!arms) return set_error(err, (SourceLoc){0}, "match arms list is NULL");
    for (size_t i = 0; i < arms->len; i++) {
        MatchArm* arm = &arms->data[i];
        if (!arm->pattern) return set_error(err, (SourceLoc){0}, "match arm pattern is NULL");
        if (!arm->body) return set_error(err, (SourceLoc){0}, "match arm body is NULL");
        if (!ast_validate_pattern(arm->pattern, err)) return false;
        if (arm->guard && !ast_validate_expr(arm->guard, err)) return false;
        if (!ast_validate_expr(arm->body, err)) return false;
    }
    return true;
}

/**
 * Validate a full program statement list.
 * @param stmts The top-level statements.
 * @param err The error output to populate on failure.
 * @return true if the program is structurally valid.
 */
bool ast_validate_program(const StmtVec* stmts, AstValidationError* err) {
    assert(err != NULL);
    assert(stmts != NULL);
    if (!stmts) return set_error(err, (SourceLoc){0}, "program statements are NULL");
    return validate_stmt_vec(stmts, err);
}

/**
 * Validate a pattern node.
 * @param pattern The pattern to validate.
 * @param err The error output to populate on failure.
 * @return true if the pattern is structurally valid.
 */
bool ast_validate_pattern(const Pattern* pattern, AstValidationError* err) {
    assert(err != NULL);
    assert(pattern != NULL);
    if (!pattern) return set_error(err, (SourceLoc){0}, "pattern is NULL");

    switch (pattern->type) {
        case PATTERN_IDENT:
            if (!pattern->data.ident) return set_error(err, pattern->loc, "pattern ident name is NULL");
            return true;
        case PATTERN_WILDCARD:
            return true;
        case PATTERN_LIT:
            if (!pattern->data.literal) return set_error(err, pattern->loc, "pattern literal is NULL");
            return ast_validate_expr(pattern->data.literal, err);
        case PATTERN_CONSTRUCTOR:
            if (!pattern->data.constructor.name) return set_error(err, pattern->loc, "constructor name is NULL");
            if (!pattern->data.constructor.args) return set_error(err, pattern->loc, "constructor args are NULL");
            return validate_pattern_vec(pattern->data.constructor.args, err);
        case PATTERN_TUPLE:
            if (!pattern->data.tuple) return set_error(err, pattern->loc, "tuple pattern elements are NULL");
            return validate_pattern_vec(pattern->data.tuple, err);
        case PATTERN_REST:
            return true;
        default:
            return set_error(err, pattern->loc, "unknown pattern type");
    }
}

/**
 * Validate a type expression node.
 * @param type The type expression to validate.
 * @param err The error output to populate on failure.
 * @return true if the type expression is structurally valid.
 */
bool ast_validate_type(const TypeExpr* type, AstValidationError* err) {
    assert(err != NULL);
    assert(type != NULL);
    if (!type) return set_error(err, (SourceLoc){0}, "type expression is NULL");

    switch (type->kind) {
        case TYPEEXPR_NAMED:
            if (!type->data.named.name) return set_error(err, type->loc, "type name is NULL");
            if (type->data.named.args && !validate_type_vec(type->data.named.args, err)) return false;
            return true;
        case TYPEEXPR_FUNCTION:
            if (!type->data.func.params) return set_error(err, type->loc, "function type params are NULL");
            if (!type->data.func.return_type) return set_error(err, type->loc, "function type return is NULL");
            if (!validate_type_vec(type->data.func.params, err)) return false;
            return ast_validate_type(type->data.func.return_type, err);
        case TYPEEXPR_TUPLE:
            if (!type->data.tuple.elements) return set_error(err, type->loc, "tuple type elements are NULL");
            return validate_type_vec(type->data.tuple.elements, err);
        default:
            return set_error(err, type->loc, "unknown type expression kind");
    }
}

/**
 * Validate an expression node.
 * @param expr The expression to validate.
 * @param err The error output to populate on failure.
 * @return true if the expression is structurally valid.
 */
bool ast_validate_expr(const Expr* expr, AstValidationError* err) {
    // FERN_STYLE: allow(function-length) validator covers all expression variants
    assert(err != NULL);
    assert(expr != NULL);
    if (!expr) return set_error(err, (SourceLoc){0}, "expression is NULL");

    switch (expr->type) {
        case EXPR_INT_LIT:
        case EXPR_FLOAT_LIT:
        case EXPR_BOOL_LIT:
            return true;
        case EXPR_STRING_LIT:
            if (!expr->data.string_lit.value) return set_error(err, expr->loc, "string literal is NULL");
            return true;
        case EXPR_IDENT:
            if (!expr->data.ident.name) return set_error(err, expr->loc, "identifier name is NULL");
            return true;
        case EXPR_BINARY:
            if (!expr->data.binary.left) return set_error(err, expr->loc, "binary left is NULL");
            if (!expr->data.binary.right) return set_error(err, expr->loc, "binary right is NULL");
            if (!ast_validate_expr(expr->data.binary.left, err)) return false;
            return ast_validate_expr(expr->data.binary.right, err);
        case EXPR_UNARY:
            if (!expr->data.unary.operand) return set_error(err, expr->loc, "unary operand is NULL");
            return ast_validate_expr(expr->data.unary.operand, err);
        case EXPR_CALL:
            if (!expr->data.call.func) return set_error(err, expr->loc, "call func is NULL");
            if (!expr->data.call.args) return set_error(err, expr->loc, "call args are NULL");
            if (!ast_validate_expr(expr->data.call.func, err)) return false;
            for (size_t i = 0; i < expr->data.call.args->len; i++) {
                CallArg* arg = &expr->data.call.args->data[i];
                if (!arg->value) return set_error(err, expr->loc, "call arg value is NULL");
                if (!ast_validate_expr(arg->value, err)) return false;
            }
            return true;
        case EXPR_IF:
            if (!expr->data.if_expr.condition) return set_error(err, expr->loc, "if condition is NULL");
            if (!expr->data.if_expr.then_branch) return set_error(err, expr->loc, "if then is NULL");
            if (!ast_validate_expr(expr->data.if_expr.condition, err)) return false;
            if (!ast_validate_expr(expr->data.if_expr.then_branch, err)) return false;
            if (expr->data.if_expr.else_branch && !ast_validate_expr(expr->data.if_expr.else_branch, err)) return false;
            return true;
        case EXPR_MATCH:
            if (!expr->data.match_expr.value) return set_error(err, expr->loc, "match value is NULL");
            if (!expr->data.match_expr.arms) return set_error(err, expr->loc, "match arms are NULL");
            if (!ast_validate_expr(expr->data.match_expr.value, err)) return false;
            return validate_match_arms(expr->data.match_expr.arms, err);
        case EXPR_BLOCK:
            if (!expr->data.block.stmts) return set_error(err, expr->loc, "block statements are NULL");
            if (!validate_stmt_vec(expr->data.block.stmts, err)) return false;
            if (expr->data.block.final_expr && !ast_validate_expr(expr->data.block.final_expr, err)) return false;
            return true;
        case EXPR_LIST:
            if (!expr->data.list.elements) return set_error(err, expr->loc, "list elements are NULL");
            return validate_expr_vec(expr->data.list.elements, err);
        case EXPR_BIND:
            if (!expr->data.bind.name) return set_error(err, expr->loc, "bind name is NULL");
            if (!expr->data.bind.value) return set_error(err, expr->loc, "bind value is NULL");
            return ast_validate_expr(expr->data.bind.value, err);
        case EXPR_WITH:
            if (!expr->data.with_expr.bindings) return set_error(err, expr->loc, "with bindings are NULL");
            if (!expr->data.with_expr.body) return set_error(err, expr->loc, "with body is NULL");
            for (size_t i = 0; i < expr->data.with_expr.bindings->len; i++) {
                WithBinding* b = &expr->data.with_expr.bindings->data[i];
                if (!b->name) return set_error(err, expr->loc, "with binding name is NULL");
                if (!b->value) return set_error(err, expr->loc, "with binding value is NULL");
                if (!ast_validate_expr(b->value, err)) return false;
            }
            if (!ast_validate_expr(expr->data.with_expr.body, err)) return false;
            if (expr->data.with_expr.else_arms && !validate_match_arms(expr->data.with_expr.else_arms, err)) return false;
            return true;
        case EXPR_DOT:
            if (!expr->data.dot.object) return set_error(err, expr->loc, "dot object is NULL");
            if (!expr->data.dot.field) return set_error(err, expr->loc, "dot field is NULL");
            return ast_validate_expr(expr->data.dot.object, err);
        case EXPR_RANGE:
            if (!expr->data.range.start) return set_error(err, expr->loc, "range start is NULL");
            if (!expr->data.range.end) return set_error(err, expr->loc, "range end is NULL");
            if (!ast_validate_expr(expr->data.range.start, err)) return false;
            return ast_validate_expr(expr->data.range.end, err);
        case EXPR_FOR:
            if (!expr->data.for_loop.var_name) return set_error(err, expr->loc, "for var name is NULL");
            if (!expr->data.for_loop.iterable) return set_error(err, expr->loc, "for iterable is NULL");
            if (!expr->data.for_loop.body) return set_error(err, expr->loc, "for body is NULL");
            if (!ast_validate_expr(expr->data.for_loop.iterable, err)) return false;
            return ast_validate_expr(expr->data.for_loop.body, err);
        case EXPR_LAMBDA:
            if (!expr->data.lambda.params) return set_error(err, expr->loc, "lambda params are NULL");
            if (!expr->data.lambda.body) return set_error(err, expr->loc, "lambda body is NULL");
            for (size_t i = 0; i < expr->data.lambda.params->len; i++) {
                if (!expr->data.lambda.params->data[i]) {
                    return set_error(err, expr->loc, "lambda param name is NULL");
                }
            }
            return ast_validate_expr(expr->data.lambda.body, err);
        case EXPR_INTERP_STRING:
            if (!expr->data.interp_string.parts) return set_error(err, expr->loc, "interp parts are NULL");
            return validate_expr_vec(expr->data.interp_string.parts, err);
        case EXPR_MAP:
            if (!expr->data.map.entries) return set_error(err, expr->loc, "map entries are NULL");
            for (size_t i = 0; i < expr->data.map.entries->len; i++) {
                MapEntry* entry = &expr->data.map.entries->data[i];
                if (!entry->key) return set_error(err, expr->loc, "map entry key is NULL");
                if (!entry->value) return set_error(err, expr->loc, "map entry value is NULL");
                if (!ast_validate_expr(entry->key, err)) return false;
                if (!ast_validate_expr(entry->value, err)) return false;
            }
            return true;
        case EXPR_TUPLE:
            if (!expr->data.tuple.elements) return set_error(err, expr->loc, "tuple elements are NULL");
            return validate_expr_vec(expr->data.tuple.elements, err);
        case EXPR_RECORD_UPDATE:
            if (!expr->data.record_update.base) return set_error(err, expr->loc, "record update base is NULL");
            if (!expr->data.record_update.fields) return set_error(err, expr->loc, "record update fields are NULL");
            if (!ast_validate_expr(expr->data.record_update.base, err)) return false;
            for (size_t i = 0; i < expr->data.record_update.fields->len; i++) {
                RecordField* field = &expr->data.record_update.fields->data[i];
                if (!field->name) return set_error(err, expr->loc, "record field name is NULL");
                if (!field->value) return set_error(err, expr->loc, "record field value is NULL");
                if (!ast_validate_expr(field->value, err)) return false;
            }
            return true;
        case EXPR_LIST_COMP:
            if (!expr->data.list_comp.body) return set_error(err, expr->loc, "list comp body is NULL");
            if (!expr->data.list_comp.var_name) return set_error(err, expr->loc, "list comp var name is NULL");
            if (!expr->data.list_comp.iterable) return set_error(err, expr->loc, "list comp iterable is NULL");
            if (!ast_validate_expr(expr->data.list_comp.body, err)) return false;
            if (!ast_validate_expr(expr->data.list_comp.iterable, err)) return false;
            if (expr->data.list_comp.condition && !ast_validate_expr(expr->data.list_comp.condition, err)) return false;
            return true;
        case EXPR_INDEX:
            if (!expr->data.index_expr.object) return set_error(err, expr->loc, "index object is NULL");
            if (!expr->data.index_expr.index) return set_error(err, expr->loc, "index expression is NULL");
            if (!ast_validate_expr(expr->data.index_expr.object, err)) return false;
            return ast_validate_expr(expr->data.index_expr.index, err);
        case EXPR_SPAWN:
            if (!expr->data.spawn_expr.func) return set_error(err, expr->loc, "spawn func is NULL");
            return ast_validate_expr(expr->data.spawn_expr.func, err);
        case EXPR_SEND:
            if (!expr->data.send_expr.pid) return set_error(err, expr->loc, "send pid is NULL");
            if (!expr->data.send_expr.message) return set_error(err, expr->loc, "send message is NULL");
            if (!ast_validate_expr(expr->data.send_expr.pid, err)) return false;
            return ast_validate_expr(expr->data.send_expr.message, err);
        case EXPR_RECEIVE:
            if (!expr->data.receive_expr.arms) return set_error(err, expr->loc, "receive arms are NULL");
            if (!validate_match_arms(expr->data.receive_expr.arms, err)) return false;
            if (expr->data.receive_expr.after_timeout && !expr->data.receive_expr.after_body) {
                return set_error(err, expr->loc, "receive after timeout without body");
            }
            if (expr->data.receive_expr.after_body && !ast_validate_expr(expr->data.receive_expr.after_body, err)) return false;
            if (expr->data.receive_expr.after_timeout && !ast_validate_expr(expr->data.receive_expr.after_timeout, err)) return false;
            return true;
        case EXPR_TRY:
            if (!expr->data.try_expr.operand) return set_error(err, expr->loc, "try operand is NULL");
            return ast_validate_expr(expr->data.try_expr.operand, err);
        default:
            return set_error(err, expr->loc, "unknown expression type");
    }
}

/**
 * Validate a statement node.
 * @param stmt The statement to validate.
 * @param err The error output to populate on failure.
 * @return true if the statement is structurally valid.
 */
bool ast_validate_stmt(const Stmt* stmt, AstValidationError* err) {
    // FERN_STYLE: allow(function-length) validator covers all statement variants
    assert(err != NULL);
    assert(stmt != NULL);
    if (!stmt) return set_error(err, (SourceLoc){0}, "statement is NULL");

    switch (stmt->type) {
        case STMT_LET:
            if (!stmt->data.let.pattern) return set_error(err, stmt->loc, "let pattern is NULL");
            if (!stmt->data.let.value) return set_error(err, stmt->loc, "let value is NULL");
            if (!ast_validate_pattern(stmt->data.let.pattern, err)) return false;
            if (stmt->data.let.type_ann && !ast_validate_type(stmt->data.let.type_ann, err)) return false;
            if (!ast_validate_expr(stmt->data.let.value, err)) return false;
            if (stmt->data.let.else_expr && !ast_validate_expr(stmt->data.let.else_expr, err)) return false;
            return true;
        case STMT_RETURN:
            if (stmt->data.return_stmt.value && !ast_validate_expr(stmt->data.return_stmt.value, err)) return false;
            if (stmt->data.return_stmt.condition && !ast_validate_expr(stmt->data.return_stmt.condition, err)) return false;
            return true;
        case STMT_EXPR:
            if (!stmt->data.expr.expr) return set_error(err, stmt->loc, "expression statement is NULL");
            return ast_validate_expr(stmt->data.expr.expr, err);
        case STMT_FN:
            if (!stmt->data.fn.name) return set_error(err, stmt->loc, "function name is NULL");
            if (stmt->data.fn.params && stmt->data.fn.clauses) {
                return set_error(err, stmt->loc, "function has both params and clauses");
            }
            if (!stmt->data.fn.params && !stmt->data.fn.clauses) {
                return set_error(err, stmt->loc, "function missing params and clauses");
            }
            if (stmt->data.fn.params) {
                for (size_t i = 0; i < stmt->data.fn.params->len; i++) {
                    Parameter* p = &stmt->data.fn.params->data[i];
                    if (!p->name) return set_error(err, stmt->loc, "function param name is NULL");
                    if (!p->type_ann) return set_error(err, stmt->loc, "function param type is NULL");
                    if (!ast_validate_type(p->type_ann, err)) return false;
                }
            }
            if (stmt->data.fn.params && !stmt->data.fn.body) {
                return set_error(err, stmt->loc, "function body is NULL");
            }
            if (stmt->data.fn.where_clauses && !validate_type_vec(stmt->data.fn.where_clauses, err)) return false;
            if (stmt->data.fn.return_type && !ast_validate_type(stmt->data.fn.return_type, err)) return false;
            if (stmt->data.fn.body && !ast_validate_expr(stmt->data.fn.body, err)) return false;
            if (stmt->data.fn.clauses) {
                if (stmt->data.fn.clauses->len == 0) {
                    return set_error(err, stmt->loc, "function clauses are empty");
                }
                for (size_t i = 0; i < stmt->data.fn.clauses->len; i++) {
                    FunctionClause* clause = &stmt->data.fn.clauses->data[i];
                    if (!clause->params) return set_error(err, stmt->loc, "function clause params are NULL");
                    if (!clause->body) return set_error(err, stmt->loc, "function clause body is NULL");
                    if (!validate_pattern_vec(clause->params, err)) return false;
                    if (clause->return_type && !ast_validate_type(clause->return_type, err)) return false;
                    if (!ast_validate_expr(clause->body, err)) return false;
                }
            }
            return true;
        case STMT_IMPORT:
            if (!stmt->data.import.path) return set_error(err, stmt->loc, "import path is NULL");
            for (size_t i = 0; i < stmt->data.import.path->len; i++) {
                if (!stmt->data.import.path->data[i]) return set_error(err, stmt->loc, "import path segment is NULL");
            }
            if (stmt->data.import.items) {
                for (size_t i = 0; i < stmt->data.import.items->len; i++) {
                    if (!stmt->data.import.items->data[i]) return set_error(err, stmt->loc, "import item is NULL");
                }
            }
            return true;
        case STMT_DEFER:
            if (!stmt->data.defer_stmt.expr) return set_error(err, stmt->loc, "defer expr is NULL");
            return ast_validate_expr(stmt->data.defer_stmt.expr, err);
        case STMT_TYPE_DEF:
            if (!stmt->data.type_def.name) return set_error(err, stmt->loc, "type name is NULL");
            if (stmt->data.type_def.type_params) {
                for (size_t i = 0; i < stmt->data.type_def.type_params->len; i++) {
                    if (!stmt->data.type_def.type_params->data[i]) {
                        return set_error(err, stmt->loc, "type parameter is NULL");
                    }
                }
            }
            if (stmt->data.type_def.derives) {
                for (size_t i = 0; i < stmt->data.type_def.derives->len; i++) {
                    if (!stmt->data.type_def.derives->data[i]) {
                        return set_error(err, stmt->loc, "derive name is NULL");
                    }
                }
            }
            if (stmt->data.type_def.variants) {
                for (size_t i = 0; i < stmt->data.type_def.variants->len; i++) {
                    TypeVariant* v = &stmt->data.type_def.variants->data[i];
                    if (!v->name) return set_error(err, stmt->loc, "variant name is NULL");
                    if (v->fields) {
                        for (size_t j = 0; j < v->fields->len; j++) {
                            TypeField* f = &v->fields->data[j];
                            if (!f->type_ann) return set_error(err, stmt->loc, "variant field type is NULL");
                            if (!ast_validate_type(f->type_ann, err)) return false;
                        }
                    }
                }
            }
            if (stmt->data.type_def.record_fields) {
                for (size_t i = 0; i < stmt->data.type_def.record_fields->len; i++) {
                    TypeField* f = &stmt->data.type_def.record_fields->data[i];
                    if (!f->name) return set_error(err, stmt->loc, "record field name is NULL");
                    if (!f->type_ann) return set_error(err, stmt->loc, "record field type is NULL");
                    if (!ast_validate_type(f->type_ann, err)) return false;
                }
            }
            return true;
        case STMT_BREAK:
            if (stmt->data.break_stmt.value && !ast_validate_expr(stmt->data.break_stmt.value, err)) return false;
            return true;
        case STMT_CONTINUE:
            return true;
        case STMT_TRAIT:
            if (!stmt->data.trait_def.name) return set_error(err, stmt->loc, "trait name is NULL");
            if (stmt->data.trait_def.type_params) {
                for (size_t i = 0; i < stmt->data.trait_def.type_params->len; i++) {
                    if (!stmt->data.trait_def.type_params->data[i]) {
                        return set_error(err, stmt->loc, "trait type param is NULL");
                    }
                }
            }
            if (stmt->data.trait_def.constraints && !validate_type_vec(stmt->data.trait_def.constraints, err)) return false;
            if (!stmt->data.trait_def.methods) return set_error(err, stmt->loc, "trait methods are NULL");
            return validate_stmt_vec(stmt->data.trait_def.methods, err);
        case STMT_IMPL:
            if (!stmt->data.impl_def.trait_name) return set_error(err, stmt->loc, "impl trait name is NULL");
            if (stmt->data.impl_def.type_args && !validate_type_vec(stmt->data.impl_def.type_args, err)) return false;
            if (!stmt->data.impl_def.methods) return set_error(err, stmt->loc, "impl methods are NULL");
            return validate_stmt_vec(stmt->data.impl_def.methods, err);
        case STMT_NEWTYPE:
            if (!stmt->data.newtype_def.name) return set_error(err, stmt->loc, "newtype name is NULL");
            if (!stmt->data.newtype_def.constructor) return set_error(err, stmt->loc, "newtype ctor is NULL");
            if (!stmt->data.newtype_def.inner_type) return set_error(err, stmt->loc, "newtype inner type is NULL");
            return ast_validate_type(stmt->data.newtype_def.inner_type, err);
        case STMT_MODULE:
            if (!stmt->data.module_decl.path) return set_error(err, stmt->loc, "module path is NULL");
            for (size_t i = 0; i < stmt->data.module_decl.path->len; i++) {
                if (!stmt->data.module_decl.path->data[i]) return set_error(err, stmt->loc, "module path segment is NULL");
            }
            return true;
        default:
            return set_error(err, stmt->loc, "unknown statement type");
    }
}
