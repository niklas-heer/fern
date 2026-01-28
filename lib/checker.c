/* Fern Type Checker Implementation */

#include "checker.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* Error message storage */
typedef struct ErrorNode {
    String* message;
    struct ErrorNode* next;
} ErrorNode;

/* The type checker context */
struct Checker {
    Arena* arena;
    TypeEnv* env;
    ErrorNode* errors;
    ErrorNode* errors_tail;
};

/* ========== Helper Functions ========== */

static void add_error(Checker* checker, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    /* Format the error message */
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    
    ErrorNode* node = arena_alloc(checker->arena, sizeof(ErrorNode));
    node->message = string_new(checker->arena, buf);
    node->next = NULL;
    
    if (checker->errors_tail) {
        checker->errors_tail->next = node;
        checker->errors_tail = node;
    } else {
        checker->errors = node;
        checker->errors_tail = node;
    }
}

static Type* error_type(Checker* checker, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    
    add_error(checker, "%s", buf);
    return type_error(checker->arena, string_new(checker->arena, buf));
}

/* ========== Checker Creation ========== */

Checker* checker_new(Arena* arena) {
    Checker* checker = arena_alloc(arena, sizeof(Checker));
    checker->arena = arena;
    checker->env = type_env_new(arena);
    checker->errors = NULL;
    checker->errors_tail = NULL;
    return checker;
}

/* ========== Binary Operator Type Checking ========== */

static Type* check_arithmetic_op(Checker* checker, Type* left, Type* right, 
                                  const char* op_name) {
    /* Both operands must be the same numeric type */
    if (left->kind == TYPE_INT && right->kind == TYPE_INT) {
        return type_int(checker->arena);
    }
    if (left->kind == TYPE_FLOAT && right->kind == TYPE_FLOAT) {
        return type_float(checker->arena);
    }
    
    /* String concatenation with + */
    if (strcmp(op_name, "+") == 0 && 
        left->kind == TYPE_STRING && right->kind == TYPE_STRING) {
        return type_string(checker->arena);
    }
    
    return error_type(checker, "Cannot apply '%s' to %s and %s",
        op_name,
        string_cstr(type_to_string(checker->arena, left)),
        string_cstr(type_to_string(checker->arena, right)));
}

static Type* check_comparison_op(Checker* checker, Type* left, Type* right,
                                  const char* op_name) {
    /* Both operands must be the same comparable type */
    if (!type_equals(left, right)) {
        return error_type(checker, "Cannot compare %s with %s using '%s'",
            string_cstr(type_to_string(checker->arena, left)),
            string_cstr(type_to_string(checker->arena, right)),
            op_name);
    }
    
    if (!type_is_comparable(left)) {
        return error_type(checker, "Type %s is not comparable",
            string_cstr(type_to_string(checker->arena, left)));
    }
    
    return type_bool(checker->arena);
}

static Type* check_equality_op(Checker* checker, Type* left, Type* right,
                                const char* op_name) {
    (void)op_name;  /* Used for future error messages */
    
    /* Both operands must be the same type */
    if (!type_equals(left, right)) {
        return error_type(checker, "Cannot compare %s with %s for equality",
            string_cstr(type_to_string(checker->arena, left)),
            string_cstr(type_to_string(checker->arena, right)));
    }
    
    return type_bool(checker->arena);
}

static Type* check_logical_op(Checker* checker, Type* left, Type* right,
                               const char* op_name) {
    /* Both operands must be Bool */
    if (left->kind != TYPE_BOOL) {
        return error_type(checker, "Left operand of '%s' must be Bool, got %s",
            op_name, string_cstr(type_to_string(checker->arena, left)));
    }
    if (right->kind != TYPE_BOOL) {
        return error_type(checker, "Right operand of '%s' must be Bool, got %s",
            op_name, string_cstr(type_to_string(checker->arena, right)));
    }
    
    return type_bool(checker->arena);
}

static Type* check_binary_expr(Checker* checker, BinaryExpr* expr) {
    Type* left = checker_infer_expr(checker, expr->left);
    Type* right = checker_infer_expr(checker, expr->right);
    
    /* Propagate errors */
    if (left->kind == TYPE_ERROR) return left;
    if (right->kind == TYPE_ERROR) return right;
    
    switch (expr->op) {
        case BINOP_ADD:
            return check_arithmetic_op(checker, left, right, "+");
        case BINOP_SUB:
            return check_arithmetic_op(checker, left, right, "-");
        case BINOP_MUL:
            return check_arithmetic_op(checker, left, right, "*");
        case BINOP_DIV:
            return check_arithmetic_op(checker, left, right, "/");
        case BINOP_MOD:
            return check_arithmetic_op(checker, left, right, "%");
        case BINOP_POW:
            return check_arithmetic_op(checker, left, right, "**");
            
        case BINOP_LT:
            return check_comparison_op(checker, left, right, "<");
        case BINOP_LE:
            return check_comparison_op(checker, left, right, "<=");
        case BINOP_GT:
            return check_comparison_op(checker, left, right, ">");
        case BINOP_GE:
            return check_comparison_op(checker, left, right, ">=");
            
        case BINOP_EQ:
            return check_equality_op(checker, left, right, "==");
        case BINOP_NE:
            return check_equality_op(checker, left, right, "!=");
            
        case BINOP_AND:
            return check_logical_op(checker, left, right, "and");
        case BINOP_OR:
            return check_logical_op(checker, left, right, "or");
            
        case BINOP_PIPE:
            /* Pipe is special - right must be a function taking left as first arg */
            /* For now, just return the type of the result */
            /* TODO: Proper pipe type checking */
            return right;
    }
    
    return error_type(checker, "Unknown binary operator");
}

