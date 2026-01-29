/* AST Pretty-Printer Implementation */

#include "ast_print.h"
#include <assert.h>
#include <string.h>

#define MAX_INDENT 100  /* Prevent infinite recursion. */

/**
 * Print indentation spaces.
 * @param out The output file.
 * @param indent The indentation level.
 */
static void print_indent(FILE* out, int indent) {
    assert(out != NULL);
    assert(indent >= 0 && indent < MAX_INDENT);
    for (int i = 0; i < indent; i++) {
        fprintf(out, "  ");
    }
}

/* Forward declarations. */
void ast_print_expr(FILE* out, Expr* expr, int indent);
void ast_print_stmt(FILE* out, Stmt* stmt, int indent);

/**
 * Get the string name for a binary operator.
 * @param op The binary operator.
 * @return The operator name string.
 */
static const char* binop_name(BinaryOp op) {
    assert(op >= BINOP_ADD);
    assert(op <= BINOP_IN);
    switch (op) {
        case BINOP_ADD: return "+";
        case BINOP_SUB: return "-";
        case BINOP_MUL: return "*";
        case BINOP_DIV: return "/";
        case BINOP_MOD: return "%";
        case BINOP_POW: return "**";
        case BINOP_EQ:  return "==";
        case BINOP_NE:  return "!=";
        case BINOP_LT:  return "<";
        case BINOP_LE:  return "<=";
        case BINOP_GT:  return ">";
        case BINOP_GE:  return ">=";
        case BINOP_AND: return "and";
        case BINOP_OR:  return "or";
        case BINOP_PIPE: return "|>";
        case BINOP_IN:   return "in";
        default: return "?";
    }
}

/**
 * Get the string name for a unary operator.
 * @param op The unary operator.
 * @return The operator name string.
 */
static const char* unop_name(UnaryOp op) {
    assert(op >= UNOP_NEG);
    assert(op <= UNOP_NOT);
    switch (op) {
        case UNOP_NEG: return "-";
        case UNOP_NOT: return "not";
        default: return "?";
    }
}

/**
 * Print a type expression.
 * @param out The output file.
 * @param type The type expression to print.
 * @param indent The indentation level.
 */
void ast_print_type(FILE* out, TypeExpr* type, int indent) {
    assert(out != NULL);
    assert(indent >= 0 && indent < MAX_INDENT);
    
    if (!type) {
        print_indent(out, indent);
        fprintf(out, "(null)\n");
        return;
    }
    
    print_indent(out, indent);
    
    switch (type->kind) {
        case TYPEEXPR_NAMED:
            fprintf(out, "Type: %s", string_cstr(type->data.named.name));
            if (type->data.named.args && type->data.named.args->len > 0) {
                fprintf(out, "(");
                for (size_t i = 0; i < type->data.named.args->len; i++) {
                    if (i > 0) fprintf(out, ", ");
                    TypeExpr* arg = type->data.named.args->data[i];
                    fprintf(out, "%s", string_cstr(arg->data.named.name));
                }
                fprintf(out, ")");
            }
            fprintf(out, "\n");
            break;
            
        case TYPEEXPR_FUNCTION:
            fprintf(out, "FnType: (");
            for (size_t i = 0; i < type->data.func.params->len; i++) {
                if (i > 0) fprintf(out, ", ");
                TypeExpr* param = type->data.func.params->data[i];
                fprintf(out, "%s", string_cstr(param->data.named.name));
            }
            fprintf(out, ") -> ");
            fprintf(out, "%s\n", string_cstr(type->data.func.return_type->data.named.name));
            break;
            
        case TYPEEXPR_TUPLE:
            fprintf(out, "TupleType: (");
            for (size_t i = 0; i < type->data.tuple.elements->len; i++) {
                if (i > 0) fprintf(out, ", ");
                TypeExpr* elem = type->data.tuple.elements->data[i];
                if (elem->kind == TYPEEXPR_NAMED) {
                    fprintf(out, "%s", string_cstr(elem->data.named.name));
                } else {
                    fprintf(out, "...");
                }
            }
            fprintf(out, ")\n");
            break;
    }
}

/**
 * Print a pattern.
 * @param out The output file.
 * @param pattern The pattern to print.
 * @param indent The indentation level.
 */
