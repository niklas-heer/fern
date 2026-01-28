/* Fern Type Checker Implementation */

#include "checker.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
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
static bool unify(Type* a, Type* b);
static Type* substitute(Arena* arena, Type* type);
static Type* check_pipe_expr(Checker* checker, BinaryExpr* expr);

/* Map from old var id to new type variable */
typedef struct VarMapping {
    int old_id;
    Type* new_var;
    struct VarMapping* next;
} VarMapping;

static Type* instantiate_type(Arena* arena, Type* type, VarMapping** map);

/* ========== Helper Functions ========== */

static void add_error(Checker* checker, const char* fmt, ...) {
    assert(checker != NULL);
    assert(fmt != NULL);
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
    assert(checker != NULL);
    assert(fmt != NULL);
    va_list args;
    va_start(args, fmt);
    
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    
    add_error(checker, "%s", buf);
    return type_error(checker->arena, string_new(checker->arena, buf));
}

/* Error with source location */
static Type* error_type_at(Checker* checker, SourceLoc loc, const char* fmt, ...) {
    assert(checker != NULL);
    assert(fmt != NULL);
    va_list args;
    va_start(args, fmt);
    
    char msg_buf[1024];
    vsnprintf(msg_buf, sizeof(msg_buf), fmt, args);
    va_end(args);
    
    /* Format with location: "file:line:col: error message" */
    char full_buf[1280];
    if (loc.filename && loc.line > 0) {
        snprintf(full_buf, sizeof(full_buf), "%s:%zu:%zu: %s",
            string_cstr(loc.filename), loc.line, loc.column, msg_buf);
    } else if (loc.line > 0) {
        snprintf(full_buf, sizeof(full_buf), "%zu:%zu: %s",
            loc.line, loc.column, msg_buf);
    } else {
        snprintf(full_buf, sizeof(full_buf), "%s", msg_buf);
    }
    
    add_error(checker, "%s", full_buf);
    return type_error(checker->arena, string_new(checker->arena, full_buf));
}

/* ========== Checker Creation ========== */

Checker* checker_new(Arena* arena) {
    assert(arena != NULL);
    Checker* checker = arena_alloc(arena, sizeof(Checker));
    assert(checker != NULL);
    checker->arena = arena;
    checker->env = type_env_new(arena);
    checker->errors = NULL;
    checker->errors_tail = NULL;
    return checker;
}

/* ========== Binary Operator Type Checking ========== */

static Type* check_arithmetic_op(Checker* checker, Type* left, Type* right, 
                                  const char* op_name) {
    /* Handle type variables through unification */
    /* Follow bound type variables */
    while (left->kind == TYPE_VAR && left->data.var.bound) {
        left = left->data.var.bound;
    }
    while (right->kind == TYPE_VAR && right->data.var.bound) {
        right = right->data.var.bound;
    }
    
    /* If one side is a type variable, try to unify with the other */
    if (left->kind == TYPE_VAR && !left->data.var.bound) {
        if (right->kind == TYPE_INT || right->kind == TYPE_FLOAT) {
            left->data.var.bound = right;
            return right;
        }
    }
    if (right->kind == TYPE_VAR && !right->data.var.bound) {
        if (left->kind == TYPE_INT || left->kind == TYPE_FLOAT) {
            right->data.var.bound = left;
            return left;
        }
    }
    
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

/* ========== Pipe Operator Type Checking ========== */

static Type* check_pipe_expr(Checker* checker, BinaryExpr* expr) {
    assert(checker != NULL);
    assert(expr != NULL);
    /* Pipe: left |> right
     * 
     * The pipe operator passes the left value as the first argument to
     * the function call on the right. For example:
     *   x |> f(a, b)  is equivalent to  f(x, a, b)
     *   x |> f()      is equivalent to  f(x)
     *
     * The right side must be a call expression. We type-check it specially
     * by injecting the left value as an additional first argument.
     */
    
    /* Type check the left side first */
    Type* left_type = checker_infer_expr(checker, expr->left);
    if (left_type->kind == TYPE_ERROR) return left_type;
    
    /* Right side must be a call expression */
    Expr* right = expr->right;
    if (right->type != EXPR_CALL) {
        return error_type(checker, "Pipe target must be a function call");
    }
    
    CallExpr* call = &right->data.call;
    
    /* Get the function being called */
    Type* callee_type = checker_infer_expr(checker, call->func);
    if (callee_type->kind == TYPE_ERROR) return callee_type;
    
    /* Must be a function type */
    if (callee_type->kind != TYPE_FN) {
        return error_type(checker, "Cannot call non-function type %s",
            string_cstr(type_to_string(checker->arena, callee_type)));
    }
    
    /* Instantiate the function type with fresh type variables */
    VarMapping* var_map = NULL;
    Type* fn_type_inst = instantiate_type(checker->arena, callee_type, &var_map);
    TypeFn* fn = &fn_type_inst->data.fn;
    
    /* The total number of arguments is: 1 (piped) + explicit args */
    size_t total_args = 1 + call->args->len;
    size_t expected_count = fn->params->len;
    
    if (total_args != expected_count) {
        return error_type(checker, "Expected %zu arguments, got %zu (including piped value)",
            expected_count, total_args);
    }
    
    /* Unify the piped value (left) with the first parameter */
    Type* first_param = fn->params->data[0];
    if (!unify(first_param, left_type)) {
        return error_type(checker, "Pipe: cannot pass %s to function expecting %s",
            string_cstr(type_to_string(checker->arena, left_type)),
            string_cstr(type_to_string(checker->arena, first_param)));
    }
    
    /* Unify the explicit arguments with the remaining parameters */
    for (size_t i = 0; i < call->args->len; i++) {
        Type* expected = fn->params->data[i + 1]; /* +1 because first param is piped */
        Type* actual = checker_infer_expr(checker, call->args->data[i].value);
        
        if (actual->kind == TYPE_ERROR) return actual;
        
        if (!unify(expected, actual)) {
            return error_type(checker, "Argument %zu: expected %s, got %s",
                i + 2, /* +2 because arg 1 is piped, user sees 2-based */
                string_cstr(type_to_string(checker->arena, expected)),
                string_cstr(type_to_string(checker->arena, actual)));
        }
    }
    
    /* Return the result type with substitutions applied */
    return substitute(checker->arena, fn->result);
}

static Type* check_binary_expr(Checker* checker, BinaryExpr* expr) {
    assert(checker != NULL);
    assert(expr != NULL);
    /* Pipe operator needs special handling - don't type check right side yet */
    if (expr->op == BINOP_PIPE) {
        return check_pipe_expr(checker, expr);
    }
    
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
            /* Handled above by check_pipe_expr */
            return right; /* Should not reach here */
    }
    
    return error_type(checker, "Unknown binary operator");
}

