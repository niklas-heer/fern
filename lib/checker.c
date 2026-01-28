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

/* ========== Forward Declarations ========== */

static bool bind_pattern(Checker* checker, Pattern* pattern, Type* type);

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

/* ========== Match Expression Type Checking ========== */

static Type* check_match_expr(Checker* checker, MatchExpr* expr) {
    /* Infer type of the scrutinee */
    Type* scrutinee_type = checker_infer_expr(checker, expr->value);
    if (scrutinee_type->kind == TYPE_ERROR) return scrutinee_type;
    
    if (expr->arms->len == 0) {
        return error_type(checker, "Match expression must have at least one arm");
    }
    
    /* Type check the first arm to get the expected result type */
    MatchArm first_arm = expr->arms->data[0];
    
    /* Push scope for pattern bindings */
    checker_push_scope(checker);
    bind_pattern(checker, first_arm.pattern, scrutinee_type);
    
    /* Check guard if present */
    if (first_arm.guard) {
        Type* guard_type = checker_infer_expr(checker, first_arm.guard);
        if (guard_type->kind == TYPE_ERROR) {
            checker_pop_scope(checker);
            return guard_type;
        }
        if (guard_type->kind != TYPE_BOOL) {
            checker_pop_scope(checker);
            return error_type(checker, "Match guard must be Bool, got %s",
                string_cstr(type_to_string(checker->arena, guard_type)));
        }
    }
    
    Type* result_type = checker_infer_expr(checker, first_arm.body);
    checker_pop_scope(checker);
    
    if (result_type->kind == TYPE_ERROR) return result_type;
    
    /* Check remaining arms match the result type */
    for (size_t i = 1; i < expr->arms->len; i++) {
        MatchArm arm = expr->arms->data[i];
        
        /* Push scope for pattern bindings */
        checker_push_scope(checker);
        bind_pattern(checker, arm.pattern, scrutinee_type);
        
        /* Check guard if present */
        if (arm.guard) {
            Type* guard_type = checker_infer_expr(checker, arm.guard);
            if (guard_type->kind == TYPE_ERROR) {
                checker_pop_scope(checker);
                return guard_type;
            }
            if (guard_type->kind != TYPE_BOOL) {
                checker_pop_scope(checker);
                return error_type(checker, "Match guard must be Bool, got %s",
                    string_cstr(type_to_string(checker->arena, guard_type)));
            }
        }
        
        Type* arm_type = checker_infer_expr(checker, arm.body);
        checker_pop_scope(checker);
        
        if (arm_type->kind == TYPE_ERROR) return arm_type;
        
        if (!type_equals(arm_type, result_type)) {
            return error_type(checker, "Match arm types must be equal: expected %s, got %s",
                string_cstr(type_to_string(checker->arena, result_type)),
                string_cstr(type_to_string(checker->arena, arm_type)));
        }
    }
    
    return result_type;
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
            
        case EXPR_MATCH:
            return check_match_expr(checker, &expr->data.match_expr);
            
        /* TODO: Implement remaining expression types */
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

/* ========== Type Expression Resolution ========== */

/* Convert a TypeExpr (AST type annotation) to a Type */
static Type* resolve_type_expr(Checker* checker, TypeExpr* type_expr) {
    if (!type_expr) return NULL;
    
    switch (type_expr->kind) {
        case TYPE_NAMED: {
            String* name = type_expr->data.named.name;
            const char* name_str = string_cstr(name);
            
            /* Check for built-in primitive types */
            if (strcmp(name_str, "Int") == 0) {
                return type_int(checker->arena);
            }
            if (strcmp(name_str, "Float") == 0) {
                return type_float(checker->arena);
            }
            if (strcmp(name_str, "String") == 0) {
                return type_string(checker->arena);
            }
            if (strcmp(name_str, "Bool") == 0) {
                return type_bool(checker->arena);
            }
            
            /* Check for parameterized types */
            TypeExprVec* args = type_expr->data.named.args;
            if (args && args->len > 0) {
                /* Resolve type arguments */
                TypeVec* resolved_args = TypeVec_new(checker->arena);
                for (size_t i = 0; i < args->len; i++) {
                    Type* arg = resolve_type_expr(checker, args->data[i]);
                    if (!arg || arg->kind == TYPE_ERROR) return arg;
                    TypeVec_push(checker->arena, resolved_args, arg);
                }
                
                /* Common parameterized types */
                if (strcmp(name_str, "List") == 0 && resolved_args->len == 1) {
                    return type_list(checker->arena, resolved_args->data[0]);
                }
                if (strcmp(name_str, "Option") == 0 && resolved_args->len == 1) {
                    return type_option(checker->arena, resolved_args->data[0]);
                }
                if (strcmp(name_str, "Result") == 0 && resolved_args->len == 2) {
                    return type_result(checker->arena, 
                        resolved_args->data[0], resolved_args->data[1]);
                }
                if (strcmp(name_str, "Map") == 0 && resolved_args->len == 2) {
                    return type_map(checker->arena,
                        resolved_args->data[0], resolved_args->data[1]);
                }
                
                /* User-defined parameterized type */
                return type_con(checker->arena, name, resolved_args);
            }
            
            /* Look up in type environment */
            Type* defined = type_env_lookup_type(checker->env, name);
            if (defined) {
                return defined;
            }
            
            /* Unknown type - treat as a simple type constructor */
            return type_con(checker->arena, name, NULL);
        }
        
        case TYPE_FUNCTION: {
            /* Resolve parameter types */
            TypeVec* params = TypeVec_new(checker->arena);
            for (size_t i = 0; i < type_expr->data.func.params->len; i++) {
                Type* param = resolve_type_expr(checker, 
                    type_expr->data.func.params->data[i]);
                if (!param || param->kind == TYPE_ERROR) return param;
                TypeVec_push(checker->arena, params, param);
            }
            
            /* Resolve return type */
            Type* ret = resolve_type_expr(checker, type_expr->data.func.return_type);
            if (!ret || ret->kind == TYPE_ERROR) return ret;
            
            return type_fn(checker->arena, params, ret);
        }
    }
    
    return error_type(checker, "Unknown type expression kind");
}

/* ========== Statement Type Checking ========== */

/* Bind a pattern to a type in the environment */
static bool bind_pattern(Checker* checker, Pattern* pattern, Type* type) {
    switch (pattern->type) {
        case PATTERN_IDENT:
            checker_define(checker, pattern->data.ident, type);
            return true;
            
        case PATTERN_WILDCARD:
            /* _ doesn't bind anything */
            return true;
            
        case PATTERN_LIT:
            /* Literal patterns don't bind */
            return true;
            
        case PATTERN_TUPLE: {
            /* Type must be a tuple with matching arity */
            if (type->kind != TYPE_TUPLE) {
                add_error(checker, "Cannot destructure non-tuple type %s",
                    string_cstr(type_to_string(checker->arena, type)));
                return false;
            }
            PatternVec* sub = pattern->data.tuple;
            TypeVec* elem_types = type->data.tuple.elements;
            if (sub->len != elem_types->len) {
                add_error(checker, "Tuple pattern has %zu elements but type has %zu",
                    sub->len, elem_types->len);
                return false;
            }
            for (size_t i = 0; i < sub->len; i++) {
                if (!bind_pattern(checker, sub->data[i], elem_types->data[i])) {
                    return false;
                }
            }
            return true;
        }
            
        case PATTERN_CONSTRUCTOR:
        case PATTERN_REST:
            /* TODO: Implement constructor pattern binding */
            return true;
    }
    
    return false;
}

static bool check_let_stmt(Checker* checker, LetStmt* stmt) {
    /* Infer type of the value expression */
    Type* value_type = checker_infer_expr(checker, stmt->value);
    if (value_type->kind == TYPE_ERROR) {
        return false;
    }
    
    /* If there's a type annotation, resolve it and check compatibility */
    if (stmt->type_ann) {
        Type* annotated = resolve_type_expr(checker, stmt->type_ann);
        if (!annotated || annotated->kind == TYPE_ERROR) {
            return false;
        }
        
        if (!type_assignable(value_type, annotated)) {
            add_error(checker, "Type mismatch: expected %s, got %s",
                string_cstr(type_to_string(checker->arena, annotated)),
                string_cstr(type_to_string(checker->arena, value_type)));
            return false;
        }
        
        /* Use the annotated type for the binding */
        value_type = annotated;
    }
    
    /* Bind the pattern to the inferred/annotated type */
    return bind_pattern(checker, stmt->pattern, value_type);
}

bool checker_check_stmt(Checker* checker, Stmt* stmt) {
    switch (stmt->type) {
        case STMT_LET:
            return check_let_stmt(checker, &stmt->data.let);
            
        case STMT_EXPR:
            /* Type check the expression but discard the result */
            checker_infer_expr(checker, stmt->data.expr.expr);
            return !checker_has_errors(checker);
            
        case STMT_RETURN:
        case STMT_FN:
        case STMT_IMPORT:
        case STMT_DEFER:
        case STMT_TYPE_DEF:
        case STMT_BREAK:
        case STMT_CONTINUE:
        case STMT_TRAIT:
        case STMT_IMPL:
        case STMT_NEWTYPE:
        case STMT_MODULE:
            /* TODO: Implement other statement types */
            return true;
    }
    
    return false;
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