void ast_print_pattern(FILE* out, Pattern* pattern, int indent) {
    assert(out != NULL);
    assert(indent >= 0 && indent < MAX_INDENT);
    
    if (!pattern) {
        print_indent(out, indent);
        fprintf(out, "(null)\n");
        return;
    }
    
    print_indent(out, indent);
    
    switch (pattern->type) {
        case PATTERN_IDENT:
            fprintf(out, "PatIdent: %s\n", string_cstr(pattern->data.ident));
            break;
            
        case PATTERN_WILDCARD:
            fprintf(out, "PatWildcard: _\n");
            break;
            
        case PATTERN_LIT:
            fprintf(out, "PatLit:\n");
            ast_print_expr(out, pattern->data.literal, indent + 1);
            break;
            
        case PATTERN_CONSTRUCTOR:
            fprintf(out, "PatConstructor: %s\n", string_cstr(pattern->data.constructor.name));
            for (size_t i = 0; i < pattern->data.constructor.args->len; i++) {
                ast_print_pattern(out, pattern->data.constructor.args->data[i], indent + 1);
            }
            break;
            
        case PATTERN_TUPLE:
            fprintf(out, "PatTuple:\n");
            for (size_t i = 0; i < pattern->data.tuple->len; i++) {
                ast_print_pattern(out, pattern->data.tuple->data[i], indent + 1);
            }
            break;
            
        case PATTERN_REST:
            if (pattern->data.rest_name) {
                fprintf(out, "PatRest: ..%s\n", string_cstr(pattern->data.rest_name));
            } else {
                fprintf(out, "PatRest: .._\n");
            }
            break;
    }
}

/* Helper functions for printing specific expression types. */

/**
 * Print a call expression.
 * @param out The output file.
 * @param expr The call expression.
 * @param indent The indentation level.
 */
static void print_expr_call(FILE* out, Expr* expr, int indent) {
    assert(out != NULL);
    assert(expr != NULL && expr->type == EXPR_CALL);
    fprintf(out, "Call:\n");
    print_indent(out, indent + 1);
    fprintf(out, "func:\n");
    ast_print_expr(out, expr->data.call.func, indent + 2);
    print_indent(out, indent + 1);
    fprintf(out, "args: (%zu)\n", expr->data.call.args->len);
    for (size_t i = 0; i < expr->data.call.args->len; i++) {
        CallArg* arg = &expr->data.call.args->data[i];
        if (arg->label) {
            print_indent(out, indent + 2);
            fprintf(out, "%s:\n", string_cstr(arg->label));
            ast_print_expr(out, arg->value, indent + 3);
        } else {
            ast_print_expr(out, arg->value, indent + 2);
        }
    }
}

/**
 * Print an if expression.
 * @param out The output file.
 * @param expr The if expression.
 * @param indent The indentation level.
 */
static void print_expr_if(FILE* out, Expr* expr, int indent) {
    assert(out != NULL);
    assert(expr != NULL && expr->type == EXPR_IF);
    fprintf(out, "If:\n");
    print_indent(out, indent + 1);
    fprintf(out, "condition:\n");
    ast_print_expr(out, expr->data.if_expr.condition, indent + 2);
    print_indent(out, indent + 1);
    fprintf(out, "then:\n");
    ast_print_expr(out, expr->data.if_expr.then_branch, indent + 2);
    if (expr->data.if_expr.else_branch) {
        print_indent(out, indent + 1);
        fprintf(out, "else:\n");
        ast_print_expr(out, expr->data.if_expr.else_branch, indent + 2);
    }
}

/**
 * Print a match expression.
 * @param out The output file.
 * @param expr The match expression.
 * @param indent The indentation level.
 */
static void print_expr_match(FILE* out, Expr* expr, int indent) {
    assert(out != NULL);
    assert(expr != NULL && expr->type == EXPR_MATCH);
    fprintf(out, "Match:\n");
    if (expr->data.match_expr.value) {
        print_indent(out, indent + 1);
        fprintf(out, "value:\n");
        ast_print_expr(out, expr->data.match_expr.value, indent + 2);
    }
    print_indent(out, indent + 1);
    fprintf(out, "arms: (%zu)\n", expr->data.match_expr.arms->len);
    for (size_t i = 0; i < expr->data.match_expr.arms->len; i++) {
        MatchArm* arm = &expr->data.match_expr.arms->data[i];
        print_indent(out, indent + 2);
        fprintf(out, "arm %zu:\n", i);
        ast_print_pattern(out, arm->pattern, indent + 3);
        if (arm->guard) {
            print_indent(out, indent + 3);
            fprintf(out, "guard:\n");
            ast_print_expr(out, arm->guard, indent + 4);
        }
        print_indent(out, indent + 3);
        fprintf(out, "body:\n");
        ast_print_expr(out, arm->body, indent + 4);
    }
}