/* ========== Unary Operator Type Checking ========== */

static Type* check_unary_expr(Checker* checker, UnaryExpr* expr) {
    assert(checker != NULL);
    assert(expr != NULL);
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
    assert(checker != NULL);
    assert(expr != NULL);
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
    assert(checker != NULL);
    assert(expr != NULL);
    TypeVec* elem_types = TypeVec_new(checker->arena);
    
    for (size_t i = 0; i < expr->elements->len; i++) {
        Type* t = checker_infer_expr(checker, expr->elements->data[i]);
        if (t->kind == TYPE_ERROR) return t;
        TypeVec_push(checker->arena, elem_types, t);
    }
    
    return type_tuple(checker->arena, elem_types);
}

/* ========== Type Unification ========== */

/* Check if a type contains a specific type variable */
static bool type_contains_var(Type* type, int var_id) {
    assert(var_id >= 0);
    if (!type) return false;
    assert(type->kind >= TYPE_INT && type->kind <= TYPE_ERROR);
    
    switch (type->kind) {
        case TYPE_VAR:
            if (type->data.var.bound) {
                return type_contains_var(type->data.var.bound, var_id);
            }
            return type->data.var.id == var_id;
            
        case TYPE_CON:
            if (type->data.con.args) {
                for (size_t i = 0; i < type->data.con.args->len; i++) {
                    if (type_contains_var(type->data.con.args->data[i], var_id)) {
                        return true;
                    }
                }
            }
            return false;
            
        case TYPE_FN:
            for (size_t i = 0; i < type->data.fn.params->len; i++) {
                if (type_contains_var(type->data.fn.params->data[i], var_id)) {
                    return true;
                }
            }
            return type_contains_var(type->data.fn.result, var_id);
            
        case TYPE_TUPLE:
            for (size_t i = 0; i < type->data.tuple.elements->len; i++) {
                if (type_contains_var(type->data.tuple.elements->data[i], var_id)) {
                    return true;
                }
            }
            return false;
            
        default:
            return false;
    }
}

/* Unify two types, binding type variables as needed
 * Returns true if unification succeeded */
static bool unify(Type* a, Type* b) {
    /* NULL types are equal only to each other. */
    if (!a || !b) return a == b;
    assert(a->kind >= TYPE_INT && a->kind <= TYPE_ERROR);
    assert(b->kind >= TYPE_INT && b->kind <= TYPE_ERROR);
    
    /* Follow bound type variables */
    while (a->kind == TYPE_VAR && a->data.var.bound) {
        a = a->data.var.bound;
    }
    while (b->kind == TYPE_VAR && b->data.var.bound) {
        b = b->data.var.bound;
    }
    
    /* Same type variable */
    if (a->kind == TYPE_VAR && b->kind == TYPE_VAR && 
        a->data.var.id == b->data.var.id) {
        return true;
    }
    
    /* Bind unbound type variable to concrete type */
    if (a->kind == TYPE_VAR && !a->data.var.bound) {
        /* Occurs check: can't unify a with something containing a */
        if (type_contains_var(b, a->data.var.id)) {
            return false;
        }
        a->data.var.bound = b;
        return true;
    }
    
    if (b->kind == TYPE_VAR && !b->data.var.bound) {
        if (type_contains_var(a, b->data.var.id)) {
            return false;
        }
        b->data.var.bound = a;
        return true;
    }
    
    /* Different kinds */
    if (a->kind != b->kind) {
        return false;
    }
    
    /* Same kind - compare structurally */
    switch (a->kind) {
        case TYPE_INT:
        case TYPE_FLOAT:
        case TYPE_STRING:
        case TYPE_BOOL:
        case TYPE_UNIT:
        case TYPE_ERROR:
            return true;
            
        case TYPE_CON:
            if (!string_equal(a->data.con.name, b->data.con.name)) {
                return false;
            }
            if (!a->data.con.args && !b->data.con.args) return true;
            if (!a->data.con.args || !b->data.con.args) return false;
            if (a->data.con.args->len != b->data.con.args->len) return false;
            for (size_t i = 0; i < a->data.con.args->len; i++) {
                if (!unify(a->data.con.args->data[i], b->data.con.args->data[i])) {
                    return false;
                }
            }
            return true;
            
        case TYPE_FN:
            if (a->data.fn.params->len != b->data.fn.params->len) return false;
            for (size_t i = 0; i < a->data.fn.params->len; i++) {
                if (!unify(a->data.fn.params->data[i], b->data.fn.params->data[i])) {
                    return false;
                }
            }
            return unify(a->data.fn.result, b->data.fn.result);
            
        case TYPE_TUPLE:
            if (a->data.tuple.elements->len != b->data.tuple.elements->len) return false;
            for (size_t i = 0; i < a->data.tuple.elements->len; i++) {
                if (!unify(a->data.tuple.elements->data[i], b->data.tuple.elements->data[i])) {
                    return false;
                }
            }
            return true;
            
        case TYPE_VAR:
            /* Already handled above */
            return false;
    }
    
    return false;
}

