/* Token Utility Functions */

#include "token.h"
#include <string.h>

const char* token_type_name(TokenType type) {
    switch (type) {
        case TOKEN_EOF: return "EOF";
        case TOKEN_ERROR: return "ERROR";
        case TOKEN_NEWLINE: return "NEWLINE";
        case TOKEN_INDENT: return "INDENT";
        case TOKEN_DEDENT: return "DEDENT";
        
        case TOKEN_INT: return "INT";
        case TOKEN_FLOAT: return "FLOAT";
        case TOKEN_STRING: return "STRING";
        case TOKEN_TRUE: return "TRUE";
        case TOKEN_FALSE: return "FALSE";
        
        case TOKEN_IDENT: return "IDENT";
        
        case TOKEN_LET: return "LET";
        case TOKEN_FN: return "FN";
        case TOKEN_RETURN: return "RETURN";
        case TOKEN_IF: return "IF";
        case TOKEN_ELSE: return "ELSE";
        case TOKEN_MATCH: return "MATCH";
        case TOKEN_WITH: return "WITH";
        case TOKEN_DO: return "DO";
        case TOKEN_DEFER: return "DEFER";
        case TOKEN_PUB: return "PUB";
        case TOKEN_IMPORT: return "IMPORT";
        case TOKEN_TYPE: return "TYPE";
        case TOKEN_TRAIT: return "TRAIT";
        case TOKEN_IMPL: return "IMPL";
        case TOKEN_AND: return "AND";
        case TOKEN_OR: return "OR";
        case TOKEN_NOT: return "NOT";
        case TOKEN_AS: return "AS";
        case TOKEN_MODULE: return "MODULE";
        case TOKEN_FOR: return "FOR";
        case TOKEN_WHILE: return "WHILE";
        case TOKEN_LOOP: return "LOOP";
        case TOKEN_BREAK: return "BREAK";
        case TOKEN_CONTINUE: return "CONTINUE";
        case TOKEN_IN: return "IN";
        case TOKEN_SPAWN: return "SPAWN";
        case TOKEN_SEND: return "SEND";
        case TOKEN_RECEIVE: return "RECEIVE";
        
        case TOKEN_PLUS: return "PLUS";
        case TOKEN_MINUS: return "MINUS";
        case TOKEN_STAR: return "STAR";
        case TOKEN_SLASH: return "SLASH";
        case TOKEN_PERCENT: return "PERCENT";
        case TOKEN_POWER: return "POWER";
        
        case TOKEN_EQ: return "EQ";
        case TOKEN_NE: return "NE";
        case TOKEN_LT: return "LT";
        case TOKEN_LE: return "LE";
        case TOKEN_GT: return "GT";
        case TOKEN_GE: return "GE";
        
        case TOKEN_ASSIGN: return "ASSIGN";
        case TOKEN_BIND: return "BIND";
        case TOKEN_PIPE: return "PIPE";
        case TOKEN_ARROW: return "ARROW";
        case TOKEN_FAT_ARROW: return "FAT_ARROW";
        
        case TOKEN_LPAREN: return "LPAREN";
        case TOKEN_RPAREN: return "RPAREN";
        case TOKEN_LBRACKET: return "LBRACKET";
        case TOKEN_RBRACKET: return "RBRACKET";
        case TOKEN_LBRACE: return "LBRACE";
        case TOKEN_RBRACE: return "RBRACE";
        
        case TOKEN_COMMA: return "COMMA";
        case TOKEN_COLON: return "COLON";
        case TOKEN_DOT: return "DOT";
        case TOKEN_DOTDOT: return "DOTDOT";
        case TOKEN_ELLIPSIS: return "ELLIPSIS";
        case TOKEN_UNDERSCORE: return "UNDERSCORE";
        case TOKEN_AT: return "AT";
        
        case TOKEN_COMMENT: return "COMMENT";
        case TOKEN_BLOCK_COMMENT: return "BLOCK_COMMENT";
        case TOKEN_DOC_COMMENT: return "DOC_COMMENT";
        
        default: return "UNKNOWN";
    }
}

bool token_is_keyword(TokenType type) {
    return type >= TOKEN_LET && type <= TOKEN_RECEIVE;
}

bool token_is_operator(TokenType type) {
    return type >= TOKEN_PLUS && type <= TOKEN_FAT_ARROW;
}

bool token_is_literal(TokenType type) {
    return type >= TOKEN_INT && type <= TOKEN_FALSE;
}
