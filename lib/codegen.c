/* Fern Code Generator
 *
 * Generates QBE IR from type-checked Fern AST.
 */

#include "codegen.h"
#include "ast.h"
#include "arena.h"
#include "fern_string.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

/* ========== Codegen Structure ========== */

/* Maximum number of deferred expressions in a single function */
#define MAX_DEFERS 64

/* Maximum number of tracked wide (64-bit) variables */
#define MAX_WIDE_VARS 256

struct Codegen {
    Arena* arena;
    String* output;      /* Accumulated QBE IR (functions) */
    String* data_section;/* Accumulated data section (strings, etc.) */
    int temp_counter;    /* For generating unique temporaries %t0, %t1, ... */
    int label_counter;   /* For generating unique labels @L0, @L1, ... */
    int string_counter;  /* For generating unique string labels $str0, $str1, ... */
    
    /* Defer stack for current function - expressions run in LIFO order */
    Expr* defer_stack[MAX_DEFERS];
    int defer_count;
    
    /* Track variables that are 64-bit (pointers: lists, strings) */
    String* wide_vars[MAX_WIDE_VARS];
    int wide_var_count;
};

/* ========== Helper Functions ========== */

/**
 * Append text to the output buffer.
 * @param cg The codegen context.
 * @param fmt The format string.
 * @param ... The format arguments.
 */
static void emit(Codegen* cg, const char* fmt, ...) {
    assert(cg != NULL);
    assert(fmt != NULL);
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    
    cg->output = string_concat(cg->arena, cg->output, string_new(cg->arena, buf));
}

/**
 * Generate a fresh temporary name.
 * @param cg The codegen context.
 * @return The new temporary name.
 */
static String* fresh_temp(Codegen* cg) {
    assert(cg != NULL);
    assert(cg->temp_counter >= 0);
    char buf[32];
    snprintf(buf, sizeof(buf), "%%t%d", cg->temp_counter++);
    return string_new(cg->arena, buf);
}

/**
 * Generate a fresh label name.
 * @param cg The codegen context.
 * @return The new label name.
 */
static String* fresh_label(Codegen* cg) {
    assert(cg != NULL);
    assert(cg->label_counter >= 0);
    char buf[32];
    snprintf(buf, sizeof(buf), "@L%d", cg->label_counter++);
    return string_new(cg->arena, buf);
}

/* ========== QBE Type Helpers ========== */

/**
 * Register a variable as wide (64-bit pointer type).
 * @param cg The codegen context.
 * @param name The variable name.
 */
static void register_wide_var(Codegen* cg, String* name) {
    assert(cg != NULL);
    assert(name != NULL);
    assert(cg->wide_var_count < MAX_WIDE_VARS);
    cg->wide_vars[cg->wide_var_count++] = name;
}

/**
 * Check if a variable is wide (64-bit pointer type).
 * @param cg The codegen context.
 * @param name The variable name.
 * @return True if the variable is wide.
 */
static bool is_wide_var(Codegen* cg, String* name) {
    assert(cg != NULL);
    assert(name != NULL);
    for (int i = 0; i < cg->wide_var_count; i++) {
        if (strcmp(string_cstr(cg->wide_vars[i]), string_cstr(name)) == 0) {
            return true;
        }
    }
    return false;
}

static void clear_wide_vars(Codegen* cg) __attribute__((unused));
/**
 * Clear wide variable tracking (call at start of each function).
 * Currently unused but available for future per-function scope tracking.
 * @param cg The codegen context.
 */
static void clear_wide_vars(Codegen* cg) {
    /* FERN_STYLE: allow(assertion-density) - simple reset function */
    assert(cg != NULL);
    cg->wide_var_count = 0;
}

/**
 * Check if a function name returns a pointer type (string or list).
 * @param name The function name.
 * @return True if the function returns a pointer type.
 */
static bool fn_returns_pointer(const char* name) {
    /* FERN_STYLE: allow(assertion-density) - simple lookup */
    if (name == NULL) return false;
    
    /* String functions that return String */
    if (strcmp(name, "str_concat") == 0) return true;
    if (strcmp(name, "str_slice") == 0) return true;
    if (strcmp(name, "str_trim") == 0) return true;
    if (strcmp(name, "str_trim_start") == 0) return true;
    if (strcmp(name, "str_trim_end") == 0) return true;
    if (strcmp(name, "str_to_upper") == 0) return true;
    if (strcmp(name, "str_to_lower") == 0) return true;
    if (strcmp(name, "str_replace") == 0) return true;
    if (strcmp(name, "str_repeat") == 0) return true;
    if (strcmp(name, "str_join") == 0) return true;
    
    /* List functions that return List */
    if (strcmp(name, "list_push") == 0) return true;
    if (strcmp(name, "list_filter") == 0) return true;
    if (strcmp(name, "list_map") == 0) return true;
    if (strcmp(name, "list_reverse") == 0) return true;
    if (strcmp(name, "list_concat") == 0) return true;
    if (strcmp(name, "list_tail") == 0) return true;
    
    return false;
}

/* Print type for polymorphic print/println */
typedef enum {
    PRINT_INT,
    PRINT_STRING,
    PRINT_BOOL
} PrintType;

/**
 * Determine the print type for an expression.
 * Used to select the correct runtime print function.
 * @param cg The codegen context.
 * @param expr The expression to check.
 * @return The print type (PRINT_INT, PRINT_STRING, or PRINT_BOOL).
 */
static PrintType get_print_type(Codegen* cg, Expr* expr) {
    assert(cg != NULL);
    assert(cg->arena != NULL);
    if (expr == NULL) return PRINT_INT;
    
    switch (expr->type) {
        case EXPR_STRING_LIT:
        case EXPR_INTERP_STRING:
            return PRINT_STRING;
        
        case EXPR_BOOL_LIT:
            return PRINT_BOOL;
        
        case EXPR_INT_LIT:
        case EXPR_FLOAT_LIT:
            return PRINT_INT;
        
        case EXPR_IDENT:
            /* Check if variable is a wide type (string/list) */
            if (is_wide_var(cg, expr->data.ident.name)) {
                return PRINT_STRING;
            }
            return PRINT_INT;
        
        case EXPR_CALL: {
            CallExpr* call = &expr->data.call;
            /* Check for module.function calls that return strings */
            if (call->func->type == EXPR_DOT) {
                DotExpr* dot = &call->func->data.dot;
                if (dot->object->type == EXPR_IDENT) {
                    const char* module = string_cstr(dot->object->data.ident.name);
                    const char* func = string_cstr(dot->field);
                    /* String module functions that return String */
                    if (strcmp(module, "String") == 0) {
                        if (strcmp(func, "concat") == 0 ||
                            strcmp(func, "slice") == 0 ||
                            strcmp(func, "trim") == 0 ||
                            strcmp(func, "trim_start") == 0 ||
                            strcmp(func, "trim_end") == 0 ||
                            strcmp(func, "to_upper") == 0 ||
                            strcmp(func, "to_lower") == 0 ||
                            strcmp(func, "replace") == 0 ||
                            strcmp(func, "repeat") == 0) {
                            return PRINT_STRING;
                        }
                    }
                }
            }
            if (call->func->type == EXPR_IDENT) {
                const char* fn_name = string_cstr(call->func->data.ident.name);
                /* String functions return strings */
                if (strncmp(fn_name, "str_", 4) == 0) {
                    return PRINT_STRING;
                }
            }
            return PRINT_INT;
        }
        
        default:
            return PRINT_INT;
    }
}

/**
 * Get QBE type specifier for an expression.
 * Returns 'l' for pointer types (lists, strings), 'w' for word types (int, bool).
 * @param expr The expression to check.
 * @return 'l' for 64-bit, 'w' for 32-bit.
 */