/* Substitute bound type variables with their concrete types */
static Type* substitute(Arena* arena, Type* type) {
    assert(arena != NULL);
    if (!type) return NULL;
    assert(type->kind >= TYPE_INT && type->kind <= TYPE_ERROR);
    
    switch (type->kind) {
        case TYPE_VAR:
            if (type->data.var.bound) {
                return substitute(arena, type->data.var.bound);
            }
            return type;
            
        case TYPE_CON: {
            if (!type->data.con.args || type->data.con.args->len == 0) {
                return type;
            }
            TypeVec* new_args = TypeVec_new(arena);
            for (size_t i = 0; i < type->data.con.args->len; i++) {
                TypeVec_push(arena, new_args, 
                    substitute(arena, type->data.con.args->data[i]));
            }
            return type_con(arena, type->data.con.name, new_args);
        }
            
        case TYPE_FN: {
            TypeVec* new_params = TypeVec_new(arena);
            for (size_t i = 0; i < type->data.fn.params->len; i++) {
                TypeVec_push(arena, new_params,
                    substitute(arena, type->data.fn.params->data[i]));
            }
            return type_fn(arena, new_params, 
                substitute(arena, type->data.fn.result));
        }
            
        case TYPE_TUPLE: {
            TypeVec* new_elements = TypeVec_new(arena);
            for (size_t i = 0; i < type->data.tuple.elements->len; i++) {
                TypeVec_push(arena, new_elements,
                    substitute(arena, type->data.tuple.elements->data[i]));
            }
            return type_tuple(arena, new_elements);
        }
            
        default:
            return type;
    }
}

/* ========== Type Instantiation ========== */

/* Find a mapping for a type variable ID */
static Type* find_var_mapping(VarMapping* map, int var_id) {
    assert(var_id >= 0);
    /* map may be NULL on first call - that's valid. */
    for (VarMapping* m = map; m != NULL; m = m->next) {
        assert(m->new_var != NULL);
        if (m->old_id == var_id) {
            return m->new_var;
        }
    }
    return NULL;
}

/* Instantiate a type, replacing type variables with fresh ones.
 * This maintains sharing: if the same type variable appears multiple times,
 * it will be replaced with the same fresh variable each time. */
static Type* instantiate_type(Arena* arena, Type* type, VarMapping** map) {
    assert(arena != NULL);
    assert(map != NULL);
    if (!type) return NULL;
    
    switch (type->kind) {
        case TYPE_VAR: {
            /* Check if we already have a mapping for this variable */
            Type* existing = find_var_mapping(*map, type->data.var.id);
            if (existing) {
                return existing;
            }
            
            /* Create a fresh type variable with a new ID */
            int new_id = type_fresh_var_id();
            Type* fresh = type_var(arena, type->data.var.name, new_id);
            
            /* Add to mapping */
            VarMapping* new_map = arena_alloc(arena, sizeof(VarMapping));
            new_map->old_id = type->data.var.id;
            new_map->new_var = fresh;
            new_map->next = *map;
            *map = new_map;
            
            return fresh;
        }
            
        case TYPE_CON: {
            if (!type->data.con.args || type->data.con.args->len == 0) {
                return type;
            }
            TypeVec* new_args = TypeVec_new(arena);
            for (size_t i = 0; i < type->data.con.args->len; i++) {
                TypeVec_push(arena, new_args, 
                    instantiate_type(arena, type->data.con.args->data[i], map));
            }
            return type_con(arena, type->data.con.name, new_args);
        }
            
        case TYPE_FN: {
            TypeVec* new_params = TypeVec_new(arena);
            for (size_t i = 0; i < type->data.fn.params->len; i++) {
                TypeVec_push(arena, new_params,
                    instantiate_type(arena, type->data.fn.params->data[i], map));
            }
            return type_fn(arena, new_params, 
                instantiate_type(arena, type->data.fn.result, map));
        }
            
        case TYPE_TUPLE: {
            TypeVec* new_elements = TypeVec_new(arena);
            for (size_t i = 0; i < type->data.tuple.elements->len; i++) {
                TypeVec_push(arena, new_elements,
                    instantiate_type(arena, type->data.tuple.elements->data[i], map));
            }
            return type_tuple(arena, new_elements);
        }
            
        default:
            return type;
    }
}