/**
 * Print a block expression.
 * @param out The output file.
 * @param expr The block expression.
 * @param indent The indentation level.
 */
static void print_expr_block(FILE* out, Expr* expr, int indent) {
    assert(out != NULL);
    assert(expr != NULL && expr->type == EXPR_BLOCK);
    fprintf(out, "Block:\n");
    for (size_t i = 0; i < expr->data.block.stmts->len; i++) {
        ast_print_stmt(out, expr->data.block.stmts->data[i], indent + 1);
    }
    if (expr->data.block.final_expr) {
        print_indent(out, indent + 1);
        fprintf(out, "result:\n");
        ast_print_expr(out, expr->data.block.final_expr, indent + 2);
    }
}

/**
 * Print a with expression.
 * @param out The output file.
 * @param expr The with expression.
 * @param indent The indentation level.
 */
static void print_expr_with(FILE* out, Expr* expr, int indent) {
    assert(out != NULL);
    assert(expr != NULL && expr->type == EXPR_WITH);
    fprintf(out, "With:\n");
    print_indent(out, indent + 1);
    fprintf(out, "bindings:\n");
    for (size_t i = 0; i < expr->data.with_expr.bindings->len; i++) {
        WithBinding* b = &expr->data.with_expr.bindings->data[i];
        print_indent(out, indent + 2);
        fprintf(out, "%s <-\n", string_cstr(b->name));
        ast_print_expr(out, b->value, indent + 3);
    }
    print_indent(out, indent + 1);
    fprintf(out, "body:\n");
    ast_print_expr(out, expr->data.with_expr.body, indent + 2);
}

/**
 * Print a map expression.
 * @param out The output file.
 * @param expr The map expression.
 * @param indent The indentation level.
 */
static void print_expr_map(FILE* out, Expr* expr, int indent) {
    assert(out != NULL);
    assert(expr != NULL && expr->type == EXPR_MAP);
    fprintf(out, "Map: %%{ (%zu entries)\n", expr->data.map.entries->len);
    for (size_t i = 0; i < expr->data.map.entries->len; i++) {
        MapEntry* e = &expr->data.map.entries->data[i];
        print_indent(out, indent + 1);
        fprintf(out, "key:\n");
        ast_print_expr(out, e->key, indent + 2);
        print_indent(out, indent + 1);
        fprintf(out, "value:\n");
        ast_print_expr(out, e->value, indent + 2);
    }
}

/**
 * Print a record update expression.
 * @param out The output file.
 * @param expr The record update expression.
 * @param indent The indentation level.
 */
static void print_expr_record_update(FILE* out, Expr* expr, int indent) {
    assert(out != NULL);
    assert(expr != NULL && expr->type == EXPR_RECORD_UPDATE);
    fprintf(out, "RecordUpdate:\n");
    print_indent(out, indent + 1);
    fprintf(out, "base:\n");
    ast_print_expr(out, expr->data.record_update.base, indent + 2);
    print_indent(out, indent + 1);
    fprintf(out, "fields:\n");
    for (size_t i = 0; i < expr->data.record_update.fields->len; i++) {
        RecordField* f = &expr->data.record_update.fields->data[i];
        print_indent(out, indent + 2);
        fprintf(out, "%s:\n", string_cstr(f->name));
        ast_print_expr(out, f->value, indent + 3);
    }
}

/**
 * Print a list comprehension expression.
 * @param out The output file.
 * @param expr The list comprehension expression.
 * @param indent The indentation level.
 */
static void print_expr_list_comp(FILE* out, Expr* expr, int indent) {
    assert(out != NULL);
    assert(expr != NULL && expr->type == EXPR_LIST_COMP);
    fprintf(out, "ListComp: [... for %s in ...]\n", string_cstr(expr->data.list_comp.var_name));
    print_indent(out, indent + 1);
    fprintf(out, "body:\n");
    ast_print_expr(out, expr->data.list_comp.body, indent + 2);
    print_indent(out, indent + 1);
    fprintf(out, "iterable:\n");
    ast_print_expr(out, expr->data.list_comp.iterable, indent + 2);
    if (expr->data.list_comp.condition) {
        print_indent(out, indent + 1);
        fprintf(out, "condition:\n");
        ast_print_expr(out, expr->data.list_comp.condition, indent + 2);
    }
}