/* ========== Unary Operator Type Checking ========== */

static Type* check_unary_expr(Checker* checker, UnaryExpr* expr) {
    Type* operand = checker_infer_expr(checker, expr->operand);
    
    if (operand->kind == TYPE_ERROR) return operand;
    
    switch (expr->op) {
        case UNOP_NEG:
            if (operand->kind == TYPE_INT) {
                return type_int(checker->arena);
            }
            if (operand->kind == TYPE_FLOAT) {
                return type_float(checker->arena);
            }
            return error_type(checker, "Cannot negate %s, expected numeric type",
                string_cstr(type_to_string(checker->arena, operand)));
            
        case UNOP_NOT:
            if (operand->kind == TYPE_BOOL) {
                return type_bool(checker->arena);
            }
            return error_type(checker, "Cannot apply 'not' to %s, expected Bool",
                string_cstr(type_to_string(checker->arena, operand)));
    }
    
    return error_type(checker, "Unknown unary operator");
}

/* ========== Collection Type Checking ========== */

static Type* check_list_expr(Checker* checker, ListExpr* expr) {
    if (expr->elements->len == 0) {
        /* Empty list has type List(a) where a is a fresh type variable */
        int var_id = type_fresh_var_id();
        Type* elem_type = type_var(checker->arena, 
            string_new(checker->arena, "a"), var_id);
        return type_list(checker->arena, elem_type);
    }
    
    /* Infer type of first element */
    Type* elem_type = checker_infer_expr(checker, expr->elements->data[0]);
    if (elem_type->kind == TYPE_ERROR) return elem_type;
    
    /* Check all elements have the same type */
    for (size_t i = 1; i < expr->elements->len; i++) {
        Type* t = checker_infer_expr(checker, expr->elements->data[i]);
        if (t->kind == TYPE_ERROR) return t;
        
        if (!type_equals(elem_type, t)) {
            return error_type(checker, 
                "List element type mismatch: expected %s, got %s at index %zu",
                string_cstr(type_to_string(checker->arena, elem_type)),
                string_cstr(type_to_string(checker->arena, t)),
                i);
        }
    }
    
    return type_list(checker->arena, elem_type);
}

static Type* check_tuple_expr(Checker* checker, TupleExpr* expr) {
    TypeVec* elem_types = TypeVec_new(checker->arena);
    
    for (size_t i = 0; i < expr->elements->len; i++) {
        Type* t = checker_infer_expr(checker, expr->elements->data[i]);
        if (t->kind == TYPE_ERROR) return t;
        TypeVec_push(checker->arena, elem_types, t);
    }
    
    return type_tuple(checker->arena, elem_types);
}

/* ========== Function Call Type Checking ========== */

static Type* check_call_expr(Checker* checker, CallExpr* expr) {
    /* Infer the type of the callee */
    Type* callee_type = checker_infer_expr(checker, expr->func);
    if (callee_type->kind == TYPE_ERROR) return callee_type;
    
    /* Callee must be a function type */
    if (callee_type->kind != TYPE_FN) {
        return error_type(checker, "Cannot call non-function type %s",
            string_cstr(type_to_string(checker->arena, callee_type)));
    }
    
    TypeFn* fn = &callee_type->data.fn;
    
    /* Check argument count */
    size_t expected_count = fn->params->len;
    size_t actual_count = expr->args->len;
    
    if (actual_count != expected_count) {
        return error_type(checker, "Expected %zu arguments, got %zu",
            expected_count, actual_count);
    }
    
    /* Check argument types */
    for (size_t i = 0; i < actual_count; i++) {
        Type* expected = fn->params->data[i];
        Type* actual = checker_infer_expr(checker, expr->args->data[i].value);
        
        if (actual->kind == TYPE_ERROR) return actual;
        
        if (!type_assignable(actual, expected)) {
            return error_type(checker, "Argument %zu: expected %s, got %s",
                i + 1,
                string_cstr(type_to_string(checker->arena, expected)),
                string_cstr(type_to_string(checker->arena, actual)));
        }
    }
    
    return fn->result;
}

/* ========== If Expression Type Checking ========== */