/* ========== Function Call Type Checking ========== */

static Type* check_call_expr(Checker* checker, CallExpr* expr) {
    assert(checker != NULL);
    assert(expr != NULL);
    /* Infer the type of the callee */
    Type* callee_type = checker_infer_expr(checker, expr->func);
    if (callee_type->kind == TYPE_ERROR) return callee_type;
    
    /* Callee must be a function type */
    if (callee_type->kind != TYPE_FN) {
        return error_type(checker, "Cannot call non-function type %s",
            string_cstr(type_to_string(checker->arena, callee_type)));
    }
    
    /* Instantiate the function type with fresh type variables.
     * This ensures that multiple calls to the same generic function
     * don't interfere with each other, and that type variables that
     * appear multiple times (like in (a) -> a) share the same binding. */
    VarMapping* var_map = NULL;
    Type* fn_type_inst = instantiate_type(checker->arena, callee_type, &var_map);
    TypeFn* fn = &fn_type_inst->data.fn;
    
    /* Check argument count */
    size_t expected_count = fn->params->len;
    size_t actual_count = expr->args->len;
    
    if (actual_count != expected_count) {
        return error_type(checker, "Expected %zu arguments, got %zu",
            expected_count, actual_count);
    }
    
    /* Unify argument types with parameter types (handles generics) */
    for (size_t i = 0; i < actual_count; i++) {
        Type* expected = fn->params->data[i];
        Type* actual = checker_infer_expr(checker, expr->args->data[i].value);
        
        if (actual->kind == TYPE_ERROR) return actual;
        
        if (!unify(expected, actual)) {
            return error_type(checker, "Argument %zu: expected %s, got %s",
                i + 1,
                string_cstr(type_to_string(checker->arena, expected)),
                string_cstr(type_to_string(checker->arena, actual)));
        }
    }
    
    /* Substitute bound type variables in the result type */
    return substitute(checker->arena, fn->result);
}

/* ========== If Expression Type Checking ========== */