/**
 * Print a receive expression.
 * @param out The output file.
 * @param expr The receive expression.
 * @param indent The indentation level.
 */
static void print_expr_receive(FILE* out, Expr* expr, int indent) {
    assert(out != NULL);
    assert(expr != NULL && expr->type == EXPR_RECEIVE);
    fprintf(out, "Receive:\n");
    for (size_t i = 0; i < expr->data.receive_expr.arms->len; i++) {
        MatchArm* arm = &expr->data.receive_expr.arms->data[i];
        print_indent(out, indent + 1);
        fprintf(out, "arm:\n");
        ast_print_pattern(out, arm->pattern, indent + 2);
        ast_print_expr(out, arm->body, indent + 2);
    }
    if (expr->data.receive_expr.after_timeout) {
        print_indent(out, indent + 1);
        fprintf(out, "after:\n");
        ast_print_expr(out, expr->data.receive_expr.after_timeout, indent + 2);
        ast_print_expr(out, expr->data.receive_expr.after_body, indent + 2);
    }
}

/**
 * Print a list expression.
 * @param out The output file.
 * @param expr The list expression.
 * @param indent The indentation level.
 */
static void print_expr_list(FILE* out, Expr* expr, int indent) {
    assert(out != NULL);
    assert(expr != NULL && expr->type == EXPR_LIST);
    fprintf(out, "List: (%zu elements)\n", expr->data.list.elements->len);
    for (size_t i = 0; i < expr->data.list.elements->len; i++) {
        ast_print_expr(out, expr->data.list.elements->data[i], indent + 1);
    }
}

/**
 * Print a for loop expression.
 * @param out The output file.
 * @param expr The for loop expression.
 * @param indent The indentation level.
 */
static void print_expr_for(FILE* out, Expr* expr, int indent) {
    assert(out != NULL);
    assert(expr != NULL && expr->type == EXPR_FOR);
    fprintf(out, "For: %s in\n", string_cstr(expr->data.for_loop.var_name));
    ast_print_expr(out, expr->data.for_loop.iterable, indent + 1);
    print_indent(out, indent + 1);
    fprintf(out, "body:\n");
    ast_print_expr(out, expr->data.for_loop.body, indent + 2);
}

/**
 * Print a lambda expression.
 * @param out The output file.
 * @param expr The lambda expression.
 * @param indent The indentation level.
 */
static void print_expr_lambda(FILE* out, Expr* expr, int indent) {
    assert(out != NULL);
    assert(expr != NULL && expr->type == EXPR_LAMBDA);
    fprintf(out, "Lambda: (");
    for (size_t i = 0; i < expr->data.lambda.params->len; i++) {
        if (i > 0) fprintf(out, ", ");
        fprintf(out, "%s", string_cstr(expr->data.lambda.params->data[i]));
    }
    fprintf(out, ") ->\n");
    ast_print_expr(out, expr->data.lambda.body, indent + 1);
}

/**
 * Print an interpolated string expression.
 * @param out The output file.
 * @param expr The interpolated string expression.
 * @param indent The indentation level.
 */
static void print_expr_interp_string(FILE* out, Expr* expr, int indent) {
    assert(out != NULL);
    assert(expr != NULL && expr->type == EXPR_INTERP_STRING);
    fprintf(out, "InterpString: (%zu parts)\n", expr->data.interp_string.parts->len);
    for (size_t i = 0; i < expr->data.interp_string.parts->len; i++) {
        ast_print_expr(out, expr->data.interp_string.parts->data[i], indent + 1);
    }
}

/**
 * Print a tuple expression.
 * @param out The output file.
 * @param expr The tuple expression.
 * @param indent The indentation level.
 */
static void print_expr_tuple(FILE* out, Expr* expr, int indent) {
    assert(out != NULL);
    assert(expr != NULL && expr->type == EXPR_TUPLE);
    fprintf(out, "Tuple: (%zu elements)\n", expr->data.tuple.elements->len);
    for (size_t i = 0; i < expr->data.tuple.elements->len; i++) {
        ast_print_expr(out, expr->data.tuple.elements->data[i], indent + 1);
    }
}