static char qbe_type_for_expr(Expr* expr) {
    // FERN_STYLE: allow(function-length) type dispatch handles all module return types
    /* FERN_STYLE: allow(assertion-density) - simple type lookup */
    if (expr == NULL) return 'w';
    
    switch (expr->type) {
        /* Pointer types (64-bit) */
        case EXPR_LIST:
        case EXPR_STRING_LIT:
            return 'l';
        
        /* Word types (32-bit) */
        case EXPR_INT_LIT:
        case EXPR_BOOL_LIT:
            return 'w';
        
        /* Check if function call returns pointer type */
        case EXPR_CALL: {
            CallExpr* call = &expr->data.call;
            /* Check module.function calls */
            if (call->func->type == EXPR_DOT) {
                DotExpr* dot = &call->func->data.dot;
                if (dot->object->type == EXPR_IDENT) {
                    const char* module = string_cstr(dot->object->data.ident.name);
                    const char* func = string_cstr(dot->field);
                    /* String module functions returning String or pointer types */
                    if (strcmp(module, "String") == 0) {
                        if (strcmp(func, "concat") == 0 ||
                            strcmp(func, "slice") == 0 ||
                            strcmp(func, "trim") == 0 ||
                            strcmp(func, "trim_start") == 0 ||
                            strcmp(func, "trim_end") == 0 ||
                            strcmp(func, "to_upper") == 0 ||
                            strcmp(func, "to_lower") == 0 ||
                            strcmp(func, "replace") == 0 ||
                            strcmp(func, "repeat") == 0 ||
                            strcmp(func, "split") == 0 ||
                            strcmp(func, "lines") == 0 ||
                            strcmp(func, "join") == 0 ||
                            strcmp(func, "index_of") == 0 ||
                            strcmp(func, "char_at") == 0) {
                            return 'l';
                        }
                    }
                    /* List module functions returning List (pointer) */
                    if (strcmp(module, "List") == 0) {
                        if (strcmp(func, "push") == 0 ||
                            strcmp(func, "reverse") == 0 ||
                            strcmp(func, "concat") == 0 ||
                            strcmp(func, "tail") == 0) {
                            return 'l';
                        }
                    }
                    /* File module functions returning Result or List (pointer) */
                    if (strcmp(module, "File") == 0) {
                        if (strcmp(func, "read") == 0 ||
                            strcmp(func, "write") == 0 ||
                            strcmp(func, "append") == 0 ||
                            strcmp(func, "delete") == 0 ||
                            strcmp(func, "size") == 0 ||
                            strcmp(func, "list_dir") == 0) {
                            return 'l';
                        }
                    }
                    /* System module functions returning pointers */
                    if (strcmp(module, "System") == 0) {
                        if (strcmp(func, "args") == 0 ||
                            strcmp(func, "arg") == 0) {
                            return 'l';
                        }
                    }
                }
            }
            if (call->func->type == EXPR_IDENT) {
                const char* fn_name = string_cstr(call->func->data.ident.name);
                if (fn_returns_pointer(fn_name)) {
                    return 'l';
                }
            }
            return 'w';
        }
        
        /* For identifiers and other exprs, we'd need type info - default to word */
        /* TODO: track types through codegen for proper handling */
        case EXPR_IDENT:
        case EXPR_BINARY:
        case EXPR_UNARY:
        case EXPR_IF:
        case EXPR_MATCH:
        case EXPR_BLOCK:
        default:
            return 'w';
    }
}

/* ========== Codegen Creation ========== */

/**
 * Create a new code generator.
 * @param arena The arena for allocations.
 * @return The new codegen context.
 */
Codegen* codegen_new(Arena* arena) {
    assert(arena != NULL);
    Codegen* cg = arena_alloc(arena, sizeof(Codegen));
    assert(cg != NULL);
    cg->arena = arena;
    cg->output = string_new(arena, "");
    cg->data_section = string_new(arena, "");
    cg->temp_counter = 0;
    cg->label_counter = 0;
    cg->string_counter = 0;
    cg->defer_count = 0;
    cg->wide_var_count = 0;
    return cg;
}

/**
 * Push a deferred expression onto the stack.
 * @param cg The codegen context.
 * @param expr The expression to defer.
 */
static void push_defer(Codegen* cg, Expr* expr) {
    assert(cg != NULL);
    assert(expr != NULL);
    assert(cg->defer_count < MAX_DEFERS);
    cg->defer_stack[cg->defer_count++] = expr;
}

/**
 * Emit all deferred expressions in reverse order (LIFO).
 * @param cg The codegen context.
 */
static void emit_defers(Codegen* cg) {
    assert(cg != NULL);
    assert(cg->defer_count >= 0 && cg->defer_count <= MAX_DEFERS);
    /* Emit in reverse order: last defer runs first */
    for (int i = cg->defer_count - 1; i >= 0; i--) {
        assert(cg->defer_stack[i] != NULL);
        codegen_expr(cg, cg->defer_stack[i]);
    }
}

/**
 * Clear the defer stack (called at function boundaries).
 * @param cg The codegen context.
 */
static void clear_defers(Codegen* cg) {
    assert(cg != NULL);
    assert(cg->defer_count >= 0);
    cg->defer_count = 0;
}

/**
 * Emit to data section.
 * @param cg The codegen context.
 * @param fmt The format string.
 * @param ... The format arguments.
 */
static void emit_data(Codegen* cg, const char* fmt, ...) {
    assert(cg != NULL);
    assert(fmt != NULL);
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    
    cg->data_section = string_concat(cg->arena, cg->data_section, string_new(cg->arena, buf));
}

/**
 * Generate a fresh string label.
 * @param cg The codegen context.
 * @return The new string label.
 */
static String* fresh_string_label(Codegen* cg) {
    assert(cg != NULL);
    assert(cg->string_counter >= 0);
    char buf[32];
    snprintf(buf, sizeof(buf), "$str%d", cg->string_counter++);
    return string_new(cg->arena, buf);
}

/* ========== Expression Code Generation ========== */

/**
 * Generate QBE IR code for an expression.
 * @param cg The codegen context.
 * @param expr The expression to compile.
 * @return The temporary holding the result.
 */