static Type* check_if_expr(Checker* checker, IfExpr* expr) {
    assert(checker != NULL);
    assert(expr != NULL);
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
    assert(checker != NULL);
    assert(expr != NULL);
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
    assert(checker != NULL);
    assert(expr != NULL);
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
    assert(checker != NULL);
    assert(expr != NULL);
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
                return error_type_at(checker, expr->loc, "Undefined variable: %s",
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
            
        case EXPR_BIND: {
            /* Bind expression: name <- value
             * Value must be Result(ok, err), binds name to ok type.
             * The expression itself returns the ok type (for use in blocks).
             */
            BindExpr* bind = &expr->data.bind;
            Type* value_type = checker_infer_expr(checker, bind->value);
            if (value_type->kind == TYPE_ERROR) return value_type;
            
            /* Value must be a Result type */
            if (!type_is_result(value_type)) {
                return error_type_at(checker, expr->loc, "The <- operator requires a Result type, got %s",
                    string_cstr(type_to_string(checker->arena, value_type)));
            }
            
            /* Extract the Ok type (first type argument of Result) */
            Type* ok_type = value_type->data.con.args->data[0];
            
            /* Bind the name to the Ok type in the environment */
            checker_define(checker, bind->name, ok_type);
            
            return ok_type;
        }
            
        case EXPR_WITH: {
            /* With expression: with bindings do body [else arms]
             * Each binding must be Result type, binds name to ok type.
             * Body is evaluated with all bindings in scope.
             */
            WithExpr* with = &expr->data.with_expr;
            
            /* Push a new scope for the bindings */
            checker_push_scope(checker);
            
            /* Process each binding */
            for (size_t i = 0; i < with->bindings->len; i++) {
                WithBinding* binding = &with->bindings->data[i];
                Type* value_type = checker_infer_expr(checker, binding->value);
                
                if (value_type->kind == TYPE_ERROR) {
                    checker_pop_scope(checker);
                    return value_type;
                }
                
                /* Value must be a Result type */
                if (!type_is_result(value_type)) {
                    checker_pop_scope(checker);
                    return error_type(checker, "with binding requires Result type, got %s",
                        string_cstr(type_to_string(checker->arena, value_type)));
                }
                
                /* Extract Ok type and bind the name */
                Type* ok_type = value_type->data.con.args->data[0];
                checker_define(checker, binding->name, ok_type);
            }
            
            /* Type check the body with bindings in scope */
            Type* body_type = checker_infer_expr(checker, with->body);
            
            checker_pop_scope(checker);
            
            if (body_type->kind == TYPE_ERROR) return body_type;
            
            /* TODO: Type check else arms if present */
            
            return body_type;
        }
            
        case EXPR_LAMBDA: {
            /* Lambda expression: (params) -> body
             * Creates a function type with fresh type variables for params.
             */
            LambdaExpr* lambda = &expr->data.lambda;
            
            /* Push a new scope for lambda parameters */
            checker_push_scope(checker);
            
            /* Create fresh type variables for each parameter */
            TypeVec* param_types = TypeVec_new(checker->arena);
            for (size_t i = 0; i < lambda->params->len; i++) {
                int var_id = type_fresh_var_id();
                Type* param_type = type_var(checker->arena, lambda->params->data[i], var_id);
                TypeVec_push(checker->arena, param_types, param_type);
                checker_define(checker, lambda->params->data[i], param_type);
            }
            
            /* Infer the body type with parameters in scope */
            Type* body_type = checker_infer_expr(checker, lambda->body);
            
            checker_pop_scope(checker);
            
            if (body_type->kind == TYPE_ERROR) return body_type;
            
            /* Create and return function type */
            return type_fn(checker->arena, param_types, body_type);
        }
            
        case EXPR_FOR: {
            /* For loop: for var in iterable: body
             * Iterable must be a List, var is bound to element type.
             * For loop returns Unit (side-effect only).
             */
            ForExpr* for_loop = &expr->data.for_loop;
            
            /* Infer type of iterable */
            Type* iter_type = checker_infer_expr(checker, for_loop->iterable);
            if (iter_type->kind == TYPE_ERROR) return iter_type;
            
            /* Iterable must be a List (for now) */
            if (iter_type->kind != TYPE_CON || 
                strcmp(string_cstr(iter_type->data.con.name), "List") != 0) {
                return error_type_at(checker, expr->loc, "for loop requires List, got %s",
                    string_cstr(type_to_string(checker->arena, iter_type)));
            }
            
            /* Extract element type from List(elem) */
            Type* elem_type = iter_type->data.con.args->data[0];
            
            /* Push scope and bind loop variable */
            checker_push_scope(checker);
            checker_define(checker, for_loop->var_name, elem_type);
            
            /* Type check body (result is discarded) */
            Type* body_type = checker_infer_expr(checker, for_loop->body);
            (void)body_type; /* Body type is not used */
            
            checker_pop_scope(checker);
            
            /* For loop returns Unit */
            return type_unit(checker->arena);
        }
            
        case EXPR_INDEX: {
            /* Index expression: object[index]
             * For List, index must be Int, returns element type.
             * For Map, index must be key type, returns value type.
             */
            IndexExpr* index = &expr->data.index_expr;
            
            Type* obj_type = checker_infer_expr(checker, index->object);
            if (obj_type->kind == TYPE_ERROR) return obj_type;
            
            Type* idx_type = checker_infer_expr(checker, index->index);
            if (idx_type->kind == TYPE_ERROR) return idx_type;
            
            /* Check if indexing a List */
            if (obj_type->kind == TYPE_CON && 
                strcmp(string_cstr(obj_type->data.con.name), "List") == 0) {
                /* Index must be Int */
                if (idx_type->kind != TYPE_INT) {
                    return error_type_at(checker, expr->loc, "List index must be Int, got %s",
                        string_cstr(type_to_string(checker->arena, idx_type)));
                }
                /* Return element type */
                return obj_type->data.con.args->data[0];
            }
            
            /* Check if indexing a Map */
            if (obj_type->kind == TYPE_CON &&
                strcmp(string_cstr(obj_type->data.con.name), "Map") == 0) {
                /* Index must match key type */
                Type* key_type = obj_type->data.con.args->data[0];
                if (!type_equals(idx_type, key_type)) {
                    return error_type_at(checker, expr->loc, "Map key must be %s, got %s",
                        string_cstr(type_to_string(checker->arena, key_type)),
                        string_cstr(type_to_string(checker->arena, idx_type)));
                }
                /* Return value type */
                return obj_type->data.con.args->data[1];
            }
            
            return error_type_at(checker, expr->loc, "Cannot index type %s",
                string_cstr(type_to_string(checker->arena, obj_type)));
        }
            
        case EXPR_DOT: {
            /* Dot expression: object.field
             * For tuples: returns the field type at the given index
             * For records: would look up field type (not yet implemented)
             */
            DotExpr* dot = &expr->data.dot;
            Type* obj_type = checker_infer_expr(checker, dot->object);
            if (obj_type->kind == TYPE_ERROR) return obj_type;
            
            /* Tuple field access: tuple.0, tuple.1, etc. */
            if (obj_type->kind == TYPE_TUPLE) {
                const char* field_str = string_cstr(dot->field);
                /* Parse field as index */
                char* endptr;
                long index = strtol(field_str, &endptr, 10);
                if (*endptr != '\0' || index < 0) {
                    return error_type_at(checker, expr->loc, 
                        "Invalid tuple field: %s (expected numeric index)", field_str);
                }
                
                size_t idx = (size_t)index;
                if (idx >= obj_type->data.tuple.elements->len) {
                    return error_type_at(checker, expr->loc,
                        "Tuple index %zu out of bounds (tuple has %zu elements)",
                        idx, obj_type->data.tuple.elements->len);
                }
                
                return obj_type->data.tuple.elements->data[idx];
            }
            
            /* TODO: Record field access */
            return error_type_at(checker, expr->loc, "Cannot access field '%s' on type %s",
                string_cstr(dot->field),
                string_cstr(type_to_string(checker->arena, obj_type)));
        }
            
        case EXPR_RANGE: {
            /* Range expression: start..end or start..=end
             * Both bounds must be the same type (typically Int)
             * Returns Range(element_type)
             */
            RangeExpr* range = &expr->data.range;
            Type* start_type = checker_infer_expr(checker, range->start);
            if (start_type->kind == TYPE_ERROR) return start_type;
            
            Type* end_type = checker_infer_expr(checker, range->end);
            if (end_type->kind == TYPE_ERROR) return end_type;
            
            /* Bounds must have the same type */
            if (!type_equals(start_type, end_type)) {
                return error_type_at(checker, expr->loc,
                    "Range bounds must have same type: %s vs %s",
                    string_cstr(type_to_string(checker->arena, start_type)),
                    string_cstr(type_to_string(checker->arena, end_type)));
            }
            
            /* Return Range(element_type) */
            TypeVec* args = TypeVec_new(checker->arena);
            TypeVec_push(checker->arena, args, start_type);
            return type_con(checker->arena, string_new(checker->arena, "Range"), args);
        }
            
        case EXPR_MAP: {
            /* Map literal: %{ key: value, ... }
             * All keys must have same type, all values must have same type
             * Returns Map(key_type, value_type)
             */
            MapExpr* map = &expr->data.map;
            
            if (map->entries->len == 0) {
                /* Empty map has type Map(a, b) with fresh type variables */
                int key_var = type_fresh_var_id();
                int val_var = type_fresh_var_id();
                Type* key_type = type_var(checker->arena, string_new(checker->arena, "k"), key_var);
                Type* val_type = type_var(checker->arena, string_new(checker->arena, "v"), val_var);
                return type_map(checker->arena, key_type, val_type);
            }
            
            /* Infer types from first entry */
            Type* key_type = checker_infer_expr(checker, map->entries->data[0].key);
            if (key_type->kind == TYPE_ERROR) return key_type;
            
            Type* val_type = checker_infer_expr(checker, map->entries->data[0].value);
            if (val_type->kind == TYPE_ERROR) return val_type;
            
            /* Check all entries have consistent types */
            for (size_t i = 1; i < map->entries->len; i++) {
                Type* k = checker_infer_expr(checker, map->entries->data[i].key);
                if (k->kind == TYPE_ERROR) return k;
                
                if (!type_equals(key_type, k)) {
                    return error_type_at(checker, expr->loc,
                        "Map key type mismatch at entry %zu: expected %s, got %s",
                        i, string_cstr(type_to_string(checker->arena, key_type)),
                        string_cstr(type_to_string(checker->arena, k)));
                }
                
                Type* v = checker_infer_expr(checker, map->entries->data[i].value);
                if (v->kind == TYPE_ERROR) return v;
                
                if (!type_equals(val_type, v)) {
                    return error_type_at(checker, expr->loc,
                        "Map value type mismatch at entry %zu: expected %s, got %s",
                        i, string_cstr(type_to_string(checker->arena, val_type)),
                        string_cstr(type_to_string(checker->arena, v)));
                }
            }
            
            return type_map(checker->arena, key_type, val_type);
        }
            
        case EXPR_LIST_COMP: {
            /* List comprehension: [body for var in iterable if condition] */
            ListCompExpr* comp = &expr->data.list_comp;
            
            /* Infer type of iterable */
            Type* iter_type = checker_infer_expr(checker, comp->iterable);
            if (iter_type->kind == TYPE_ERROR) return iter_type;
            
            /* Iterable must be List(T) or Range */
            Type* elem_type = NULL;
            if (iter_type->kind == TYPE_CON) {
                const char* name = string_cstr(iter_type->data.con.name);
                if (strcmp(name, "List") == 0 && iter_type->data.con.args && 
                    iter_type->data.con.args->len > 0) {
                    elem_type = iter_type->data.con.args->data[0];
                } else if (strcmp(name, "Range") == 0) {
                    /* Range iterates over Int */
                    elem_type = type_int(checker->arena);
                }
            }
            
            if (!elem_type) {
                return error_type_at(checker, expr->loc, 
                    "List comprehension requires an iterable (List or Range), got %s",
                    string_cstr(type_to_string(checker->arena, iter_type)));
            }
            
            /* Push scope and bind loop variable */
            checker_push_scope(checker);
            checker_define(checker, comp->var_name, elem_type);
            
            /* Check filter condition if present */
            if (comp->condition) {
                Type* cond_type = checker_infer_expr(checker, comp->condition);
                if (cond_type->kind == TYPE_ERROR) {
                    checker_pop_scope(checker);
                    return cond_type;
                }
                if (cond_type->kind != TYPE_BOOL) {
                    checker_pop_scope(checker);
                    return error_type_at(checker, expr->loc,
                        "List comprehension filter must be Bool, got %s",
                        string_cstr(type_to_string(checker->arena, cond_type)));
                }
            }
            
            /* Infer body type */
            Type* body_type = checker_infer_expr(checker, comp->body);
            checker_pop_scope(checker);
            
            if (body_type->kind == TYPE_ERROR) return body_type;
            
            /* Result is List(body_type) */
            return type_list(checker->arena, body_type);
        }
            
        case EXPR_INTERP_STRING: {
            /* String interpolation: "Hello, {name}!" */
            InterpStringExpr* interp = &expr->data.interp_string;
            
            /* Type check each part - string literals and interpolated expressions */
            for (size_t i = 0; i < interp->parts->len; i++) {
                Type* part_type = checker_infer_expr(checker, interp->parts->data[i]);
                if (part_type->kind == TYPE_ERROR) return part_type;
                /* All parts get implicitly converted to string (assume Show trait) */
            }
            
            /* Result is always String */
            return type_string(checker->arena);
        }
            
        /* TODO: Implement remaining expression types */
        case EXPR_RECORD_UPDATE:
        case EXPR_TRY: {
            /* The ? operator unwraps Result types */
            Type* operand_type = checker_infer_expr(checker, expr->data.try_expr.operand);
            if (operand_type->kind == TYPE_ERROR) return operand_type;
            
            /* Operand must be Result(ok, err) */
            if (!type_is_result(operand_type)) {
                return error_type_at(checker, expr->loc, "The ? operator requires a Result type, got %s",
                    string_cstr(type_to_string(checker->arena, operand_type)));
            }
            
            /* Return the Ok type (first type argument) */
            return operand_type->data.con.args->data[0];
        }
            
        case EXPR_SPAWN:
        case EXPR_SEND:
        case EXPR_RECEIVE:
            return error_type(checker, "Type checking not yet implemented for this expression type");
    }
    
    return error_type(checker, "Unknown expression type");
}