/**
 * Print an index expression.
 * @param out The output file.
 * @param expr The index expression.
 * @param indent The indentation level.
 */
static void print_expr_index(FILE* out, Expr* expr, int indent) {
    assert(out != NULL);
    assert(expr != NULL && expr->type == EXPR_INDEX);
    fprintf(out, "Index:\n");
    ast_print_expr(out, expr->data.index_expr.object, indent + 1);
    print_indent(out, indent + 1);
    fprintf(out, "index:\n");
    ast_print_expr(out, expr->data.index_expr.index, indent + 2);
}

/**
 * Print a send expression.
 * @param out The output file.
 * @param expr The send expression.
 * @param indent The indentation level.
 */
static void print_expr_send(FILE* out, Expr* expr, int indent) {
    assert(out != NULL);
    assert(expr != NULL && expr->type == EXPR_SEND);
    fprintf(out, "Send:\n");
    print_indent(out, indent + 1);
    fprintf(out, "pid:\n");
    ast_print_expr(out, expr->data.send_expr.pid, indent + 2);
    print_indent(out, indent + 1);
    fprintf(out, "message:\n");
    ast_print_expr(out, expr->data.send_expr.message, indent + 2);
}

/**
 * Print a binary expression.
 * @param out The output file.
 * @param expr The binary expression.
 * @param indent The indentation level.
 */
static void print_expr_binary(FILE* out, Expr* expr, int indent) {
    assert(out != NULL);
    assert(expr != NULL && expr->type == EXPR_BINARY);
    fprintf(out, "Binary: %s\n", binop_name(expr->data.binary.op));
    ast_print_expr(out, expr->data.binary.left, indent + 1);
    ast_print_expr(out, expr->data.binary.right, indent + 1);
}

/**
 * Print a unary expression.
 * @param out The output file.
 * @param expr The unary expression.
 * @param indent The indentation level.
 */
static void print_expr_unary(FILE* out, Expr* expr, int indent) {
    assert(out != NULL);
    assert(expr != NULL && expr->type == EXPR_UNARY);
    fprintf(out, "Unary: %s\n", unop_name(expr->data.unary.op));
    ast_print_expr(out, expr->data.unary.operand, indent + 1);
}

/**
 * Print a bind expression.
 * @param out The output file.
 * @param expr The bind expression.
 * @param indent The indentation level.
 */
static void print_expr_bind(FILE* out, Expr* expr, int indent) {
    assert(out != NULL);
    assert(expr != NULL && expr->type == EXPR_BIND);
    fprintf(out, "Bind: %s <-\n", string_cstr(expr->data.bind.name));
    ast_print_expr(out, expr->data.bind.value, indent + 1);
}

/**
 * Print a dot expression.
 * @param out The output file.
 * @param expr The dot expression.
 * @param indent The indentation level.
 */
static void print_expr_dot(FILE* out, Expr* expr, int indent) {
    assert(out != NULL);
    assert(expr != NULL && expr->type == EXPR_DOT);
    fprintf(out, "Dot: .%s\n", string_cstr(expr->data.dot.field));
    ast_print_expr(out, expr->data.dot.object, indent + 1);
}

/**
 * Print a range expression.
 * @param out The output file.
 * @param expr The range expression.
 * @param indent The indentation level.
 */
static void print_expr_range(FILE* out, Expr* expr, int indent) {
    assert(out != NULL);
    assert(expr != NULL && expr->type == EXPR_RANGE);
    fprintf(out, "Range: %s\n", expr->data.range.inclusive ? "..=" : "..");
    ast_print_expr(out, expr->data.range.start, indent + 1);
    ast_print_expr(out, expr->data.range.end, indent + 1);
}

/**
 * Print a spawn expression.
 * @param out The output file.
 * @param expr The spawn expression.
 * @param indent The indentation level.
 */
static void print_expr_spawn(FILE* out, Expr* expr, int indent) {
    assert(out != NULL);
    assert(expr != NULL && expr->type == EXPR_SPAWN);
    fprintf(out, "Spawn:\n");
    ast_print_expr(out, expr->data.spawn_expr.func, indent + 1);
}

/**
 * Print a try expression (?).
 * @param out The output file.
 * @param expr The try expression.
 * @param indent The indentation level.
 */