String* codegen_expr(Codegen* cg, Expr* expr) {
    // FERN_STYLE: allow(function-length) code generation handles all expression types in one switch
    assert(cg != NULL);
    assert(expr != NULL);
    
    switch (expr->type) {
        case EXPR_INT_LIT: {
            String* tmp = fresh_temp(cg);
            emit(cg, "    %s =w copy %lld\n", string_cstr(tmp), expr->data.int_lit.value);
            return tmp;
        }
        
        case EXPR_FLOAT_LIT: {
            String* tmp = fresh_temp(cg);
            emit(cg, "    %s =d copy d_%g\n", string_cstr(tmp), expr->data.float_lit.value);
            return tmp;
        }
        
        case EXPR_BOOL_LIT: {
            String* tmp = fresh_temp(cg);
            emit(cg, "    %s =w copy %d\n", string_cstr(tmp), expr->data.bool_lit.value ? 1 : 0);
            return tmp;
        }
        
        case EXPR_STRING_LIT: {
            /* Create a data section entry for the string */
            String* label = fresh_string_label(cg);
            String* tmp = fresh_temp(cg);
            
            /* Emit data section: data $str0 = { b "hello", b 0 } */
            emit_data(cg, "data %s = { b \"%s\", b 0 }\n", 
                string_cstr(label), string_cstr(expr->data.string_lit.value));
            
            /* Load pointer to string */
            emit(cg, "    %s =l copy %s\n", string_cstr(tmp), string_cstr(label));
            return tmp;
        }
        
        case EXPR_INTERP_STRING: {
            /* String interpolation: "Hello, {name}!" 
             * Parts is a vector alternating string literals and expressions.
             * We convert each part to a string and concatenate them all.
             */
            InterpStringExpr* interp = &expr->data.interp_string;
            assert(interp->parts != NULL);
            
            if (interp->parts->len == 0) {
                /* Empty interpolated string - return empty string */
                String* label = fresh_string_label(cg);
                String* tmp = fresh_temp(cg);
                emit_data(cg, "data %s = { b \"\", b 0 }\n", string_cstr(label));
                emit(cg, "    %s =l copy %s\n", string_cstr(tmp), string_cstr(label));
                return tmp;
            }
            
            /* Process first part */
            Expr* first = interp->parts->data[0];
            String* result;
            
            if (first->type == EXPR_STRING_LIT) {
                result = codegen_expr(cg, first);
            } else {
                /* Convert expression to string based on its type */
                String* val = codegen_expr(cg, first);
                PrintType pt = get_print_type(cg, first);
                result = fresh_temp(cg);
                switch (pt) {
                    case PRINT_STRING:
                        emit(cg, "    %s =l copy %s\n", string_cstr(result), string_cstr(val));
                        break;
                    case PRINT_BOOL:
                        emit(cg, "    %s =l call $fern_bool_to_str(w %s)\n", 
                            string_cstr(result), string_cstr(val));
                        break;
                    case PRINT_INT:
                    default:
                        emit(cg, "    %s =l call $fern_int_to_str(w %s)\n", 
                            string_cstr(result), string_cstr(val));
                        break;
                }
            }
            
            /* Concatenate remaining parts */
            for (size_t i = 1; i < interp->parts->len; i++) {
                Expr* part = interp->parts->data[i];
                String* part_str;
                
                if (part->type == EXPR_STRING_LIT) {
                    part_str = codegen_expr(cg, part);
                } else {
                    /* Convert expression to string */
                    String* val = codegen_expr(cg, part);
                    PrintType pt = get_print_type(cg, part);
                    part_str = fresh_temp(cg);
                    switch (pt) {
                        case PRINT_STRING:
                            emit(cg, "    %s =l copy %s\n", string_cstr(part_str), string_cstr(val));
                            break;
                        case PRINT_BOOL:
                            emit(cg, "    %s =l call $fern_bool_to_str(w %s)\n", 
                                string_cstr(part_str), string_cstr(val));
                            break;
                        case PRINT_INT:
                        default:
                            emit(cg, "    %s =l call $fern_int_to_str(w %s)\n", 
                                string_cstr(part_str), string_cstr(val));
                            break;
                    }
                }
                
                /* Concatenate result with this part */
                String* new_result = fresh_temp(cg);
                emit(cg, "    %s =l call $fern_str_concat(l %s, l %s)\n",
                    string_cstr(new_result), string_cstr(result), string_cstr(part_str));
                result = new_result;
            }
            
            return result;
        }
        
        case EXPR_BINARY: {
            BinaryExpr* bin = &expr->data.binary;
            String* left = codegen_expr(cg, bin->left);
            String* right = codegen_expr(cg, bin->right);
            String* tmp = fresh_temp(cg);
            
            const char* op = NULL;
            switch (bin->op) {
                case BINOP_ADD: op = "add"; break;
                case BINOP_SUB: op = "sub"; break;
                case BINOP_MUL: op = "mul"; break;
                case BINOP_DIV: op = "div"; break;
                case BINOP_MOD: op = "rem"; break;
                case BINOP_EQ:  op = "ceqw"; break;
                case BINOP_NE:  op = "cnew"; break;
                case BINOP_LT:  op = "csltw"; break;
                case BINOP_LE:  op = "cslew"; break;
                case BINOP_GT:  op = "csgtw"; break;
                case BINOP_GE:  op = "csgew"; break;
                default:
                    /* TODO: Handle other operators */
                    emit(cg, "    # TODO: unhandled binary op %d\n", bin->op);
                    return tmp;
            }
            
            emit(cg, "    %s =w %s %s, %s\n", 
                string_cstr(tmp), op, string_cstr(left), string_cstr(right));
            return tmp;
        }
        
        case EXPR_UNARY: {
            UnaryExpr* unary = &expr->data.unary;
            String* operand = codegen_expr(cg, unary->operand);
            String* tmp = fresh_temp(cg);
            
            switch (unary->op) {
                case UNOP_NEG:
                    emit(cg, "    %s =w sub 0, %s\n", string_cstr(tmp), string_cstr(operand));
                    break;
                case UNOP_NOT:
                    emit(cg, "    %s =w ceqw %s, 0\n", string_cstr(tmp), string_cstr(operand));
                    break;
                default:
                    emit(cg, "    # TODO: unhandled unary op %d\n", unary->op);
            }
            return tmp;
        }
        
        case EXPR_IDENT: {
            /* Variable reference - return the variable name as a QBE temporary */
            String* tmp = fresh_temp(cg);
            /* Check if variable is a wide type (pointer) */
            char type_spec = is_wide_var(cg, expr->data.ident.name) ? 'l' : 'w';
            emit(cg, "    %s =%c copy %%%s\n", string_cstr(tmp), type_spec, 
                string_cstr(expr->data.ident.name));
            return tmp;
        }
        
        case EXPR_BLOCK: {
            BlockExpr* block = &expr->data.block;
            String* last = NULL;
            
            /* Generate code for each statement */
            for (size_t i = 0; i < block->stmts->len; i++) {
                codegen_stmt(cg, block->stmts->data[i]);
            }
            
            /* Generate code for the final expression if present */
            if (block->final_expr) {
                last = codegen_expr(cg, block->final_expr);
            } else {
                /* Unit value */
                last = fresh_temp(cg);
                emit(cg, "    %s =w copy 0\n", string_cstr(last));
            }
            
            return last;
        }
        
        case EXPR_IF: {
            IfExpr* if_expr = &expr->data.if_expr;
            String* cond = codegen_expr(cg, if_expr->condition);
            String* then_label = fresh_label(cg);
            String* else_label = fresh_label(cg);
            String* end_label = fresh_label(cg);
            String* result = fresh_temp(cg);
            
            emit(cg, "    jnz %s, %s, %s\n", 
                string_cstr(cond), string_cstr(then_label), string_cstr(else_label));
            
            /* Then branch */
            emit(cg, "%s\n", string_cstr(then_label));
            String* then_val = codegen_expr(cg, if_expr->then_branch);
            emit(cg, "    %s =w copy %s\n", string_cstr(result), string_cstr(then_val));
            emit(cg, "    jmp %s\n", string_cstr(end_label));
            
            /* Else branch */
            emit(cg, "%s\n", string_cstr(else_label));
            if (if_expr->else_branch) {
                String* else_val = codegen_expr(cg, if_expr->else_branch);
                emit(cg, "    %s =w copy %s\n", string_cstr(result), string_cstr(else_val));
            } else {
                emit(cg, "    %s =w copy 0\n", string_cstr(result));
            }
            emit(cg, "    jmp %s\n", string_cstr(end_label));
            
            emit(cg, "%s\n", string_cstr(end_label));
            return result;
        }
        
        case EXPR_MATCH: {
            MatchExpr* match = &expr->data.match_expr;
            String* scrutinee = codegen_expr(cg, match->value);
            String* result = fresh_temp(cg);
            String* end_label = fresh_label(cg);
            
            /* Generate code for each arm */
            for (size_t i = 0; i < match->arms->len; i++) {
                MatchArm* arm = &match->arms->data[i];
                String* next_arm_label = fresh_label(cg);
                String* arm_body_label = fresh_label(cg);
                
                /* Pattern matching */
                switch (arm->pattern->type) {
                    case PATTERN_WILDCARD:
                        /* Wildcard always matches - just jump to body */
                        emit(cg, "    jmp %s\n", string_cstr(arm_body_label));
                        break;
                        
                    case PATTERN_IDENT:
                        /* Identifier pattern - bind value and jump to body */
                        emit(cg, "    %%%s =w copy %s\n", 
                            string_cstr(arm->pattern->data.ident), string_cstr(scrutinee));
                        emit(cg, "    jmp %s\n", string_cstr(arm_body_label));
                        break;
                        
                    case PATTERN_LIT: {
                        /* Literal pattern - compare and branch */
                        Expr* lit = arm->pattern->data.literal;
                        String* lit_temp = codegen_expr(cg, lit);
                        String* cmp = fresh_temp(cg);
                        emit(cg, "    %s =w ceqw %s, %s\n", 
                            string_cstr(cmp), string_cstr(scrutinee), string_cstr(lit_temp));
                        emit(cg, "    jnz %s, %s, %s\n",
                            string_cstr(cmp), string_cstr(arm_body_label), string_cstr(next_arm_label));
                        break;
                    }
                    
                    default:
                        emit(cg, "    # TODO: pattern type %d\n", arm->pattern->type);
                        emit(cg, "    jmp %s\n", string_cstr(arm_body_label));
                }
                
                /* Arm body */
                emit(cg, "%s\n", string_cstr(arm_body_label));
                String* arm_val = codegen_expr(cg, arm->body);
                emit(cg, "    %s =w copy %s\n", string_cstr(result), string_cstr(arm_val));
                emit(cg, "    jmp %s\n", string_cstr(end_label));
                
                /* Next arm label */
                emit(cg, "%s\n", string_cstr(next_arm_label));
            }
            
            /* If we fall through all arms, return 0 (should not happen with exhaustive matching) */
            emit(cg, "    %s =w copy 0\n", string_cstr(result));
            emit(cg, "    jmp %s\n", string_cstr(end_label));
            
            emit(cg, "%s\n", string_cstr(end_label));
            return result;
        }
        
        case EXPR_CALL: {
            CallExpr* call = &expr->data.call;
            String* result = fresh_temp(cg);
            
            /* Check for module.function calls (e.g., String.len, List.get) */
            if (call->func->type == EXPR_DOT) {
                DotExpr* dot = &call->func->data.dot;
                if (dot->object->type == EXPR_IDENT) {
                    const char* module = string_cstr(dot->object->data.ident.name);
                    const char* func = string_cstr(dot->field);
                    
                    /* ===== String module ===== */
                    if (strcmp(module, "String") == 0) {
                        /* String.len(s) -> Int */
                        if (strcmp(func, "len") == 0 && call->args->len == 1) {
                            String* s = codegen_expr(cg, call->args->data[0].value);
                            emit(cg, "    %s =w call $fern_str_len(l %s)\n",
                                string_cstr(result), string_cstr(s));
                            return result;
                        }
                        /* String.concat(a, b) -> String */
                        if (strcmp(func, "concat") == 0 && call->args->len == 2) {
                            String* a = codegen_expr(cg, call->args->data[0].value);
                            String* b = codegen_expr(cg, call->args->data[1].value);
                            emit(cg, "    %s =l call $fern_str_concat(l %s, l %s)\n",
                                string_cstr(result), string_cstr(a), string_cstr(b));
                            return result;
                        }
                        /* String.eq(a, b) -> Bool */
                        if (strcmp(func, "eq") == 0 && call->args->len == 2) {
                            String* a = codegen_expr(cg, call->args->data[0].value);
                            String* b = codegen_expr(cg, call->args->data[1].value);
                            emit(cg, "    %s =w call $fern_str_eq(l %s, l %s)\n",
                                string_cstr(result), string_cstr(a), string_cstr(b));
                            return result;
                        }
                        /* String.starts_with(s, prefix) -> Bool */
                        if (strcmp(func, "starts_with") == 0 && call->args->len == 2) {
                            String* s = codegen_expr(cg, call->args->data[0].value);
                            String* prefix = codegen_expr(cg, call->args->data[1].value);
                            emit(cg, "    %s =w call $fern_str_starts_with(l %s, l %s)\n",
                                string_cstr(result), string_cstr(s), string_cstr(prefix));
                            return result;
                        }
                        /* String.ends_with(s, suffix) -> Bool */
                        if (strcmp(func, "ends_with") == 0 && call->args->len == 2) {
                            String* s = codegen_expr(cg, call->args->data[0].value);
                            String* suffix = codegen_expr(cg, call->args->data[1].value);
                            emit(cg, "    %s =w call $fern_str_ends_with(l %s, l %s)\n",
                                string_cstr(result), string_cstr(s), string_cstr(suffix));
                            return result;
                        }
                        /* String.contains(s, substr) -> Bool */
                        if (strcmp(func, "contains") == 0 && call->args->len == 2) {
                            String* s = codegen_expr(cg, call->args->data[0].value);
                            String* substr = codegen_expr(cg, call->args->data[1].value);
                            emit(cg, "    %s =w call $fern_str_contains(l %s, l %s)\n",
                                string_cstr(result), string_cstr(s), string_cstr(substr));
                            return result;
                        }
                        /* String.slice(s, start, end) -> String */
                        if (strcmp(func, "slice") == 0 && call->args->len == 3) {
                            String* s = codegen_expr(cg, call->args->data[0].value);
                            String* start = codegen_expr(cg, call->args->data[1].value);
                            String* end = codegen_expr(cg, call->args->data[2].value);
                            emit(cg, "    %s =l call $fern_str_slice(l %s, w %s, w %s)\n",
                                string_cstr(result), string_cstr(s), string_cstr(start), string_cstr(end));
                            return result;
                        }
                        /* String.trim(s) -> String */
                        if (strcmp(func, "trim") == 0 && call->args->len == 1) {
                            String* s = codegen_expr(cg, call->args->data[0].value);
                            emit(cg, "    %s =l call $fern_str_trim(l %s)\n",
                                string_cstr(result), string_cstr(s));
                            return result;
                        }
                        /* String.trim_start(s) -> String */
                        if (strcmp(func, "trim_start") == 0 && call->args->len == 1) {
                            String* s = codegen_expr(cg, call->args->data[0].value);
                            emit(cg, "    %s =l call $fern_str_trim_start(l %s)\n",
                                string_cstr(result), string_cstr(s));
                            return result;
                        }
                        /* String.trim_end(s) -> String */
                        if (strcmp(func, "trim_end") == 0 && call->args->len == 1) {
                            String* s = codegen_expr(cg, call->args->data[0].value);
                            emit(cg, "    %s =l call $fern_str_trim_end(l %s)\n",
                                string_cstr(result), string_cstr(s));
                            return result;
                        }
                        /* String.to_upper(s) -> String */
                        if (strcmp(func, "to_upper") == 0 && call->args->len == 1) {
                            String* s = codegen_expr(cg, call->args->data[0].value);
                            emit(cg, "    %s =l call $fern_str_to_upper(l %s)\n",
                                string_cstr(result), string_cstr(s));
                            return result;
                        }
                        /* String.to_lower(s) -> String */
                        if (strcmp(func, "to_lower") == 0 && call->args->len == 1) {
                            String* s = codegen_expr(cg, call->args->data[0].value);
                            emit(cg, "    %s =l call $fern_str_to_lower(l %s)\n",
                                string_cstr(result), string_cstr(s));
                            return result;
                        }
                        /* String.replace(s, old, new) -> String */
                        if (strcmp(func, "replace") == 0 && call->args->len == 3) {
                            String* s = codegen_expr(cg, call->args->data[0].value);
                            String* old_str = codegen_expr(cg, call->args->data[1].value);
                            String* new_str = codegen_expr(cg, call->args->data[2].value);
                            emit(cg, "    %s =l call $fern_str_replace(l %s, l %s, l %s)\n",
                                string_cstr(result), string_cstr(s), string_cstr(old_str), string_cstr(new_str));
                            return result;
                        }
                        /* String.repeat(s, n) -> String */
                        if (strcmp(func, "repeat") == 0 && call->args->len == 2) {
                            String* s = codegen_expr(cg, call->args->data[0].value);
                            String* n = codegen_expr(cg, call->args->data[1].value);
                            emit(cg, "    %s =l call $fern_str_repeat(l %s, w %s)\n",
                                string_cstr(result), string_cstr(s), string_cstr(n));
                            return result;
                        }
                        /* String.is_empty(s) -> Bool */
                        if (strcmp(func, "is_empty") == 0 && call->args->len == 1) {
                            String* s = codegen_expr(cg, call->args->data[0].value);
                            emit(cg, "    %s =w call $fern_str_is_empty(l %s)\n",
                                string_cstr(result), string_cstr(s));
                            return result;
                        }
                        /* String.split(s, delim) -> List(String) */
                        if (strcmp(func, "split") == 0 && call->args->len == 2) {
                            String* s = codegen_expr(cg, call->args->data[0].value);
                            String* delim = codegen_expr(cg, call->args->data[1].value);
                            emit(cg, "    %s =l call $fern_str_split(l %s, l %s)\n",
                                string_cstr(result), string_cstr(s), string_cstr(delim));
                            return result;
                        }
                        /* String.lines(s) -> List(String) */
                        if (strcmp(func, "lines") == 0 && call->args->len == 1) {
                            String* s = codegen_expr(cg, call->args->data[0].value);
                            emit(cg, "    %s =l call $fern_str_lines(l %s)\n",
                                string_cstr(result), string_cstr(s));
                            return result;
                        }
                        /* String.index_of(s, substr) -> Option(Int) */
                        if (strcmp(func, "index_of") == 0 && call->args->len == 2) {
                            String* s = codegen_expr(cg, call->args->data[0].value);
                            String* substr = codegen_expr(cg, call->args->data[1].value);
                            emit(cg, "    %s =l call $fern_str_index_of(l %s, l %s)\n",
                                string_cstr(result), string_cstr(s), string_cstr(substr));
                            return result;
                        }
                        /* String.char_at(s, index) -> Option(Int) */
                        if (strcmp(func, "char_at") == 0 && call->args->len == 2) {
                            String* s = codegen_expr(cg, call->args->data[0].value);
                            String* idx = codegen_expr(cg, call->args->data[1].value);
                            emit(cg, "    %s =l call $fern_str_char_at(l %s, w %s)\n",
                                string_cstr(result), string_cstr(s), string_cstr(idx));
                            return result;
                        }
                        /* String.join(list, sep) -> String */
                        if (strcmp(func, "join") == 0 && call->args->len == 2) {
                            String* list = codegen_expr(cg, call->args->data[0].value);
                            String* sep = codegen_expr(cg, call->args->data[1].value);
                            emit(cg, "    %s =l call $fern_str_join(l %s, l %s)\n",
                                string_cstr(result), string_cstr(list), string_cstr(sep));
                            return result;
                        }
                    }
                    
                    /* ===== List module ===== */
                    if (strcmp(module, "List") == 0) {
                        /* List.len(list) -> Int */
                        if (strcmp(func, "len") == 0 && call->args->len == 1) {
                            String* list = codegen_expr(cg, call->args->data[0].value);
                            emit(cg, "    %s =w call $fern_list_len(l %s)\n",
                                string_cstr(result), string_cstr(list));
                            return result;
                        }
                        /* List.get(list, index) -> elem */
                        if (strcmp(func, "get") == 0 && call->args->len == 2) {
                            String* list = codegen_expr(cg, call->args->data[0].value);
                            String* index = codegen_expr(cg, call->args->data[1].value);
                            emit(cg, "    %s =w call $fern_list_get(l %s, w %s)\n",
                                string_cstr(result), string_cstr(list), string_cstr(index));
                            return result;
                        }
                        /* List.push(list, elem) -> List */
                        if (strcmp(func, "push") == 0 && call->args->len == 2) {
                            String* list = codegen_expr(cg, call->args->data[0].value);
                            String* elem = codegen_expr(cg, call->args->data[1].value);
                            emit(cg, "    %s =l call $fern_list_push(l %s, w %s)\n",
                                string_cstr(result), string_cstr(list), string_cstr(elem));
                            return result;
                        }
                        /* List.reverse(list) -> List */
                        if (strcmp(func, "reverse") == 0 && call->args->len == 1) {
                            String* list = codegen_expr(cg, call->args->data[0].value);
                            emit(cg, "    %s =l call $fern_list_reverse(l %s)\n",
                                string_cstr(result), string_cstr(list));
                            return result;
                        }
                        /* List.concat(a, b) -> List */
                        if (strcmp(func, "concat") == 0 && call->args->len == 2) {
                            String* a = codegen_expr(cg, call->args->data[0].value);
                            String* b = codegen_expr(cg, call->args->data[1].value);
                            emit(cg, "    %s =l call $fern_list_concat(l %s, l %s)\n",
                                string_cstr(result), string_cstr(a), string_cstr(b));
                            return result;
                        }
                        /* List.head(list) -> elem */
                        if (strcmp(func, "head") == 0 && call->args->len == 1) {
                            String* list = codegen_expr(cg, call->args->data[0].value);
                            emit(cg, "    %s =w call $fern_list_head(l %s)\n",
                                string_cstr(result), string_cstr(list));
                            return result;
                        }
                        /* List.tail(list) -> List */
                        if (strcmp(func, "tail") == 0 && call->args->len == 1) {
                            String* list = codegen_expr(cg, call->args->data[0].value);
                            emit(cg, "    %s =l call $fern_list_tail(l %s)\n",
                                string_cstr(result), string_cstr(list));
                            return result;
                        }
                        /* List.is_empty(list) -> Bool */
                        if (strcmp(func, "is_empty") == 0 && call->args->len == 1) {
                            String* list = codegen_expr(cg, call->args->data[0].value);
                            emit(cg, "    %s =w call $fern_list_is_empty(l %s)\n",
                                string_cstr(result), string_cstr(list));
                            return result;
                        }
                    }
                    
                    /* ===== File module ===== */
                    if (strcmp(module, "File") == 0) {
                        /* File.read(path) -> Result(String, Int) */
                        if (strcmp(func, "read") == 0 && call->args->len == 1) {
                            String* path = codegen_expr(cg, call->args->data[0].value);
                            emit(cg, "    %s =l call $fern_read_file(l %s)\n",
                                string_cstr(result), string_cstr(path));
                            return result;
                        }
                        /* File.write(path, contents) -> Result(Int, Int) */
                        if (strcmp(func, "write") == 0 && call->args->len == 2) {
                            String* path = codegen_expr(cg, call->args->data[0].value);
                            String* contents = codegen_expr(cg, call->args->data[1].value);
                            emit(cg, "    %s =l call $fern_write_file(l %s, l %s)\n",
                                string_cstr(result), string_cstr(path), string_cstr(contents));
                            return result;
                        }
                        /* File.append(path, contents) -> Result(Int, Int) */
                        if (strcmp(func, "append") == 0 && call->args->len == 2) {
                            String* path = codegen_expr(cg, call->args->data[0].value);
                            String* contents = codegen_expr(cg, call->args->data[1].value);
                            emit(cg, "    %s =l call $fern_append_file(l %s, l %s)\n",
                                string_cstr(result), string_cstr(path), string_cstr(contents));
                            return result;
                        }
                        /* File.exists(path) -> Bool */
                        if (strcmp(func, "exists") == 0 && call->args->len == 1) {
                            String* path = codegen_expr(cg, call->args->data[0].value);
                            emit(cg, "    %s =w call $fern_file_exists(l %s)\n",
                                string_cstr(result), string_cstr(path));
                            return result;
                        }
                        /* File.delete(path) -> Result(Int, Int) */
                        if (strcmp(func, "delete") == 0 && call->args->len == 1) {
                            String* path = codegen_expr(cg, call->args->data[0].value);
                            emit(cg, "    %s =l call $fern_delete_file(l %s)\n",
                                string_cstr(result), string_cstr(path));
                            return result;
                        }
                        /* File.size(path) -> Result(Int, Int) */
                        if (strcmp(func, "size") == 0 && call->args->len == 1) {
                            String* path = codegen_expr(cg, call->args->data[0].value);
                            emit(cg, "    %s =l call $fern_file_size(l %s)\n",
                                string_cstr(result), string_cstr(path));
                            return result;
                        }
                        /* File.is_dir(path) -> Bool */
                        if (strcmp(func, "is_dir") == 0 && call->args->len == 1) {
                            String* path = codegen_expr(cg, call->args->data[0].value);
                            emit(cg, "    %s =w call $fern_is_dir(l %s)\n",
                                string_cstr(result), string_cstr(path));
                            return result;
                        }
                        /* File.list_dir(path) -> List(String) */
                        if (strcmp(func, "list_dir") == 0 && call->args->len == 1) {
                            String* path = codegen_expr(cg, call->args->data[0].value);
                            emit(cg, "    %s =l call $fern_list_dir(l %s)\n",
                                string_cstr(result), string_cstr(path));
                            return result;
                        }
                    }

                    /* ===== System module ===== */
                    if (strcmp(module, "System") == 0) {
                        /* System.args() -> List(String) */
                        if (strcmp(func, "args") == 0 && call->args->len == 0) {
                            emit(cg, "    %s =l call $fern_args()\n", string_cstr(result));
                            return result;
                        }
                        /* System.args_count() -> Int */
                        if (strcmp(func, "args_count") == 0 && call->args->len == 0) {
                            emit(cg, "    %s =w call $fern_args_count()\n", string_cstr(result));
                            return result;
                        }
                        /* System.arg(index) -> String */
                        if (strcmp(func, "arg") == 0 && call->args->len == 1) {
                            String* idx = codegen_expr(cg, call->args->data[0].value);
                            emit(cg, "    %s =l call $fern_arg(w %s)\n",
                                string_cstr(result), string_cstr(idx));
                            return result;
                        }
                        /* System.exit(code) -> Unit */
                        if (strcmp(func, "exit") == 0 && call->args->len == 1) {
                            String* code = codegen_expr(cg, call->args->data[0].value);
                            emit(cg, "    call $fern_exit(w %s)\n", string_cstr(code));
                            emit(cg, "    %s =w copy 0\n", string_cstr(result));
                            return result;
                        }
                    }
                }
            }
            
            /* Check for special Result constructors Ok and Err */
            if (call->func->type == EXPR_IDENT) {
                const char* fn_name = string_cstr(call->func->data.ident.name);
                
                /* Handle Ok(value) - creates a successful Result */
                if (strcmp(fn_name, "Ok") == 0 && call->args->len == 1) {
                    String* val = codegen_expr(cg, call->args->data[0].value);
                    /* Call runtime to create Ok result */
                    emit(cg, "    %s =l call $fern_result_ok(w %s)\n",
                        string_cstr(result), string_cstr(val));
                    return result;
                }
                
                /* Handle Err(error) - creates an error Result */
                if (strcmp(fn_name, "Err") == 0 && call->args->len == 1) {
                    String* err = codegen_expr(cg, call->args->data[0].value);
                    /* Call runtime to create Err result */
                    emit(cg, "    %s =l call $fern_result_err(w %s)\n",
                        string_cstr(result), string_cstr(err));
                    return result;
                }

                /* Handle print(value) - prints to stdout without newline */
                if (strcmp(fn_name, "print") == 0 && call->args->len == 1) {
                    Expr* arg = call->args->data[0].value;
                    String* val = codegen_expr(cg, arg);
                    PrintType pt = get_print_type(cg, arg);
                    switch (pt) {
                        case PRINT_STRING:
                            emit(cg, "    call $fern_print_str(l %s)\n", string_cstr(val));
                            break;
                        case PRINT_BOOL:
                            emit(cg, "    call $fern_print_bool(w %s)\n", string_cstr(val));
                            break;
                        case PRINT_INT:
                        default:
                            emit(cg, "    call $fern_print_int(w %s)\n", string_cstr(val));
                            break;
                    }
                    emit(cg, "    %s =w copy 0\n", string_cstr(result));
                    return result;
                }

                /* Handle println(value) - prints to stdout with newline */
                if (strcmp(fn_name, "println") == 0 && call->args->len == 1) {
                    Expr* arg = call->args->data[0].value;
                    String* val = codegen_expr(cg, arg);
                    PrintType pt = get_print_type(cg, arg);
                    switch (pt) {
                        case PRINT_STRING:
                            emit(cg, "    call $fern_println_str(l %s)\n", string_cstr(val));
                            break;
                        case PRINT_BOOL:
                            emit(cg, "    call $fern_println_bool(w %s)\n", string_cstr(val));
                            break;
                        case PRINT_INT:
                        default:
                            emit(cg, "    call $fern_println_int(w %s)\n", string_cstr(val));
                            break;
                    }
                    emit(cg, "    %s =w copy 0\n", string_cstr(result));
                    return result;
                }

                /* Handle str_len(s) -> Int */
                if (strcmp(fn_name, "str_len") == 0 && call->args->len == 1) {
                    String* s = codegen_expr(cg, call->args->data[0].value);
                    emit(cg, "    %s =w call $fern_str_len(l %s)\n",
                        string_cstr(result), string_cstr(s));
                    return result;
                }

                /* Handle str_concat(a, b) -> String */
                if (strcmp(fn_name, "str_concat") == 0 && call->args->len == 2) {
                    String* a = codegen_expr(cg, call->args->data[0].value);
                    String* b = codegen_expr(cg, call->args->data[1].value);
                    emit(cg, "    %s =l call $fern_str_concat(l %s, l %s)\n",
                        string_cstr(result), string_cstr(a), string_cstr(b));
                    return result;
                }

                /* Handle str_eq(a, b) -> Bool */
                if (strcmp(fn_name, "str_eq") == 0 && call->args->len == 2) {
                    String* a = codegen_expr(cg, call->args->data[0].value);
                    String* b = codegen_expr(cg, call->args->data[1].value);
                    emit(cg, "    %s =w call $fern_str_eq(l %s, l %s)\n",
                        string_cstr(result), string_cstr(a), string_cstr(b));
                    return result;
                }

                /* Handle list_len(list) -> Int */
                if (strcmp(fn_name, "list_len") == 0 && call->args->len == 1) {
                    String* list = codegen_expr(cg, call->args->data[0].value);
                    emit(cg, "    %s =w call $fern_list_len(l %s)\n",
                        string_cstr(result), string_cstr(list));
                    return result;
                }

                /* Handle list_get(list, index) -> elem */
                if (strcmp(fn_name, "list_get") == 0 && call->args->len == 2) {
                    String* list = codegen_expr(cg, call->args->data[0].value);
                    String* index = codegen_expr(cg, call->args->data[1].value);
                    emit(cg, "    %s =w call $fern_list_get(l %s, w %s)\n",
                        string_cstr(result), string_cstr(list), string_cstr(index));
                    return result;
                }

                /* ===== Additional String Functions ===== */

                /* Handle str_starts_with(s, prefix) -> Bool */
                if (strcmp(fn_name, "str_starts_with") == 0 && call->args->len == 2) {
                    String* s = codegen_expr(cg, call->args->data[0].value);
                    String* prefix = codegen_expr(cg, call->args->data[1].value);
                    emit(cg, "    %s =w call $fern_str_starts_with(l %s, l %s)\n",
                        string_cstr(result), string_cstr(s), string_cstr(prefix));
                    return result;
                }

                /* Handle str_ends_with(s, suffix) -> Bool */
                if (strcmp(fn_name, "str_ends_with") == 0 && call->args->len == 2) {
                    String* s = codegen_expr(cg, call->args->data[0].value);
                    String* suffix = codegen_expr(cg, call->args->data[1].value);
                    emit(cg, "    %s =w call $fern_str_ends_with(l %s, l %s)\n",
                        string_cstr(result), string_cstr(s), string_cstr(suffix));
                    return result;
                }

                /* Handle str_contains(s, substr) -> Bool */
                if (strcmp(fn_name, "str_contains") == 0 && call->args->len == 2) {
                    String* s = codegen_expr(cg, call->args->data[0].value);
                    String* substr = codegen_expr(cg, call->args->data[1].value);
                    emit(cg, "    %s =w call $fern_str_contains(l %s, l %s)\n",
                        string_cstr(result), string_cstr(s), string_cstr(substr));
                    return result;
                }

                /* Handle str_slice(s, start, end) -> String */
                if (strcmp(fn_name, "str_slice") == 0 && call->args->len == 3) {
                    String* s = codegen_expr(cg, call->args->data[0].value);
                    String* start = codegen_expr(cg, call->args->data[1].value);
                    String* end = codegen_expr(cg, call->args->data[2].value);
                    emit(cg, "    %s =l call $fern_str_slice(l %s, w %s, w %s)\n",
                        string_cstr(result), string_cstr(s), string_cstr(start), string_cstr(end));
                    return result;
                }

                /* Handle str_trim(s) -> String */
                if (strcmp(fn_name, "str_trim") == 0 && call->args->len == 1) {
                    String* s = codegen_expr(cg, call->args->data[0].value);
                    emit(cg, "    %s =l call $fern_str_trim(l %s)\n",
                        string_cstr(result), string_cstr(s));
                    return result;
                }

                /* Handle str_trim_start(s) -> String */
                if (strcmp(fn_name, "str_trim_start") == 0 && call->args->len == 1) {
                    String* s = codegen_expr(cg, call->args->data[0].value);
                    emit(cg, "    %s =l call $fern_str_trim_start(l %s)\n",
                        string_cstr(result), string_cstr(s));
                    return result;
                }

                /* Handle str_trim_end(s) -> String */
                if (strcmp(fn_name, "str_trim_end") == 0 && call->args->len == 1) {
                    String* s = codegen_expr(cg, call->args->data[0].value);
                    emit(cg, "    %s =l call $fern_str_trim_end(l %s)\n",
                        string_cstr(result), string_cstr(s));
                    return result;
                }

                /* Handle str_to_upper(s) -> String */
                if (strcmp(fn_name, "str_to_upper") == 0 && call->args->len == 1) {
                    String* s = codegen_expr(cg, call->args->data[0].value);
                    emit(cg, "    %s =l call $fern_str_to_upper(l %s)\n",
                        string_cstr(result), string_cstr(s));
                    return result;
                }

                /* Handle str_to_lower(s) -> String */
                if (strcmp(fn_name, "str_to_lower") == 0 && call->args->len == 1) {
                    String* s = codegen_expr(cg, call->args->data[0].value);
                    emit(cg, "    %s =l call $fern_str_to_lower(l %s)\n",
                        string_cstr(result), string_cstr(s));
                    return result;
                }

                /* Handle str_replace(s, old, new) -> String */
                if (strcmp(fn_name, "str_replace") == 0 && call->args->len == 3) {
                    String* s = codegen_expr(cg, call->args->data[0].value);
                    String* old_str = codegen_expr(cg, call->args->data[1].value);
                    String* new_str = codegen_expr(cg, call->args->data[2].value);
                    emit(cg, "    %s =l call $fern_str_replace(l %s, l %s, l %s)\n",
                        string_cstr(result), string_cstr(s), string_cstr(old_str), string_cstr(new_str));
                    return result;
                }

                /* Handle str_repeat(s, n) -> String */
                if (strcmp(fn_name, "str_repeat") == 0 && call->args->len == 2) {
                    String* s = codegen_expr(cg, call->args->data[0].value);
                    String* n = codegen_expr(cg, call->args->data[1].value);
                    emit(cg, "    %s =l call $fern_str_repeat(l %s, w %s)\n",
                        string_cstr(result), string_cstr(s), string_cstr(n));
                    return result;
                }

                /* Handle str_is_empty(s) -> Bool */
                if (strcmp(fn_name, "str_is_empty") == 0 && call->args->len == 1) {
                    String* s = codegen_expr(cg, call->args->data[0].value);
                    emit(cg, "    %s =w call $fern_str_is_empty(l %s)\n",
                        string_cstr(result), string_cstr(s));
                    return result;
                }

                /* ===== Additional List Functions ===== */

                /* Handle list_push(list, elem) -> List */
                if (strcmp(fn_name, "list_push") == 0 && call->args->len == 2) {
                    String* list = codegen_expr(cg, call->args->data[0].value);
                    String* elem = codegen_expr(cg, call->args->data[1].value);
                    emit(cg, "    %s =l call $fern_list_push(l %s, w %s)\n",
                        string_cstr(result), string_cstr(list), string_cstr(elem));
                    return result;
                }

                /* Handle list_reverse(list) -> List */
                if (strcmp(fn_name, "list_reverse") == 0 && call->args->len == 1) {
                    String* list = codegen_expr(cg, call->args->data[0].value);
                    emit(cg, "    %s =l call $fern_list_reverse(l %s)\n",
                        string_cstr(result), string_cstr(list));
                    return result;
                }

                /* Handle list_concat(a, b) -> List */
                if (strcmp(fn_name, "list_concat") == 0 && call->args->len == 2) {
                    String* a = codegen_expr(cg, call->args->data[0].value);
                    String* b = codegen_expr(cg, call->args->data[1].value);
                    emit(cg, "    %s =l call $fern_list_concat(l %s, l %s)\n",
                        string_cstr(result), string_cstr(a), string_cstr(b));
                    return result;
                }

                /* Handle list_head(list) -> elem */
                if (strcmp(fn_name, "list_head") == 0 && call->args->len == 1) {
                    String* list = codegen_expr(cg, call->args->data[0].value);
                    emit(cg, "    %s =w call $fern_list_head(l %s)\n",
                        string_cstr(result), string_cstr(list));
                    return result;
                }

                /* Handle list_tail(list) -> List */
                if (strcmp(fn_name, "list_tail") == 0 && call->args->len == 1) {
                    String* list = codegen_expr(cg, call->args->data[0].value);
                    emit(cg, "    %s =l call $fern_list_tail(l %s)\n",
                        string_cstr(result), string_cstr(list));
                    return result;
                }

                /* Handle list_is_empty(list) -> Bool */
                if (strcmp(fn_name, "list_is_empty") == 0 && call->args->len == 1) {
                    String* list = codegen_expr(cg, call->args->data[0].value);
                    emit(cg, "    %s =w call $fern_list_is_empty(l %s)\n",
                        string_cstr(result), string_cstr(list));
                    return result;
                }

                /* ===== File I/O Functions ===== */

                /* Handle read_file(path) -> Result(String, Int) */
                if (strcmp(fn_name, "read_file") == 0 && call->args->len == 1) {
                    String* path = codegen_expr(cg, call->args->data[0].value);
                    emit(cg, "    %s =l call $fern_read_file(l %s)\n",
                        string_cstr(result), string_cstr(path));
                    return result;
                }

                /* Handle write_file(path, contents) -> Result(Int, Int) */
                if (strcmp(fn_name, "write_file") == 0 && call->args->len == 2) {
                    String* path = codegen_expr(cg, call->args->data[0].value);
                    String* contents = codegen_expr(cg, call->args->data[1].value);
                    emit(cg, "    %s =l call $fern_write_file(l %s, l %s)\n",
                        string_cstr(result), string_cstr(path), string_cstr(contents));
                    return result;
                }

                /* Handle append_file(path, contents) -> Result(Int, Int) */
                if (strcmp(fn_name, "append_file") == 0 && call->args->len == 2) {
                    String* path = codegen_expr(cg, call->args->data[0].value);
                    String* contents = codegen_expr(cg, call->args->data[1].value);
                    emit(cg, "    %s =l call $fern_append_file(l %s, l %s)\n",
                        string_cstr(result), string_cstr(path), string_cstr(contents));
                    return result;
                }

                /* Handle file_exists(path) -> Bool */
                if (strcmp(fn_name, "file_exists") == 0 && call->args->len == 1) {
                    String* path = codegen_expr(cg, call->args->data[0].value);
                    emit(cg, "    %s =w call $fern_file_exists(l %s)\n",
                        string_cstr(result), string_cstr(path));
                    return result;
                }

                /* Handle delete_file(path) -> Result(Int, Int) */
                if (strcmp(fn_name, "delete_file") == 0 && call->args->len == 1) {
                    String* path = codegen_expr(cg, call->args->data[0].value);
                    emit(cg, "    %s =l call $fern_delete_file(l %s)\n",
                        string_cstr(result), string_cstr(path));
                    return result;
                }

                /* Handle file_size(path) -> Result(Int, Int) */
                if (strcmp(fn_name, "file_size") == 0 && call->args->len == 1) {
                    String* path = codegen_expr(cg, call->args->data[0].value);
                    emit(cg, "    %s =l call $fern_file_size(l %s)\n",
                        string_cstr(result), string_cstr(path));
                    return result;
                }
            }

            /* Generate code for arguments */
            String** arg_temps = NULL;
            if (call->args->len > 0) {
                arg_temps = arena_alloc(cg->arena, sizeof(String*) * call->args->len);
                for (size_t i = 0; i < call->args->len; i++) {
                    arg_temps[i] = codegen_expr(cg, call->args->data[i].value);
                }
            }
            
            /* Generate call */
            /* For now, assume func is a simple identifier */
            if (call->func->type == EXPR_IDENT) {
                emit(cg, "    %s =w call $%s(", 
                    string_cstr(result), 
                    string_cstr(call->func->data.ident.name));
                for (size_t i = 0; i < call->args->len; i++) {
                    if (i > 0) emit(cg, ", ");
                    emit(cg, "w %s", string_cstr(arg_temps[i]));
                }
                emit(cg, ")\n");
            } else {
                emit(cg, "    # TODO: indirect call\n");
            }
            
            return result;
        }
        
        case EXPR_TUPLE: {
            TupleExpr* tuple = &expr->data.tuple;
            
            /* Generate code for each element */
            for (size_t i = 0; i < tuple->elements->len; i++) {
                codegen_expr(cg, tuple->elements->data[i]);
            }
            
            /* For now, return a placeholder - full tuple support needs stack allocation */
            String* tmp = fresh_temp(cg);
            emit(cg, "    %s =w copy 0  # tuple placeholder\n", string_cstr(tmp));
            return tmp;
        }
        
        case EXPR_LIST: {
            ListExpr* list = &expr->data.list;
            String* list_ptr = fresh_temp(cg);
            
            /* Create a new list via runtime */
            emit(cg, "    %s =l call $fern_list_new()\n", string_cstr(list_ptr));
            
            /* Push each element to the list (mutating for construction) */
            for (size_t i = 0; i < list->elements->len; i++) {
                String* elem = codegen_expr(cg, list->elements->data[i]);
                emit(cg, "    call $fern_list_push_mut(l %s, w %s)\n",
                    string_cstr(list_ptr), string_cstr(elem));
            }
            
            return list_ptr;
        }
        
        case EXPR_LAMBDA: {
            LambdaExpr* lambda = &expr->data.lambda;
            
            /* Generate a unique function name for this lambda */
            int lambda_id = cg->string_counter++;
            char fn_name_buf[64];
            snprintf(fn_name_buf, sizeof(fn_name_buf), "$lambda%d", lambda_id);
            
            /* Save current output and start new function */
            String* saved_output = cg->output;
            cg->output = string_new(cg->arena, "");
            
            /* Function header */
            emit(cg, "function w %s(", fn_name_buf);
            
            /* Parameters */
            if (lambda->params) {
                for (size_t i = 0; i < lambda->params->len; i++) {
                    if (i > 0) emit(cg, ", ");
                    emit(cg, "w %%%s", string_cstr(lambda->params->data[i]));
                }
            }
            
            emit(cg, ") {\n");
            emit(cg, "@start\n");
            
            /* Function body */
            String* result = codegen_expr(cg, lambda->body);
            emit(cg, "    ret %s\n", string_cstr(result));
            
            emit(cg, "}\n\n");
            
            /* Prepend the lambda function to saved output */
            String* lambda_fn = cg->output;
            cg->output = string_concat(cg->arena, lambda_fn, saved_output);
            
            /* Return a pointer to the function */
            String* tmp = fresh_temp(cg);
            emit(cg, "    %s =l copy %s\n", string_cstr(tmp), fn_name_buf);
            return tmp;
        }
        
        case EXPR_TRY: {
            /* The ? operator: unwrap Result or return early with error
             * 
             * Generated code pattern:
             *   result = <operand>           # Get the Result value
             *   tag = load result            # Load the tag (offset 0)
             *   jnz tag, @err, @ok           # Branch on tag (0=Ok, 1=Err)
             * @err
             *   ret result                   # Return the error as-is
             * @ok
             *   value = load result+8        # Load the value (offset 8)
             *   ... continue with value ...
             */
            TryExpr* try_expr = &expr->data.try_expr;
            
            /* Generate code for the operand (returns a Result pointer or struct) */
            String* result_val = codegen_expr(cg, try_expr->operand);
            
            /* For now, we'll call runtime functions to check and unwrap
             * This assumes Result is passed as a pointer */
            String* is_ok = fresh_temp(cg);
            String* ok_label = fresh_label(cg);
            String* err_label = fresh_label(cg);
            String* unwrapped = fresh_temp(cg);
            
            /* Call fern_result_is_ok to check the tag */
            emit(cg, "    %s =w call $fern_result_is_ok(l %s)\n", 
                string_cstr(is_ok), string_cstr(result_val));
            
            /* Branch: if Ok continue, if Err return early */
            emit(cg, "    jnz %s, %s, %s\n", 
                string_cstr(is_ok), string_cstr(ok_label), string_cstr(err_label));
            
            /* Error path: return the error */
            emit(cg, "%s\n", string_cstr(err_label));
            emit(cg, "    ret %s\n", string_cstr(result_val));
            
            /* Ok path: unwrap the value */
            emit(cg, "%s\n", string_cstr(ok_label));
            emit(cg, "    %s =w call $fern_result_unwrap(l %s)\n",
                string_cstr(unwrapped), string_cstr(result_val));
            
            return unwrapped;
        }
        
        case EXPR_INDEX: {
            /* Index expression: list[index]
             * Calls fern_list_get(list, index) */
            IndexExpr* idx = &expr->data.index_expr;
            
            String* obj = codegen_expr(cg, idx->object);
            String* index = codegen_expr(cg, idx->index);
            String* result = fresh_temp(cg);
            
            /* Call runtime function to get element */
            emit(cg, "    %s =w call $fern_list_get(l %s, w %s)\n",
                string_cstr(result), string_cstr(obj), string_cstr(index));
            
            return result;
        }
        
        case EXPR_FOR: {
            /* For loop: for var_name in iterable: body
             * 
             * Generated code pattern:
             *   list = <iterable>
             *   len = fern_list_len(list)
             *   idx = 0
             * @loop_start
             *   cond = idx < len
             *   jnz cond, @loop_body, @loop_end
             * @loop_body
             *   var_name = fern_list_get(list, idx)
             *   <body>
             *   idx = idx + 1
             *   jmp @loop_start
             * @loop_end
             *   result = 0  (unit value)
             */
            ForExpr* for_loop = &expr->data.for_loop;
            
            /* Generate code for the iterable (list) */
            String* list = codegen_expr(cg, for_loop->iterable);
            
            /* Get list length */
            String* len = fresh_temp(cg);
            emit(cg, "    %s =w call $fern_list_len(l %s)\n", 
                string_cstr(len), string_cstr(list));
            
            /* Initialize index to 0 */
            String* idx = fresh_temp(cg);
            emit(cg, "    %s =w copy 0\n", string_cstr(idx));
            
            /* Generate labels */
            String* loop_start = fresh_label(cg);
            String* loop_body = fresh_label(cg);
            String* loop_end = fresh_label(cg);
            
            /* Loop start: check condition */
            emit(cg, "%s\n", string_cstr(loop_start));
            String* cond = fresh_temp(cg);
            emit(cg, "    %s =w csltw %s, %s\n", 
                string_cstr(cond), string_cstr(idx), string_cstr(len));
            emit(cg, "    jnz %s, %s, %s\n", 
                string_cstr(cond), string_cstr(loop_body), string_cstr(loop_end));
            
            /* Loop body */
            emit(cg, "%s\n", string_cstr(loop_body));
            
            /* Get element at current index and bind to var_name */
            String* elem = fresh_temp(cg);
            emit(cg, "    %s =w call $fern_list_get(l %s, w %s)\n",
                string_cstr(elem), string_cstr(list), string_cstr(idx));
            emit(cg, "    %%%s =w copy %s\n", 
                string_cstr(for_loop->var_name), string_cstr(elem));
            
            /* Execute body */
            codegen_expr(cg, for_loop->body);
            
            /* Increment index */
            String* new_idx = fresh_temp(cg);
            emit(cg, "    %s =w add %s, 1\n", string_cstr(new_idx), string_cstr(idx));
            emit(cg, "    %s =w copy %s\n", string_cstr(idx), string_cstr(new_idx));
            
            /* Jump back to loop start */
            emit(cg, "    jmp %s\n", string_cstr(loop_start));
            
            /* Loop end: return unit value */
            emit(cg, "%s\n", string_cstr(loop_end));
            String* result = fresh_temp(cg);
            emit(cg, "    %s =w copy 0\n", string_cstr(result));
            
            return result;
        }
        
        case EXPR_WITH: {
            /* With expression: with x <- f(), y <- g(x) do body [else arms]
             *
             * Generated code pattern for each binding:
             *   result = f()                  # Get Result value
             *   is_ok = fern_result_is_ok(result)
             *   jnz is_ok, @ok_label, @err_label
             * @err_label
             *   # Error path: return error or match else arms
             *   ret result (or handle else arms)
             * @ok_label
             *   x = fern_result_unwrap(result)
             *   ... continue to next binding or body ...
             */
            WithExpr* with = &expr->data.with_expr;
            String* result = fresh_temp(cg);
            String* err_label = fresh_label(cg);
            String* end_label = fresh_label(cg);
            
            /* Process each binding */
            for (size_t i = 0; i < with->bindings->len; i++) {
                WithBinding* binding = &with->bindings->data[i];
                String* ok_label = fresh_label(cg);
                
                /* Evaluate the binding's value (returns a Result) */
                String* res_val = codegen_expr(cg, binding->value);
                
                /* Check if Ok */
                String* is_ok = fresh_temp(cg);
                emit(cg, "    %s =w call $fern_result_is_ok(l %s)\n",
                    string_cstr(is_ok), string_cstr(res_val));
                
                /* Branch: if Err, jump to error handling */
                emit(cg, "    jnz %s, %s, %s\n",
                    string_cstr(is_ok), string_cstr(ok_label), string_cstr(err_label));
                
                /* Ok path: unwrap and bind */
                emit(cg, "%s\n", string_cstr(ok_label));
                String* unwrapped = fresh_temp(cg);
                emit(cg, "    %s =w call $fern_result_unwrap(l %s)\n",
                    string_cstr(unwrapped), string_cstr(res_val));
                emit(cg, "    %%%s =w copy %s\n",
                    string_cstr(binding->name), string_cstr(unwrapped));
            }
            
            /* All bindings succeeded: evaluate do body */
            String* body_val = codegen_expr(cg, with->body);
            emit(cg, "    %s =w copy %s\n", string_cstr(result), string_cstr(body_val));
            emit(cg, "    jmp %s\n", string_cstr(end_label));
            
            /* Error path */
            emit(cg, "%s\n", string_cstr(err_label));
            if (with->else_arms && with->else_arms->len > 0) {
                /* TODO: Match else arms against error */
                emit(cg, "    # TODO: else arm matching\n");
                emit(cg, "    %s =w copy 0\n", string_cstr(result));
            } else {
                /* No else clause: return the error as-is */
                emit(cg, "    %s =w copy 0\n", string_cstr(result));
            }
            
            emit(cg, "%s\n", string_cstr(end_label));
            return result;
        }
            
        default:
            emit(cg, "    # TODO: codegen for expr type %d\n", expr->type);
            return fresh_temp(cg);
    }
}