static Type* check_if_expr(Checker* checker, IfExpr* expr) {
    /* Check condition is Bool */
    Type* cond_type = checker_infer_expr(checker, expr->condition);
    if (cond_type->kind == TYPE_ERROR) return cond_type;
    
    if (cond_type->kind != TYPE_BOOL) {
        return error_type(checker, "If condition must be Bool, got %s",
            string_cstr(type_to_string(checker->arena, cond_type)));
    }
    
    /* Check then branch */
    Type* then_type = checker_infer_expr(checker, expr->then_branch);
    if (then_type->kind == TYPE_ERROR) return then_type;
    
    /* If no else branch, the whole expression returns Unit */
    if (expr->else_branch == NULL) {
        return type_unit(checker->arena);
    }
    
    /* Check else branch */
    Type* else_type = checker_infer_expr(checker, expr->else_branch);
    if (else_type->kind == TYPE_ERROR) return else_type;
    
    /* Both branches must have the same type */
    if (!type_equals(then_type, else_type)) {
        return error_type(checker, "If branches have different types: %s vs %s",
            string_cstr(type_to_string(checker->arena, then_type)),
            string_cstr(type_to_string(checker->arena, else_type)));
    }
    
    return then_type;
}

/* ========== Block Expression Type Checking ========== */

static Type* check_block_expr(Checker* checker, BlockExpr* expr) {
    /* Push a new scope for the block */
    checker_push_scope(checker);
    
    /* Type check all statements in the block */
    for (size_t i = 0; i < expr->stmts->len; i++) {
        if (!checker_check_stmt(checker, expr->stmts->data[i])) {
            checker_pop_scope(checker);
            return type_error(checker->arena, 
                string_new(checker->arena, "Statement type check failed"));
        }
    }
    
    /* The block's type is the type of the final expression, or Unit if none */
    Type* result;
    if (expr->final_expr != NULL) {
        result = checker_infer_expr(checker, expr->final_expr);
    } else {
        result = type_unit(checker->arena);
    }
    
    checker_pop_scope(checker);
    return result;
}

/* ========== Main Type Inference ========== */

Type* checker_infer_expr(Checker* checker, Expr* expr) {
    switch (expr->type) {
        case EXPR_INT_LIT:
            return type_int(checker->arena);
            
        case EXPR_FLOAT_LIT:
            return type_float(checker->arena);
            
        case EXPR_STRING_LIT:
            return type_string(checker->arena);
            
        case EXPR_BOOL_LIT:
            return type_bool(checker->arena);
            
        case EXPR_IDENT: {
            Type* t = type_env_lookup(checker->env, expr->data.ident.name);
            if (!t) {
                return error_type(checker, "Undefined variable: %s",
                    string_cstr(expr->data.ident.name));
            }
            return t;
        }
            
        case EXPR_BINARY:
            return check_binary_expr(checker, &expr->data.binary);
            
        case EXPR_UNARY:
            return check_unary_expr(checker, &expr->data.unary);
            
        case EXPR_LIST:
            return check_list_expr(checker, &expr->data.list);
            
        case EXPR_TUPLE:
            return check_tuple_expr(checker, &expr->data.tuple);
            
        case EXPR_CALL:
            return check_call_expr(checker, &expr->data.call);
            
        case EXPR_IF:
            return check_if_expr(checker, &expr->data.if_expr);
            
        case EXPR_BLOCK:
            return check_block_expr(checker, &expr->data.block);
            
        /* TODO: Implement remaining expression types */
        case EXPR_MATCH:
        case EXPR_BIND:
        case EXPR_WITH:
        case EXPR_DOT:
        case EXPR_RANGE:
        case EXPR_FOR:
        case EXPR_LAMBDA:
        case EXPR_INTERP_STRING:
        case EXPR_MAP:
        case EXPR_RECORD_UPDATE:
        case EXPR_LIST_COMP:
        case EXPR_INDEX:
        case EXPR_SPAWN:
        case EXPR_SEND:
        case EXPR_RECEIVE:
        case EXPR_TRY:
            return error_type(checker, "Type checking not yet implemented for this expression type");
    }
    
    return error_type(checker, "Unknown expression type");
}

/* ========== Statement Type Checking ========== */

bool checker_check_stmt(Checker* checker, Stmt* stmt) {
    (void)checker;  /* TODO: Implement */
    (void)stmt;
    return true;
}

bool checker_check_stmts(Checker* checker, StmtVec* stmts) {
    for (size_t i = 0; i < stmts->len; i++) {
        if (!checker_check_stmt(checker, stmts->data[i])) {
            return false;
        }
    }
    return true;
}

/* ========== Error Handling ========== */

bool checker_has_errors(Checker* checker) {
    return checker->errors != NULL;
}

const char* checker_first_error(Checker* checker) {
    if (!checker->errors) return NULL;
    return string_cstr(checker->errors->message);
}

/* ========== Environment Access ========== */

TypeEnv* checker_env(Checker* checker) {
    return checker->env;
}

void checker_define(Checker* checker, String* name, Type* type) {
    type_env_define(checker->env, name, type);
}

void checker_push_scope(Checker* checker) {
    type_env_push_scope(checker->env);
}

void checker_pop_scope(Checker* checker) {
    type_env_pop_scope(checker->env);
}