static void print_expr_try(FILE* out, Expr* expr, int indent) {
    assert(out != NULL);
    assert(expr != NULL && expr->type == EXPR_TRY);
    fprintf(out, "Try (?):\n");
    ast_print_expr(out, expr->data.try_expr.operand, indent + 1);
}

/**
 * Print literal expressions (int, float, string, bool, ident).
 * @param out The output file.
 * @param expr The literal expression.
 */
static void print_expr_literal(FILE* out, Expr* expr) {
    assert(out != NULL);
    assert(expr != NULL);
    switch (expr->type) {
        case EXPR_INT_LIT:
            fprintf(out, "Int: %lld\n", (long long)expr->data.int_lit.value);
            break;
        case EXPR_FLOAT_LIT:
            fprintf(out, "Float: %g\n", expr->data.float_lit.value);
            break;
        case EXPR_STRING_LIT:
            fprintf(out, "String: \"%s\"\n", string_cstr(expr->data.string_lit.value));
            break;
        case EXPR_BOOL_LIT:
            fprintf(out, "Bool: %s\n", expr->data.bool_lit.value ? "true" : "false");
            break;
        case EXPR_IDENT:
            fprintf(out, "Ident: %s\n", string_cstr(expr->data.ident.name));
            break;
        default:
            break;
    }
}

/**
 * Dispatch helper for complex expression types (part 1).
 * @param out The output file.
 * @param expr The expression to dispatch.
 * @param indent The indentation level.
 */
static void print_expr_dispatch_1(FILE* out, Expr* expr, int indent) {
    assert(out != NULL);
    assert(expr != NULL);
    switch (expr->type) {
        case EXPR_BINARY:    print_expr_binary(out, expr, indent); break;
        case EXPR_UNARY:     print_expr_unary(out, expr, indent); break;
        case EXPR_CALL:      print_expr_call(out, expr, indent); break;
        case EXPR_IF:        print_expr_if(out, expr, indent); break;
        case EXPR_MATCH:     print_expr_match(out, expr, indent); break;
        case EXPR_BLOCK:     print_expr_block(out, expr, indent); break;
        case EXPR_LIST:      print_expr_list(out, expr, indent); break;
        case EXPR_BIND:      print_expr_bind(out, expr, indent); break;
        case EXPR_WITH:      print_expr_with(out, expr, indent); break;
        case EXPR_DOT:       print_expr_dot(out, expr, indent); break;
        case EXPR_RANGE:     print_expr_range(out, expr, indent); break;
        case EXPR_FOR:       print_expr_for(out, expr, indent); break;
        default: break;
    }
}

/**
 * Dispatch helper for complex expression types (part 2).
 * @param out The output file.
 * @param expr The expression to dispatch.
 * @param indent The indentation level.
 */
static void print_expr_dispatch_2(FILE* out, Expr* expr, int indent) {
    assert(out != NULL);
    assert(expr != NULL);
    switch (expr->type) {
        case EXPR_LAMBDA:        print_expr_lambda(out, expr, indent); break;
        case EXPR_INTERP_STRING: print_expr_interp_string(out, expr, indent); break;
        case EXPR_MAP:           print_expr_map(out, expr, indent); break;
        case EXPR_TUPLE:         print_expr_tuple(out, expr, indent); break;
        case EXPR_RECORD_UPDATE: print_expr_record_update(out, expr, indent); break;
        case EXPR_LIST_COMP:     print_expr_list_comp(out, expr, indent); break;
        case EXPR_INDEX:         print_expr_index(out, expr, indent); break;
        case EXPR_SPAWN:         print_expr_spawn(out, expr, indent); break;
        case EXPR_SEND:          print_expr_send(out, expr, indent); break;
        case EXPR_RECEIVE:       print_expr_receive(out, expr, indent); break;
        case EXPR_TRY:           print_expr_try(out, expr, indent); break;
        default: break;
    }
}

/**
 * Print an expression with indentation.
 * @param out The output file.
 * @param expr The expression to print.
 * @param indent The indentation level.
 */
void ast_print_expr(FILE* out, Expr* expr, int indent) {
    assert(out != NULL);
    assert(indent >= 0 && indent < MAX_INDENT);
    
    if (!expr) {
        print_indent(out, indent);
        fprintf(out, "(null)\n");
        return;
    }
    
    print_indent(out, indent);
    
    /* Dispatch based on expression type category. */
    if (expr->type <= EXPR_IDENT) {
        print_expr_literal(out, expr);
    } else if (expr->type <= EXPR_FOR) {
        print_expr_dispatch_1(out, expr, indent);
    } else {
        print_expr_dispatch_2(out, expr, indent);
    }
}