/* ========== Statement Code Generation ========== */

/**
 * Generate code for a function definition.
 * @param cg The codegen context.
 * @param fn The function definition.
 */
static void codegen_fn_def(Codegen* cg, FunctionDef* fn) {
    assert(cg != NULL);
    assert(fn != NULL);
    
    /* Clear defer stack at function start */
    clear_defers(cg);
    
    /* Check if this is main() with no return type (Unit return) */
    const char* fn_name = string_cstr(fn->name);
    bool is_main = (strcmp(fn_name, "main") == 0);
    bool is_main_unit = is_main && (fn->return_type == NULL);
    
    /* Function header - rename main to fern_main so C runtime can provide entry point */
    const char* emit_name = is_main ? "fern_main" : fn_name;
    emit(cg, "export function w $%s(", emit_name);
    
    /* Parameters */
    if (fn->params) {
        for (size_t i = 0; i < fn->params->len; i++) {
            if (i > 0) emit(cg, ", ");
            emit(cg, "w %%%s", string_cstr(fn->params->data[i].name));
        }
    }
    
    emit(cg, ") {\n");
    emit(cg, "@start\n");
    
    /* Function body */
    String* result = codegen_expr(cg, fn->body);
    
    /* Emit deferred expressions before final return */
    emit_defers(cg);
    
    /* For main() with Unit return, always return 0 (success exit code) */
    if (is_main_unit) {
        emit(cg, "    ret 0\n");
    } else {
        emit(cg, "    ret %s\n", string_cstr(result));
    }
    
    emit(cg, "}\n\n");
}

