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
};

/* ========== Helper Functions ========== */

/* Append text to the output buffer */
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

/* Generate a fresh temporary name */
static String* fresh_temp(Codegen* cg) {
    assert(cg != NULL);
    assert(cg->temp_counter >= 0);
    char buf[32];
    snprintf(buf, sizeof(buf), "%%t%d", cg->temp_counter++);
    return string_new(cg->arena, buf);
}

/* Generate a fresh label name */
static String* fresh_label(Codegen* cg) {
    assert(cg != NULL);
    assert(cg->label_counter >= 0);
    char buf[32];
    snprintf(buf, sizeof(buf), "@L%d", cg->label_counter++);
    return string_new(cg->arena, buf);
}

/* ========== Codegen Creation ========== */

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
    return cg;
}

/* Push a deferred expression onto the stack */
static void push_defer(Codegen* cg, Expr* expr) {
    assert(cg != NULL);
    assert(expr != NULL);
    assert(cg->defer_count < MAX_DEFERS);
    cg->defer_stack[cg->defer_count++] = expr;
}

/* Emit all deferred expressions in reverse order (LIFO) */
static void emit_defers(Codegen* cg) {
    assert(cg != NULL);
    /* Emit in reverse order: last defer runs first */
    for (int i = cg->defer_count - 1; i >= 0; i--) {
        codegen_expr(cg, cg->defer_stack[i]);
    }
}

/* Clear the defer stack (called at function boundaries) */
static void clear_defers(Codegen* cg) {
    assert(cg != NULL);
    cg->defer_count = 0;
}

/* Emit to data section */
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

/* Generate a fresh string label */
static String* fresh_string_label(Codegen* cg) {
    assert(cg != NULL);
    assert(cg->string_counter >= 0);
    char buf[32];
    snprintf(buf, sizeof(buf), "$str%d", cg->string_counter++);
    return string_new(cg->arena, buf);
}

/* ========== Expression Code Generation ========== */

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
            emit(cg, "    %s =w copy %%%s\n", string_cstr(tmp), string_cstr(expr->data.ident.name));
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
            
            /* Push each element to the list */
            for (size_t i = 0; i < list->elements->len; i++) {
                String* elem = codegen_expr(cg, list->elements->data[i]);
                emit(cg, "    call $fern_list_push(l %s, w %s)\n",
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
            
        default:
            emit(cg, "    # TODO: codegen for expr type %d\n", expr->type);
            return fresh_temp(cg);
    }
}

/* ========== Statement Code Generation ========== */

void codegen_stmt(Codegen* cg, Stmt* stmt) {
    assert(cg != NULL);
    assert(stmt != NULL);
    
    switch (stmt->type) {
        case STMT_LET: {
            LetStmt* let = &stmt->data.let;
            String* val = codegen_expr(cg, let->value);
            
            /* For simple identifier patterns, just copy to a named local */
            if (let->pattern->type == PATTERN_IDENT) {
                emit(cg, "    %%%s =w copy %s\n", 
                    string_cstr(let->pattern->data.ident), string_cstr(val));
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
            FunctionDef* fn = &stmt->data.fn;
            
            /* Clear defer stack at function start */
            clear_defers(cg);
            
            /* Function header */
            emit(cg, "export function w $%s(", string_cstr(fn->name));
            
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
            emit(cg, "    ret %s\n", string_cstr(result));
            
            emit(cg, "}\n\n");
            break;
        }
        
        default:
            emit(cg, "# TODO: codegen for stmt type %d\n", stmt->type);
    }
}

/* ========== Program Code Generation ========== */

void codegen_program(Codegen* cg, StmtVec* stmts) {
    assert(cg != NULL);
    assert(cg->arena != NULL);
    
    if (!stmts) return;
    
    for (size_t i = 0; i < stmts->len; i++) {
        codegen_stmt(cg, stmts->data[i]);
    }
}

/* ========== Output ========== */

String* codegen_output(Codegen* cg) {
    assert(cg != NULL);
    assert(cg->output != NULL);
    /* Combine functions and data section */
    return string_concat(cg->arena, cg->output, cg->data_section);
}

bool codegen_write(Codegen* cg, const char* filename) {
    assert(cg != NULL);
    assert(filename != NULL);
    
    FILE* f = fopen(filename, "w");
    if (!f) return false;
    
    fprintf(f, "%s", string_cstr(cg->output));
    fclose(f);
    return true;
}

void codegen_emit(Codegen* cg, FILE* out) {
    assert(cg != NULL);
    assert(out != NULL);
    
    fprintf(out, "%s", string_cstr(cg->output));
}