/* Helper functions for printing specific statement types. */

/**
 * Print a let statement.
 * @param out The output file.
 * @param stmt The let statement.
 * @param indent The indentation level.
 */
static void print_stmt_let(FILE* out, Stmt* stmt, int indent) {
    assert(out != NULL);
    assert(stmt != NULL && stmt->type == STMT_LET);
    fprintf(out, "Let:\n");
    print_indent(out, indent + 1);
    fprintf(out, "pattern:\n");
    ast_print_pattern(out, stmt->data.let.pattern, indent + 2);
    if (stmt->data.let.type_ann) {
        print_indent(out, indent + 1);
        fprintf(out, "type:\n");
        ast_print_type(out, stmt->data.let.type_ann, indent + 2);
    }
    print_indent(out, indent + 1);
    fprintf(out, "value:\n");
    ast_print_expr(out, stmt->data.let.value, indent + 2);
    if (stmt->data.let.else_expr) {
        print_indent(out, indent + 1);
        fprintf(out, "else:\n");
        ast_print_expr(out, stmt->data.let.else_expr, indent + 2);
    }
}

/**
 * Print a function statement.
 * @param out The output file.
 * @param stmt The function statement.
 * @param indent The indentation level.
 */
static void print_stmt_fn(FILE* out, Stmt* stmt, int indent) {
    assert(out != NULL);
    assert(stmt != NULL && stmt->type == STMT_FN);
    fprintf(out, "Fn: %s%s\n", 
        stmt->data.fn.is_public ? "pub " : "",
        string_cstr(stmt->data.fn.name));
    if (stmt->data.fn.params) {
        print_indent(out, indent + 1);
        fprintf(out, "params: (%zu)\n", stmt->data.fn.params->len);
        for (size_t i = 0; i < stmt->data.fn.params->len; i++) {
            Parameter* p = &stmt->data.fn.params->data[i];
            print_indent(out, indent + 2);
            fprintf(out, "%s", string_cstr(p->name));
            if (p->type_ann) {
                fprintf(out, ": %s", string_cstr(p->type_ann->data.named.name));
            }
            fprintf(out, "\n");
        }
    }
    if (stmt->data.fn.return_type) {
        print_indent(out, indent + 1);
        fprintf(out, "returns:\n");
        ast_print_type(out, stmt->data.fn.return_type, indent + 2);
    }
    if (stmt->data.fn.body) {
        print_indent(out, indent + 1);
        fprintf(out, "body:\n");
        ast_print_expr(out, stmt->data.fn.body, indent + 2);
    }
    if (stmt->data.fn.clauses) {
        print_indent(out, indent + 1);
        fprintf(out, "clauses: (%zu)\n", stmt->data.fn.clauses->len);
    }
}

/**
 * Print an import statement.
 * @param out The output file.
 * @param stmt The import statement.
 * @param indent The indentation level.
 */
static void print_stmt_import(FILE* out, Stmt* stmt, int indent) {
    assert(out != NULL);
    assert(stmt != NULL && stmt->type == STMT_IMPORT);
    (void)indent;  /* Unused in this function. */
    fprintf(out, "Import: ");
    for (size_t i = 0; i < stmt->data.import.path->len; i++) {
        if (i > 0) fprintf(out, ".");
        fprintf(out, "%s", string_cstr(stmt->data.import.path->data[i]));
    }
    if (stmt->data.import.items) {
        fprintf(out, ".{");
        for (size_t i = 0; i < stmt->data.import.items->len; i++) {
            if (i > 0) fprintf(out, ", ");
            fprintf(out, "%s", string_cstr(stmt->data.import.items->data[i]));
        }
        fprintf(out, "}");
    }
    if (stmt->data.import.alias) {
        fprintf(out, " as %s", string_cstr(stmt->data.import.alias));
    }
    fprintf(out, "\n");
}

/**
 * Print simple statement types inline (return, expr, defer, etc.).
 * @param out The output file.
 * @param stmt The statement to print.
 * @param indent The indentation level.
 */