/* ========== Type Expression Resolution ========== */

/* Convert a TypeExpr (AST type annotation) to a Type */
static Type* resolve_type_expr(Checker* checker, TypeExpr* type_expr) {
    assert(checker != NULL);
    if (!type_expr) return NULL;
    assert(type_expr->kind == TYPE_NAMED || type_expr->kind == TYPE_FUNCTION);
    
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
            
            /* Unknown type - treat as a simple type constructor for flexibility */
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

/* Strict version that errors on unknown types (used for type definitions) */
static Type* resolve_type_expr_strict(Checker* checker, TypeExpr* type_expr) {
    assert(checker != NULL);
    if (!type_expr) return NULL;
    assert(type_expr->kind == TYPE_NAMED || type_expr->kind == TYPE_FUNCTION);
    
    if (type_expr->kind == TYPE_NAMED) {
        String* name = type_expr->data.named.name;
        const char* name_str = string_cstr(name);
        
        /* Check for built-in primitive types */
        if (strcmp(name_str, "Int") == 0 ||
            strcmp(name_str, "Float") == 0 ||
            strcmp(name_str, "String") == 0 ||
            strcmp(name_str, "Bool") == 0) {
            return resolve_type_expr(checker, type_expr);
        }
        
        /* Check for parameterized types (List, Option, Result, Map) */
        TypeExprVec* args = type_expr->data.named.args;
        if (args && args->len > 0) {
            if (strcmp(name_str, "List") == 0 ||
                strcmp(name_str, "Option") == 0 ||
                strcmp(name_str, "Result") == 0 ||
                strcmp(name_str, "Map") == 0) {
                /* Recursively validate arguments with strict checking */
                for (size_t i = 0; i < args->len; i++) {
                    Type* arg = resolve_type_expr_strict(checker, args->data[i]);
                    if (!arg || arg->kind == TYPE_ERROR) return arg;
                }
                return resolve_type_expr(checker, type_expr);
            }
        }
        
        /* Look up in type environment */
        Type* defined = type_env_lookup_type(checker->env, name);
        if (defined) {
            return defined;
        }
        
        /* Unknown type - error in strict mode */
        return error_type(checker, "Unknown type '%s'", name_str);
    }
    
    /* For function types, use regular resolution */
    return resolve_type_expr(checker, type_expr);
}

/* ========== Statement Type Checking ========== */

/* Bind a pattern to a type in the environment */
static bool bind_pattern(Checker* checker, Pattern* pattern, Type* type) {
    assert(checker != NULL);
    assert(pattern != NULL);
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
            
        case PATTERN_CONSTRUCTOR: {
            /* Constructor pattern: Some(x), Ok(value), Err(msg), None, etc.
             * We need to determine what type the sub-patterns bind to based on
             * the constructor name and the scrutinee type.
             */
            ConstructorPattern* ctor = &pattern->data.constructor;
            const char* ctor_name = string_cstr(ctor->name);
            
            /* Handle Option constructors */
            if (type->kind == TYPE_CON && 
                strcmp(string_cstr(type->data.con.name), "Option") == 0) {
                if (strcmp(ctor_name, "Some") == 0) {
                    /* Some(x) binds x to the inner type of Option(a) */
                    if (ctor->args && ctor->args->len == 1) {
                        Type* inner_type = type->data.con.args->data[0];
                        return bind_pattern(checker, ctor->args->data[0], inner_type);
                    }
                } else if (strcmp(ctor_name, "None") == 0) {
                    /* None doesn't bind anything */
                    return true;
                }
            }
            
            /* Handle Result constructors */
            if (type->kind == TYPE_CON &&
                strcmp(string_cstr(type->data.con.name), "Result") == 0) {
                if (strcmp(ctor_name, "Ok") == 0) {
                    /* Ok(x) binds x to the first type arg of Result(ok, err) */
                    if (ctor->args && ctor->args->len == 1) {
                        Type* ok_type = type->data.con.args->data[0];
                        return bind_pattern(checker, ctor->args->data[0], ok_type);
                    }
                } else if (strcmp(ctor_name, "Err") == 0) {
                    /* Err(e) binds e to the second type arg of Result(ok, err) */
                    if (ctor->args && ctor->args->len == 1) {
                        Type* err_type = type->data.con.args->data[1];
                        return bind_pattern(checker, ctor->args->data[0], err_type);
                    }
                }
            }
            
            /* For other constructors, we'd need a type environment with
             * constructor definitions. For now, just return true. */
            return true;
        }
            
        case PATTERN_REST:
            /* TODO: Implement rest pattern binding */
            return true;
    }
    
    return false;
}