/**
 * Generate QBE IR code for a statement.
 * @param cg The codegen context.
 * @param stmt The statement to compile.
 */
void codegen_stmt(Codegen* cg, Stmt* stmt) {
    assert(cg != NULL);
    assert(stmt != NULL);
    
    switch (stmt->type) {
        case STMT_LET: {
            LetStmt* let = &stmt->data.let;
            String* val = codegen_expr(cg, let->value);
            
            /* Determine QBE type based on the value expression */
            char type_spec = qbe_type_for_expr(let->value);
            
            /* For simple identifier patterns, just copy to a named local */
            if (let->pattern->type == PATTERN_IDENT) {
                /* Register wide variables for later reference */
                if (type_spec == 'l') {
                    register_wide_var(cg, let->pattern->data.ident);
                }
                emit(cg, "    %%%s =%c copy %s\n", 
                    string_cstr(let->pattern->data.ident), type_spec, string_cstr(val));
            } else {
                emit(cg, "    # TODO: pattern binding\n");
            }
            break;
        }
        
        case STMT_EXPR: {
            codegen_expr(cg, stmt->data.expr.expr);
            break;
        }
        
        case STMT_RETURN: {
            ReturnStmt* ret = &stmt->data.return_stmt;
            /* Emit deferred expressions before returning */
            emit_defers(cg);
            if (ret->value) {
                String* val = codegen_expr(cg, ret->value);
                emit(cg, "    ret %s\n", string_cstr(val));
            } else {
                emit(cg, "    ret 0\n");
            }
            break;
        }
        
        case STMT_DEFER: {
            /* Push the deferred expression onto the stack - it will be
             * emitted before any return (LIFO order) */
            DeferStmt* defer = &stmt->data.defer_stmt;
            push_defer(cg, defer->expr);
            break;
        }
        
        case STMT_FN: {
            codegen_fn_def(cg, &stmt->data.fn);
            break;
        }
        
        default:
            emit(cg, "# TODO: codegen for stmt type %d\n", stmt->type);
    }
}