static void print_stmt_simple(FILE* out, Stmt* stmt, int indent) {
    assert(out != NULL);
    assert(stmt != NULL);
    switch (stmt->type) {
        case STMT_RETURN:
            fprintf(out, "Return:\n");
            if (stmt->data.return_stmt.value) {
                ast_print_expr(out, stmt->data.return_stmt.value, indent + 1);
            }
            if (stmt->data.return_stmt.condition) {
                print_indent(out, indent + 1);
                fprintf(out, "if:\n");
                ast_print_expr(out, stmt->data.return_stmt.condition, indent + 2);
            }
            break;
        case STMT_EXPR:
            fprintf(out, "ExprStmt:\n");
            ast_print_expr(out, stmt->data.expr.expr, indent + 1);
            break;
        case STMT_DEFER:
            fprintf(out, "Defer:\n");
            ast_print_expr(out, stmt->data.defer_stmt.expr, indent + 1);
            break;
        case STMT_BREAK:
            fprintf(out, "Break");
            if (stmt->data.break_stmt.value) {
                fprintf(out, ":\n");
                ast_print_expr(out, stmt->data.break_stmt.value, indent + 1);
            } else {
                fprintf(out, "\n");
            }
            break;
        case STMT_CONTINUE:
            fprintf(out, "Continue\n");
            break;
        default:
            break;
    }
}

/**
 * Print type definition statements (type, trait, impl, etc.).
 * @param out The output file.
 * @param stmt The type definition statement.
 * @param indent The indentation level.
 */
static void print_stmt_type_defs(FILE* out, Stmt* stmt, int indent) {
    assert(out != NULL);
    assert(stmt != NULL);
    (void)indent;
    switch (stmt->type) {
        case STMT_TYPE_DEF:
            fprintf(out, "TypeDef: %s%s\n",
                stmt->data.type_def.is_public ? "pub " : "",
                string_cstr(stmt->data.type_def.name));
            break;
        case STMT_TRAIT:
            fprintf(out, "Trait: %s\n", string_cstr(stmt->data.trait_def.name));
            break;
        case STMT_IMPL:
            fprintf(out, "Impl: %s\n", string_cstr(stmt->data.impl_def.trait_name));
            break;
        case STMT_NEWTYPE:
            fprintf(out, "Newtype: %s = %s(...)\n",
                string_cstr(stmt->data.newtype_def.name),
                string_cstr(stmt->data.newtype_def.constructor));
            break;
        case STMT_MODULE:
            fprintf(out, "Module: ");
            for (size_t i = 0; i < stmt->data.module_decl.path->len; i++) {
                if (i > 0) fprintf(out, ".");
                fprintf(out, "%s", string_cstr(stmt->data.module_decl.path->data[i]));
            }
            fprintf(out, "\n");
            break;
        default:
            break;
    }
}

/**
 * Print a statement with indentation.
 * @param out The output file.
 * @param stmt The statement to print.
 * @param indent The indentation level.
 */
void ast_print_stmt(FILE* out, Stmt* stmt, int indent) {
    assert(out != NULL);
    assert(indent >= 0 && indent < MAX_INDENT);
    
    if (!stmt) {
        print_indent(out, indent);
        fprintf(out, "(null)\n");
        return;
    }
    
    print_indent(out, indent);
    
    /* Dispatch based on statement type. */
    switch (stmt->type) {
        case STMT_LET:
            print_stmt_let(out, stmt, indent);
            break;
        case STMT_FN:
            print_stmt_fn(out, stmt, indent);
            break;
        case STMT_IMPORT:
            print_stmt_import(out, stmt, indent);
            break;
        case STMT_RETURN:
        case STMT_EXPR:
        case STMT_DEFER:
        case STMT_BREAK:
        case STMT_CONTINUE:
            print_stmt_simple(out, stmt, indent);
            break;
        default:
            print_stmt_type_defs(out, stmt, indent);
            break;
    }
}

/**
 * Dump an expression to stdout for debugging.
 * @param expr The expression to dump.
 */
void ast_dump_expr(Expr* expr) {
    assert(expr != NULL);
    assert(stdout != NULL);
    ast_print_expr(stdout, expr, 0);
}

/**
 * Dump a statement to stdout for debugging.
 * @param stmt The statement to dump.
 */
void ast_dump_stmt(Stmt* stmt) {
    assert(stmt != NULL);
    assert(stdout != NULL);
    ast_print_stmt(stdout, stmt, 0);
}