static bool check_let_stmt(Checker* checker, LetStmt* stmt) {
    assert(checker != NULL);
    assert(stmt != NULL);
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
    assert(checker != NULL);
    assert(stmt != NULL);
    switch (stmt->type) {
        case STMT_LET:
            return check_let_stmt(checker, &stmt->data.let);
            
        case STMT_EXPR:
            /* Type check the expression but discard the result */
            checker_infer_expr(checker, stmt->data.expr.expr);
            return !checker_has_errors(checker);
            
        case STMT_FN: {
            /* Function definition: fn name(params) -> return_type: body
             * - Add parameters to scope
             * - Type check body
             * - Verify body type matches return type annotation (if present)
             */
            FunctionDef* fn = &stmt->data.fn;
            
            /* Push scope for function parameters */
            checker_push_scope(checker);
            
            /* Add parameters to scope with their annotated types */
            if (fn->params) {
                for (size_t i = 0; i < fn->params->len; i++) {
                    Parameter* param = &fn->params->data[i];
                    Type* param_type = NULL;
                    
                    if (param->type_ann) {
                        param_type = resolve_type_expr(checker, param->type_ann);
                        if (!param_type || param_type->kind == TYPE_ERROR) {
                            checker_pop_scope(checker);
                            return false;
                        }
                    } else {
                        /* No type annotation - use fresh type variable */
                        int var_id = type_fresh_var_id();
                        param_type = type_var(checker->arena, param->name, var_id);
                    }
                    
                    checker_define(checker, param->name, param_type);
                }
            }
            
            /* Type check the function body */
            Type* body_type = checker_infer_expr(checker, fn->body);
            
            checker_pop_scope(checker);
            
            if (body_type->kind == TYPE_ERROR) {
                return false;
            }
            
            /* If there's a return type annotation, verify body matches */
            if (fn->return_type) {
                Type* return_type = resolve_type_expr(checker, fn->return_type);
                if (!return_type || return_type->kind == TYPE_ERROR) {
                    return false;
                }
                
                if (!type_assignable(body_type, return_type)) {
                    add_error(checker, "Function '%s' body has type %s, but declared return type is %s",
                        string_cstr(fn->name),
                        string_cstr(type_to_string(checker->arena, body_type)),
                        string_cstr(type_to_string(checker->arena, return_type)));
                    return false;
                }
            }
            
            /* TODO: Register function in environment for recursive calls */
            return true;
        }
            
        case STMT_TYPE_DEF: {
            TypeDef* def = &stmt->data.type_def;
            
            // Validate variant field types
            if (def->variants) {
                for (size_t i = 0; i < def->variants->len; i++) {
                    TypeVariant* variant = &def->variants->data[i];
                    if (variant->fields) {
                        for (size_t j = 0; j < variant->fields->len; j++) {
                            TypeField* field = &variant->fields->data[j];
                            if (field->type_ann) {
                                Type* field_type = resolve_type_expr_strict(checker, field->type_ann);
                                if (!field_type || field_type->kind == TYPE_ERROR) {
                                    add_error(checker, "Unknown type in variant '%s' field '%s'",
                                        string_cstr(variant->name),
                                        string_cstr(field->name));
                                    return false;
                                }
                            }
                        }
                    }
                }
            }
            
            // Validate record field types
            if (def->record_fields) {
                for (size_t i = 0; i < def->record_fields->len; i++) {
                    TypeField* field = &def->record_fields->data[i];
                    if (field->type_ann) {
                        Type* field_type = resolve_type_expr_strict(checker, field->type_ann);
                        if (!field_type || field_type->kind == TYPE_ERROR) {
                            add_error(checker, "Unknown type in record field '%s'",
                                string_cstr(field->name));
                            return false;
                        }
                    }
                }
            }
            
            /* TODO: Register type in environment for use in other definitions */
            return true;
        }
            
        case STMT_RETURN:
        case STMT_IMPORT:
        case STMT_DEFER:
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
    assert(checker != NULL);
    assert(stmts != NULL);
    for (size_t i = 0; i < stmts->len; i++) {
        if (!checker_check_stmt(checker, stmts->data[i])) {
            return false;
        }
    }
    return true;
}

/* ========== Error Handling ========== */

bool checker_has_errors(Checker* checker) {
    assert(checker != NULL);
    assert(checker->arena != NULL);
    return checker->errors != NULL;
}

const char* checker_first_error(Checker* checker) {
    assert(checker != NULL);
    assert(checker->arena != NULL);
    if (!checker->errors) return NULL;
    return string_cstr(checker->errors->message);
}

/* ========== Environment Access ========== */

TypeEnv* checker_env(Checker* checker) {
    assert(checker != NULL);
    assert(checker->env != NULL);
    return checker->env;
}

void checker_define(Checker* checker, String* name, Type* type) {
    assert(checker != NULL);
    assert(name != NULL);
    type_env_define(checker->env, name, type);
}

void checker_push_scope(Checker* checker) {
    assert(checker != NULL);
    assert(checker->env != NULL);
    type_env_push_scope(checker->env);
}

void checker_pop_scope(Checker* checker) {
    assert(checker != NULL);
    assert(checker->env != NULL);
    type_env_pop_scope(checker->env);
}