/* ========== Program Code Generation ========== */

/**
 * Generate code for a complete program.
 * @param cg The codegen context.
 * @param stmts The program statements.
 */
void codegen_program(Codegen* cg, StmtVec* stmts) {
    assert(cg != NULL);
    assert(cg->arena != NULL);
    
    if (!stmts) return;
    
    for (size_t i = 0; i < stmts->len; i++) {
        codegen_stmt(cg, stmts->data[i]);
    }
}

/* ========== Output ========== */

/**
 * Get the generated output as a string.
 * @param cg The codegen context.
 * @return The generated QBE IR.
 */
String* codegen_output(Codegen* cg) {
    assert(cg != NULL);
    assert(cg->output != NULL);
    /* Combine functions and data section */
    return string_concat(cg->arena, cg->output, cg->data_section);
}

/**
 * Write the generated code to a file.
 * @param cg The codegen context.
 * @param filename The output filename.
 * @return True on success.
 */
bool codegen_write(Codegen* cg, const char* filename) {
    assert(cg != NULL);
    assert(filename != NULL);
    
    FILE* f = fopen(filename, "w");
    if (!f) return false;
    
    /* Write combined output (functions + data section) */
    String* full_output = codegen_output(cg);
    fprintf(f, "%s", string_cstr(full_output));
    fclose(f);
    return true;
}

/**
 * Emit the generated code to a file stream.
 * @param cg The codegen context.
 * @param out The output file stream.
 */
void codegen_emit(Codegen* cg, FILE* out) {
    assert(cg != NULL);
    assert(out != NULL);
    
    /* Write combined output (functions + data section) */
    String* full_output = codegen_output(cg);
    fprintf(out, "%s", string_cstr(full_output));
}
