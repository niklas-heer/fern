/**
 * Fern Language Server Protocol (LSP) Implementation
 *
 * This implements a minimal LSP server that provides:
 * - Diagnostics (real-time error reporting)
 * - Hover (type information)
 * - Go-to-definition (symbol navigation)
 *
 * The server reuses the existing compiler infrastructure, so language
 * changes automatically work without modifying the LSP code.
 */

#include "lsp.h"
#include "lexer.h"
#include "parser.h"
#include "checker.h"
#include "type_env.h"
#include "type.h"
#include "arena.h"
#include "fern_string.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>

/* ========== Constants ========== */

#define LSP_VERSION "0.1.0"
#define MAX_MESSAGE_SIZE (10 * 1024 * 1024)  /* 10 MB max message */
#define MAX_DOCUMENTS 256

/* ========== Logging ========== */

/**
 * Log a message to the LSP log file.
 * @param server The LSP server.
 * @param fmt Format string.
 * @param ... Format arguments.
 */
static void lsp_log(LspServer* server, const char* fmt, ...) {
    assert(server != NULL);
    assert(fmt != NULL);

    if (server->log == NULL) return;

    va_list args;
    va_start(args, fmt);
    vfprintf(server->log, fmt, args);
    va_end(args);
    fprintf(server->log, "\n");
    fflush(server->log);
}

/* ========== JSON Helpers ========== */

/**
 * Escape a string for JSON output.
 * @param arena Arena for allocation.
 * @param s String to escape.
 * @return Escaped string.
 */
static String* json_escape_string(Arena* arena, const char* s) {
    assert(arena != NULL);
    assert(s != NULL);

    String* result = string_new(arena, "");
    size_t len = strlen(s);

    for (size_t i = 0; i < len; i++) {
        char c = s[i];
        switch (c) {
            case '"':  result = string_append_cstr(arena, result, "\\\""); break;
            case '\\': result = string_append_cstr(arena, result, "\\\\"); break;
            case '\n': result = string_append_cstr(arena, result, "\\n"); break;
            case '\r': result = string_append_cstr(arena, result, "\\r"); break;
            case '\t': result = string_append_cstr(arena, result, "\\t"); break;
            default:
                if (c >= 0 && c < 32) {
                    /* Control character - skip or escape */
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
                    result = string_append_cstr(arena, result, buf);
                } else {
                    char buf[2] = {c, '\0'};
                    result = string_append_cstr(arena, result, buf);
                }
                break;
        }
    }
    return result;
}

/**
 * Parse a JSON string value (expects input positioned after opening quote).
 * @param arena Arena for allocation.
 * @param json JSON string.
 * @param pos Current position (updated).
 * @return Parsed string value.
 */
static String* json_parse_string(Arena* arena, const char* json, size_t* pos) {
    assert(arena != NULL);
    assert(json != NULL);
    assert(pos != NULL);

    String* result = string_new(arena, "");
    size_t i = *pos;

    while (json[i] != '\0' && json[i] != '"') {
        if (json[i] == '\\' && json[i + 1] != '\0') {
            i++;
            switch (json[i]) {
                case '"':  result = string_append_cstr(arena, result, "\""); break;
                case '\\': result = string_append_cstr(arena, result, "\\"); break;
                case 'n':  result = string_append_cstr(arena, result, "\n"); break;
                case 'r':  result = string_append_cstr(arena, result, "\r"); break;
                case 't':  result = string_append_cstr(arena, result, "\t"); break;
                default: {
                    char buf[2] = {json[i], '\0'};
                    result = string_append_cstr(arena, result, buf);
                    break;
                }
            }
        } else {
            char buf[2] = {json[i], '\0'};
            result = string_append_cstr(arena, result, buf);
        }
        i++;
    }

    if (json[i] == '"') i++;  /* Skip closing quote */
    *pos = i;
    return result;
}

/**
 * Parse a JSON integer value.
 * @param json JSON string.
 * @param pos Current position (updated).
 * @return Parsed integer.
 */
static int64_t json_parse_int(const char* json, size_t* pos) {
    assert(json != NULL);
    assert(pos != NULL);

    int64_t result = 0;
    bool negative = false;
    size_t i = *pos;

    if (json[i] == '-') {
        negative = true;
        i++;
    }

    while (json[i] >= '0' && json[i] <= '9') {
        result = result * 10 + (json[i] - '0');
        i++;
    }

    *pos = i;
    return negative ? -result : result;
}

/**
 * Skip whitespace in JSON.
 * @param json JSON string.
 * @param pos Current position (updated).
 */
static void json_skip_ws(const char* json, size_t* pos) {
    assert(json != NULL);
    assert(pos != NULL);

    while (json[*pos] == ' ' || json[*pos] == '\t' ||
           json[*pos] == '\n' || json[*pos] == '\r') {
        (*pos)++;
    }
}

/**
 * Find a key in a JSON object and return position after the colon.
 * @param json JSON string.
 * @param key Key to find.
 * @param start Start position.
 * @return Position after colon, or 0 if not found.
 */
static size_t json_find_key(const char* json, const char* key, size_t start) {
    assert(json != NULL);
    assert(key != NULL);

    char pattern[256];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    const char* found = strstr(json + start, pattern);
    if (found == NULL) return 0;

    size_t pos = (found - json) + strlen(pattern);
    json_skip_ws(json, &pos);
    if (json[pos] == ':') pos++;
    json_skip_ws(json, &pos);
    return pos;
}

/**
 * Extract a string value for a key from JSON.
 * @param arena Arena for allocation.
 * @param json JSON string.
 * @param key Key to find.
 * @return String value, or NULL if not found.
 */
static String* json_get_string(Arena* arena, const char* json, const char* key) {
    assert(arena != NULL);
    assert(json != NULL);
    assert(key != NULL);

    size_t pos = json_find_key(json, key, 0);
    if (pos == 0 || json[pos] != '"') return NULL;
    pos++;  /* Skip opening quote */
    return json_parse_string(arena, json, &pos);
}

/**
 * Extract an integer value for a key from JSON.
 * @param json JSON string.
 * @param key Key to find.
 * @param default_val Default value if not found.
 * @return Integer value.
 */
static int64_t json_get_int(const char* json, const char* key, int64_t default_val) {
    assert(json != NULL);
    assert(key != NULL);

    size_t pos = json_find_key(json, key, 0);
    if (pos == 0) return default_val;
    if (json[pos] != '-' && (json[pos] < '0' || json[pos] > '9')) return default_val;
    return json_parse_int(json, &pos);
}

/**
 * Extract the "id" field from a JSON-RPC request.
 * @param json JSON string.
 * @return Request ID, or -1 if not found.
 */
static int64_t json_get_id(const char* json) {
    // FERN_STYLE: allow(assertion-density) simple wrapper function
    assert(json != NULL);
    return json_get_int(json, "id", -1);
}

/**
 * Extract the "method" field from a JSON-RPC request.
 * @param arena Arena for allocation.
 * @param json JSON string.
 * @return Method name, or NULL if not found.
 */
static String* json_get_method(Arena* arena, const char* json) {
    assert(arena != NULL);
    assert(json != NULL);
    return json_get_string(arena, json, "method");
}

/* ========== Text Helpers ========== */

/**
 * Check if a character can start an identifier.
 * @param c Character to classify.
 * @return True for [A-Za-z_].
 */
static bool is_ident_start_char(char c) {
    // FERN_STYLE: allow(assertion-density) simple character classification helper
    return ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            c == '_');
}

/**
 * Check if a character can appear in an identifier.
 * @param c Character to classify.
 * @return True for [A-Za-z0-9_].
 */
static bool is_ident_char(char c) {
    // FERN_STYLE: allow(assertion-density) simple character classification helper
    return is_ident_start_char(c) || (c >= '0' && c <= '9');
}

/**
 * Validate whether a string is a legal identifier.
 * @param text Candidate identifier.
 * @return True if valid Fern identifier shape.
 */
static bool is_valid_identifier_name(const char* text) {
    // FERN_STYLE: allow(assertion-density) compact predicate helper
    assert(text != NULL);
    if (!text || !is_ident_start_char(text[0])) {
        return false;
    }
    for (size_t i = 1; text[i] != '\0'; i++) {
        if (!is_ident_char(text[i])) {
            return false;
        }
    }
    return true;
}

/**
 * Resolve start/end byte offsets for a specific line.
 * @param content Full source text.
 * @param line Target 0-indexed line.
 * @param out_start Output line-start offset.
 * @param out_end Output line-end offset (before newline).
 * @return True if line exists.
 */
static bool line_span_for_line(const char* content, int line, size_t* out_start, size_t* out_end) {
    assert(content != NULL);
    assert(out_start != NULL);
    assert(out_end != NULL);
    if (!content || !out_start || !out_end || line < 0) {
        return false;
    }

    size_t start = 0;
    int current_line = 0;
    for (size_t i = 0; content[i] != '\0' && current_line < line; i++) {
        if (content[i] == '\n') {
            current_line++;
            start = i + 1;
        }
    }
    if (current_line != line) {
        return false;
    }

    size_t end = start;
    while (content[end] != '\0' && content[end] != '\n') {
        end++;
    }

    *out_start = start;
    *out_end = end;
    return true;
}

/**
 * Resolve identifier byte range at/near a position.
 * @param content Full source text.
 * @param pos LSP position.
 * @param out_start Output start offset.
 * @param out_end Output end offset (exclusive).
 * @param out_line_start Output line-start offset.
 * @return True if an identifier was found at the cursor.
 */
static bool word_span_at_position(const char* content, LspPosition pos,
                                  size_t* out_start, size_t* out_end,
                                  size_t* out_line_start) {
    assert(content != NULL);
    assert(out_start != NULL);
    assert(out_end != NULL);
    assert(out_line_start != NULL);
    if (!content || !out_start || !out_end || !out_line_start) {
        return false;
    }

    size_t line_start = 0;
    size_t line_end = 0;
    if (!line_span_for_line(content, pos.line, &line_start, &line_end)) {
        return false;
    }

    size_t cursor = line_start;
    if (pos.character > 0) {
        cursor += (size_t)pos.character;
    }
    if (cursor > line_end) {
        cursor = line_end;
    }
    if (cursor >= line_end || !is_ident_char(content[cursor])) {
        if (cursor == line_start || !is_ident_char(content[cursor - 1])) {
            return false;
        }
        cursor--;
    }

    size_t start = cursor;
    while (start > line_start && is_ident_char(content[start - 1])) {
        start--;
    }

    size_t end = cursor + 1;
    while (end < line_end && is_ident_char(content[end])) {
        end++;
    }
    if (end <= start) {
        return false;
    }

    *out_start = start;
    *out_end = end;
    *out_line_start = line_start;
    return true;
}

/**
 * Check whether text starts with a prefix.
 * @param text Candidate text.
 * @param prefix Prefix text.
 * @return True when text has prefix.
 */
static bool starts_with(const char* text, const char* prefix) {
    assert(text != NULL);
    assert(prefix != NULL);
    if (!text || !prefix) {
        return false;
    }
    size_t prefix_len = strlen(prefix);
    return strncmp(text, prefix, prefix_len) == 0;
}

/**
 * Parse positive integer segment from a bounded substring.
 * @param start Segment start.
 * @param end Segment end.
 * @param value_out Parsed value.
 * @return True if parse succeeded.
 */
static bool parse_positive_int_segment(const char* start, const char* end, int* value_out) {
    assert(start != NULL);
    assert(end != NULL);
    assert(value_out != NULL);
    if (!start || !end || !value_out || start >= end) {
        return false;
    }

    size_t len = (size_t)(end - start);
    if (len >= 32) {
        return false;
    }

    char buf[32];
    memcpy(buf, start, len);
    buf[len] = '\0';

    char* parse_end = NULL;
    long value = strtol(buf, &parse_end, 10);
    if (!parse_end || *parse_end != '\0') {
        return false;
    }
    if (value <= 0 || value > 2147483647L) {
        return false;
    }

    *value_out = (int)value;
    return true;
}

/**
 * Parse checker error text and extract location and message.
 * Supports "file:line:col: message" and "line:col: message".
 * @param err Checker error text.
 * @param line_out Parsed 1-indexed line.
 * @param col_out Parsed 1-indexed column.
 * @param message_out Message start pointer.
 * @return True if a location was parsed.
 */
static bool parse_checker_error_location(const char* err, int* line_out, int* col_out,
                                         const char** message_out) {
    assert(err != NULL);
    assert(line_out != NULL);
    assert(col_out != NULL);
    assert(message_out != NULL);
    if (!err || !line_out || !col_out || !message_out) {
        return false;
    }

    *line_out = 0;
    *col_out = 0;
    *message_out = err;

    const char* c1 = strchr(err, ':');
    if (!c1) {
        return false;
    }
    const char* c2 = strchr(c1 + 1, ':');
    if (!c2) {
        return false;
    }
    const char* c3 = strchr(c2 + 1, ':');

    int line = 0;
    int col = 0;
    if (c3 &&
        parse_positive_int_segment(c1 + 1, c2, &line) &&
        parse_positive_int_segment(c2 + 1, c3, &col)) {
        *line_out = line;
        *col_out = col;
        *message_out = c3 + 1;
        while (**message_out == ' ') {
            (*message_out)++;
        }
        return true;
    }

    if (parse_positive_int_segment(err, c1, &line) &&
        parse_positive_int_segment(c1 + 1, c2, &col)) {
        *line_out = line;
        *col_out = col;
        *message_out = c2 + 1;
        while (**message_out == ' ') {
            (*message_out)++;
        }
        return true;
    }

    return false;
}

/* ========== Message I/O ========== */

/**
 * Read a JSON-RPC message from stdin.
 * @param arena Arena for allocating the message string.
 * @return The message content, or NULL on EOF/error.
 */
String* lsp_read_message(Arena* arena) {
    // FERN_STYLE: allow(assertion-density) I/O function with multiple error paths
    // FERN_STYLE: allow(bounded-loops) HTTP header parsing requires reading until empty line
    assert(arena != NULL);

    /* Read headers */
    int content_length = -1;
    char header_line[1024];

    while (fgets(header_line, sizeof(header_line), stdin) != NULL) {
        /* Empty line signals end of headers */
        if (strcmp(header_line, "\r\n") == 0 || strcmp(header_line, "\n") == 0) {
            break;
        }

        /* Parse Content-Length header */
        if (strncmp(header_line, "Content-Length:", 15) == 0) {
            content_length = atoi(header_line + 15);
        }
    }

    if (content_length < 0 || content_length > MAX_MESSAGE_SIZE) {
        return NULL;
    }

    /* Read content */
    char* content = arena_alloc(arena, content_length + 1);
    size_t bytes_read = fread(content, 1, content_length, stdin);
    if ((int)bytes_read != content_length) {
        return NULL;
    }
    content[content_length] = '\0';

    return string_new(arena, content);
}

/**
 * Write a JSON-RPC message to stdout.
 * @param content The JSON content to send.
 */
void lsp_write_message(const char* content) {
    // FERN_STYLE: allow(assertion-density) simple I/O wrapper
    assert(content != NULL);

    size_t len = strlen(content);
    printf("Content-Length: %zu\r\n\r\n%s", len, content);
    fflush(stdout);
}

/* ========== Response Builders ========== */

/**
 * Build a JSON-RPC response.
 * @param arena Arena for allocation.
 * @param id Request ID.
 * @param result Result JSON (already formatted).
 * @return Complete JSON-RPC response.
 */
static String* build_response(Arena* arena, int64_t id, const char* result) {
    assert(arena != NULL);
    assert(result != NULL);

    return string_format(arena,
        "{\"jsonrpc\":\"2.0\",\"id\":%lld,\"result\":%s}",
        (long long)id, result);
}

/**
 * Build a JSON-RPC error response.
 * @param arena Arena for allocation.
 * @param id Request ID.
 * @param code Error code.
 * @param message Error message.
 * @return Complete JSON-RPC error response.
 */
static String* build_error(Arena* arena, int64_t id, int code, const char* message) {
    assert(arena != NULL);
    assert(message != NULL);

    String* escaped = json_escape_string(arena, message);
    return string_format(arena,
        "{\"jsonrpc\":\"2.0\",\"id\":%lld,\"error\":{\"code\":%d,\"message\":\"%s\"}}",
        (long long)id, code, string_cstr(escaped));
}

/**
 * Build a JSON-RPC notification (no id, no response expected).
 * @param arena Arena for allocation.
 * @param method Method name.
 * @param params Params JSON.
 * @return Complete JSON-RPC notification.
 */
static String* build_notification(Arena* arena, const char* method, const char* params) {
    assert(arena != NULL);
    assert(method != NULL);
    assert(params != NULL);

    return string_format(arena,
        "{\"jsonrpc\":\"2.0\",\"method\":\"%s\",\"params\":%s}",
        method, params);
}

/* ========== Document Management ========== */

/**
 * Create a new document vector.
 * @param arena Arena for allocation.
 * @return New document vector.
 */
static LspDocumentVec* LspDocumentVec_new(Arena* arena) {
    // FERN_STYLE: allow(assertion-density) simple allocation wrapper
    assert(arena != NULL);

    LspDocumentVec* vec = arena_alloc(arena, sizeof(LspDocumentVec));
    vec->data = arena_alloc(arena, MAX_DOCUMENTS * sizeof(LspDocument*));
    vec->len = 0;
    vec->cap = MAX_DOCUMENTS;
    return vec;
}

/**
 * Find a document by URI.
 * @param server The LSP server.
 * @param uri The document URI to find.
 * @return The document, or NULL if not found.
 */
LspDocument* lsp_document_find(LspServer* server, const char* uri) {
    assert(server != NULL);
    assert(uri != NULL);

    for (size_t i = 0; i < server->documents->len; i++) {
        if (strcmp(string_cstr(server->documents->data[i]->uri), uri) == 0) {
            return server->documents->data[i];
        }
    }
    return NULL;
}

/**
 * Parse and type-check a document.
 * @param server The LSP server.
 * @param doc The document to analyze.
 */
static void lsp_analyze_document(LspServer* server, LspDocument* doc) {
    assert(server != NULL);
    assert(doc != NULL);

    Arena* temp_arena = arena_create(64 * 1024);

    /* Create parser (it creates its own lexer internally) */
    Parser* parser = parser_new(temp_arena, string_cstr(doc->content));

    /* Parse the document */
    doc->ast = parse_stmts(parser);
    doc->has_errors = parser_had_error(parser);

    /* Type check if parsing succeeded */
    if (!doc->has_errors && doc->ast != NULL) {
        doc->checker = checker_new(temp_arena);
        for (size_t i = 0; i < doc->ast->len; i++) {
            checker_check_stmt(doc->checker, doc->ast->data[i]);
        }
        doc->has_errors = checker_has_errors(doc->checker);
    }

    lsp_log(server, "Analyzed %s: %s",
            string_cstr(doc->uri),
            doc->has_errors ? "has errors" : "OK");

    /* Note: We keep temp_arena alive because AST references it.
     * In a real implementation, we'd copy to the server arena. */
}

void lsp_document_open(LspServer* server, const char* uri,
                       const char* content, int version) {
    assert(server != NULL);
    assert(uri != NULL);
    assert(content != NULL);

    LspDocument* doc = arena_alloc(server->arena, sizeof(LspDocument));
    doc->uri = string_new(server->arena, uri);
    doc->content = string_new(server->arena, content);
    doc->version = version;
    doc->ast = NULL;
    doc->checker = NULL;
    doc->has_errors = false;

    if (server->documents->len < server->documents->cap) {
        server->documents->data[server->documents->len++] = doc;
    }

    lsp_log(server, "Document opened: %s (version %d)", uri, version);
    lsp_analyze_document(server, doc);
}

void lsp_document_change(LspServer* server, const char* uri,
                         const char* content, int version) {
    assert(server != NULL);
    assert(uri != NULL);
    assert(content != NULL);

    LspDocument* doc = lsp_document_find(server, uri);
    if (doc == NULL) {
        lsp_document_open(server, uri, content, version);
        return;
    }

    doc->content = string_new(server->arena, content);
    doc->version = version;

    lsp_log(server, "Document changed: %s (version %d)", uri, version);
    lsp_analyze_document(server, doc);
}

/**
 * Close a document and remove it from tracking.
 * @param server The LSP server.
 * @param uri The document URI to close.
 */
void lsp_document_close(LspServer* server, const char* uri) {
    assert(server != NULL);
    assert(uri != NULL);

    /* Find and remove the document */
    for (size_t i = 0; i < server->documents->len; i++) {
        if (strcmp(string_cstr(server->documents->data[i]->uri), uri) == 0) {
            /* Shift remaining documents */
            for (size_t j = i; j < server->documents->len - 1; j++) {
                server->documents->data[j] = server->documents->data[j + 1];
            }
            server->documents->len--;
            break;
        }
    }

    lsp_log(server, "Document closed: %s", uri);
}

/* ========== LSP Features ========== */

/**
 * Convert Fern source location to LSP position.
 * @param loc Source location (1-indexed line/column).
 * @return LSP position (0-indexed line/character).
 */
static LspPosition loc_to_position(SourceLoc loc) {
    // FERN_STYLE: allow(assertion-density) simple conversion function
    /* SourceLoc uses 1-indexed lines, LSP uses 0-indexed */
    LspPosition pos;
    pos.line = loc.line > 0 ? (int)loc.line - 1 : 0;
    pos.character = loc.column > 0 ? (int)loc.column - 1 : 0;
    return pos;
}

LspDiagnostic* lsp_get_diagnostics(LspServer* server, const char* uri,
                                    size_t* out_count) {
    assert(server != NULL);
    assert(uri != NULL);
    assert(out_count != NULL);

    LspDocument* doc = lsp_document_find(server, uri);
    if (doc == NULL) {
        *out_count = 0;
        return NULL;
    }

    /* Check if there are any errors */
    if (!doc->has_errors) {
        *out_count = 0;
        return NULL;
    }

    /* Report a single diagnostic with best-effort location extraction. */
    const char* error_msg = NULL;
    if (doc->checker != NULL) {
        error_msg = checker_first_error(doc->checker);
    }
    if (error_msg == NULL) {
        error_msg = "Parse or type error";
    }

    LspDiagnostic* diags = arena_alloc(server->arena, sizeof(LspDiagnostic));
    int line = 0;
    int col = 0;
    const char* message = error_msg;
    bool has_loc = parse_checker_error_location(error_msg, &line, &col, &message);

    if (has_loc && line > 0) {
        diags[0].range.start.line = line - 1;
        diags[0].range.start.character = col > 0 ? col - 1 : 0;
        diags[0].range.end.line = diags[0].range.start.line;
        diags[0].range.end.character = diags[0].range.start.character + 1;
    } else {
        diags[0].range.start.line = 0;
        diags[0].range.start.character = 0;
        diags[0].range.end.line = 0;
        diags[0].range.end.character = 1;
    }
    diags[0].severity = LSP_SEVERITY_ERROR;
    diags[0].message = string_new(server->arena, message);
    diags[0].source = string_new(server->arena, "fern");

    *out_count = 1;
    return diags;
}

String* lsp_get_hover(LspServer* server, const char* uri,
                      LspPosition pos, LspRange* out_range) {
    assert(server != NULL);
    assert(uri != NULL);
    assert(out_range != NULL);

    LspDocument* doc = lsp_document_find(server, uri);
    if (doc == NULL || doc->checker == NULL) {
        return NULL;
    }

    const char* content = string_cstr(doc->content);
    size_t word_start = 0;
    size_t word_end = 0;
    size_t line_start = 0;
    if (!word_span_at_position(content, pos, &word_start, &word_end, &line_start)) {
        return NULL;
    }

    /* Extract the word */
    size_t word_len = word_end - word_start;
    char* word = arena_alloc(server->arena, word_len + 1);
    memcpy(word, content + word_start, word_len);
    word[word_len] = '\0';

    /* Look up the type in the checker's environment */
    String* word_str = string_new(server->arena, word);
    TypeEnv* env = checker_env(doc->checker);
    Type* type = type_env_lookup(env, word_str);
    if (type == NULL) {
        return NULL;
    }

    /* Build hover content */
    String* type_str = type_to_string(server->arena, type);
    String* hover = string_format(server->arena,
        "```fern\n%s: %s\n```",
        word, string_cstr(type_str));

    /* Set the range */
    out_range->start.line = pos.line;
    out_range->start.character = (int)(word_start - line_start);
    out_range->end.line = pos.line;
    out_range->end.character = (int)(word_end - line_start);

    return hover;
}

LspLocation* lsp_get_definition(LspServer* server, const char* uri,
                                 LspPosition pos) {
    assert(server != NULL);
    assert(uri != NULL);

    LspDocument* doc = lsp_document_find(server, uri);
    if (doc == NULL || doc->ast == NULL) {
        return NULL;
    }

    const char* content = string_cstr(doc->content);
    size_t word_start = 0;
    size_t word_end = 0;
    size_t ignored_line_start = 0;
    if (!word_span_at_position(content, pos, &word_start, &word_end, &ignored_line_start)) {
        return NULL;
    }

    size_t word_len = word_end - word_start;
    char* word = arena_alloc(server->arena, word_len + 1);
    memcpy(word, content + word_start, word_len);
    word[word_len] = '\0';

    /* Search for function definition with this name */
    for (size_t i = 0; i < doc->ast->len; i++) {
        Stmt* stmt = doc->ast->data[i];
        if (stmt->type == STMT_FN) {
            if (strcmp(string_cstr(stmt->data.fn.name), word) == 0) {
                LspLocation* loc = arena_alloc(server->arena, sizeof(LspLocation));
                loc->uri = doc->uri;
                loc->range.start = loc_to_position(stmt->loc);
                int fn_line = (int)stmt->loc.line - 1;
                size_t fn_line_start = 0;
                size_t fn_line_end = 0;
                if (fn_line >= 0 && line_span_for_line(content, fn_line, &fn_line_start, &fn_line_end)) {
                    const char* found = strstr(content + fn_line_start, word);
                    if (found != NULL && (size_t)(found - content) < fn_line_end) {
                        loc->range.start.line = fn_line;
                        loc->range.start.character = (int)((size_t)(found - content) - fn_line_start);
                    }
                }
                loc->range.end = loc->range.start;
                loc->range.end.character += (int)strlen(word);
                return loc;
            }
        }
    }

    return NULL;
}

/**
 * Check whether a completion list already contains a label.
 * @param items Existing completion items.
 * @param count Current item count.
 * @param label Candidate label.
 * @return True when label already exists.
 */
static bool completion_label_exists(const LspCompletionItem* items, size_t count, const char* label) {
    assert(items != NULL);
    assert(label != NULL);
    if (!items || !label) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        if (items[i].label && strcmp(string_cstr(items[i].label), label) == 0) {
            return true;
        }
    }
    return false;
}

/**
 * Append a completion candidate if it matches prefix and is unique.
 * @param server LSP server arena owner.
 * @param items Completion array.
 * @param max_items Capacity of completion array.
 * @param count_io In/out item count.
 * @param prefix Completion prefix.
 * @param label Completion label.
 * @param detail Completion detail text.
 * @param kind LSP completion kind number.
 */
static void completion_try_add(LspServer* server, LspCompletionItem* items, size_t max_items,
                               size_t* count_io, const char* prefix, const char* label,
                               const char* detail, int kind) {
    assert(server != NULL);
    assert(items != NULL);
    assert(count_io != NULL);
    assert(prefix != NULL);
    assert(label != NULL);
    assert(detail != NULL);
    if (!server || !items || !count_io || !prefix || !label || !detail) {
        return;
    }
    if (!starts_with(label, prefix)) {
        return;
    }
    if (*count_io >= max_items || completion_label_exists(items, *count_io, label)) {
        return;
    }

    items[*count_io].label = string_new(server->arena, label);
    items[*count_io].detail = string_new(server->arena, detail);
    items[*count_io].kind = kind;
    (*count_io)++;
}

/**
 * Add identifier completions from a pattern tree.
 * @param server LSP server arena owner.
 * @param items Completion array.
 * @param max_items Capacity.
 * @param count_io In/out count.
 * @param prefix Completion prefix.
 * @param pattern Pattern to inspect.
 */
static void completion_add_pattern_idents(LspServer* server, LspCompletionItem* items, size_t max_items,
                                          size_t* count_io, const char* prefix, Pattern* pattern) {
    assert(server != NULL);
    assert(items != NULL);
    assert(count_io != NULL);
    assert(prefix != NULL);
    if (!server || !items || !count_io || !prefix || !pattern) {
        return;
    }

    if (pattern->type == PATTERN_IDENT) {
        completion_try_add(server, items, max_items, count_io, prefix,
                           string_cstr(pattern->data.ident), "local binding", 6);
        return;
    }
    if (pattern->type == PATTERN_REST && pattern->data.rest_name != NULL) {
        completion_try_add(server, items, max_items, count_io, prefix,
                           string_cstr(pattern->data.rest_name), "local binding", 6);
        return;
    }
    if (pattern->type == PATTERN_TUPLE && pattern->data.tuple != NULL) {
        for (size_t i = 0; i < pattern->data.tuple->len; i++) {
            completion_add_pattern_idents(server, items, max_items, count_io, prefix,
                                          pattern->data.tuple->data[i]);
        }
        return;
    }
    if (pattern->type == PATTERN_CONSTRUCTOR && pattern->data.constructor.args != NULL) {
        for (size_t i = 0; i < pattern->data.constructor.args->len; i++) {
            completion_add_pattern_idents(server, items, max_items, count_io, prefix,
                                          pattern->data.constructor.args->data[i]);
        }
    }
}

LspCompletionItem* lsp_get_completions(LspServer* server, const char* uri,
                                       LspPosition pos, size_t* out_count) {
    // FERN_STYLE: allow(function-length) completion assembly from keywords/modules/AST symbols
    assert(server != NULL);
    assert(uri != NULL);
    assert(out_count != NULL);
    if (!server || !uri || !out_count) {
        return NULL;
    }
    *out_count = 0;

    LspDocument* doc = lsp_document_find(server, uri);
    if (doc == NULL) {
        return NULL;
    }

    const char* content = string_cstr(doc->content);
    size_t line_start = 0;
    size_t line_end = 0;
    if (!line_span_for_line(content, pos.line, &line_start, &line_end)) {
        return NULL;
    }

    size_t cursor = line_start + (pos.character > 0 ? (size_t)pos.character : 0);
    if (cursor > line_end) {
        cursor = line_end;
    }
    size_t prefix_start = cursor;
    while (prefix_start > line_start && is_ident_char(content[prefix_start - 1])) {
        prefix_start--;
    }

    size_t prefix_len = cursor - prefix_start;
    if (prefix_len >= 128) {
        prefix_len = 127;
    }
    char prefix[128];
    memcpy(prefix, content + prefix_start, prefix_len);
    prefix[prefix_len] = '\0';

    size_t max_items = 160 + (doc->ast ? doc->ast->len * 4 : 0);
    LspCompletionItem* items = arena_alloc(server->arena, max_items * sizeof(LspCompletionItem));
    size_t count = 0;

    const char* keywords[] = {
        "fn", "let", "if", "else", "match", "with", "return", "import", "type",
        "trait", "impl", "pub", "module", "for", "break", "continue", "true", "false",
        "spawn", "send", "receive", NULL
    };
    for (size_t i = 0; keywords[i] != NULL; i++) {
        completion_try_add(server, items, max_items, &count, prefix, keywords[i], "keyword", 14);
    }

    const char* modules[] = { "fs", "json", "http", "sql", "actors", "File", "Tui", NULL };
    for (size_t i = 0; modules[i] != NULL; i++) {
        completion_try_add(server, items, max_items, &count, prefix, modules[i], "module", 9);
    }

    if (doc->ast != NULL) {
        for (size_t i = 0; i < doc->ast->len; i++) {
            Stmt* stmt = doc->ast->data[i];
            if (stmt->type == STMT_FN) {
                completion_try_add(server, items, max_items, &count, prefix,
                                   string_cstr(stmt->data.fn.name), "function", 3);
            } else if (stmt->type == STMT_LET) {
                completion_add_pattern_idents(server, items, max_items, &count, prefix, stmt->data.let.pattern);
            }
        }
    }

    if (count == 0) {
        return NULL;
    }
    *out_count = count;
    return items;
}

/**
 * Count or collect identifier occurrences in source text.
 * @param server LSP server arena owner.
 * @param content Source text.
 * @param target Target identifier.
 * @param new_text Replacement text.
 * @param out_edits Optional output buffer (NULL for count-only pass).
 * @return Number of matching occurrences.
 */
static size_t collect_identifier_occurrences(LspServer* server, const char* content, const char* target,
                                             String* new_text, LspTextEdit* out_edits) {
    assert(server != NULL);
    assert(content != NULL);
    assert(target != NULL);
    assert(new_text != NULL);
    if (!server || !content || !target || !new_text) {
        return 0;
    }

    size_t count = 0;
    size_t target_len = strlen(target);
    int line = 0;
    int col = 0;

    for (size_t i = 0; content[i] != '\0'; ) {
        if (is_ident_start_char(content[i])) {
            size_t start = i;
            int start_line = line;
            int start_col = col;

            i++;
            col++;
            while (is_ident_char(content[i])) {
                i++;
                col++;
            }

            size_t len = i - start;
            if (len == target_len && strncmp(content + start, target, target_len) == 0) {
                if (out_edits != NULL) {
                    out_edits[count].range.start.line = start_line;
                    out_edits[count].range.start.character = start_col;
                    out_edits[count].range.end.line = start_line;
                    out_edits[count].range.end.character = start_col + (int)target_len;
                    out_edits[count].new_text = new_text;
                }
                count++;
            }
            continue;
        }

        if (content[i] == '\n') {
            line++;
            col = 0;
        } else {
            col++;
        }
        i++;
    }

    return count;
}

LspTextEdit* lsp_get_rename_edits(LspServer* server, const char* uri,
                                  LspPosition pos, const char* new_name,
                                  size_t* out_count) {
    assert(server != NULL);
    assert(uri != NULL);
    assert(new_name != NULL);
    assert(out_count != NULL);
    if (!server || !uri || !new_name || !out_count) {
        return NULL;
    }
    *out_count = 0;

    if (!is_valid_identifier_name(new_name)) {
        return NULL;
    }

    LspDocument* doc = lsp_document_find(server, uri);
    if (doc == NULL) {
        return NULL;
    }

    const char* content = string_cstr(doc->content);
    size_t word_start = 0;
    size_t word_end = 0;
    size_t ignored_line_start = 0;
    if (!word_span_at_position(content, pos, &word_start, &word_end, &ignored_line_start)) {
        return NULL;
    }

    size_t word_len = word_end - word_start;
    char* target = arena_alloc(server->arena, word_len + 1);
    memcpy(target, content + word_start, word_len);
    target[word_len] = '\0';

    String* replacement = string_new(server->arena, new_name);
    size_t match_count = collect_identifier_occurrences(server, content, target, replacement, NULL);
    if (match_count == 0) {
        return NULL;
    }

    LspTextEdit* edits = arena_alloc(server->arena, match_count * sizeof(LspTextEdit));
    collect_identifier_occurrences(server, content, target, replacement, edits);
    *out_count = match_count;
    return edits;
}

LspCodeAction* lsp_get_code_actions(LspServer* server, const char* uri,
                                    LspRange range, size_t* out_count) {
    (void)range;
    assert(server != NULL);
    assert(uri != NULL);
    assert(out_count != NULL);
    if (!server || !uri || !out_count) {
        return NULL;
    }
    *out_count = 0;

    LspDocument* doc = lsp_document_find(server, uri);
    if (doc == NULL || !doc->has_errors) {
        return NULL;
    }

    const char* error_msg = "Parse or type error";
    if (doc->checker != NULL && checker_first_error(doc->checker) != NULL) {
        error_msg = checker_first_error(doc->checker);
    }

    LspCodeAction* actions = arena_alloc(server->arena, 2 * sizeof(LspCodeAction));
    size_t count = 0;

    if (strstr(error_msg, "Unhandled Result value") != NULL) {
        actions[count].title = string_new(server->arena, "Handle Result value (match/with/?)");
        actions[count].kind = string_new(server->arena, "quickfix");
        actions[count].command = string_new(server->arena, "fern.applyResultFix");
        count++;
    }

    actions[count].title = string_new(server->arena, "Show Fern diagnostic details");
    actions[count].kind = string_new(server->arena, "quickfix");
    actions[count].command = string_new(server->arena, "fern.showDiagnosticHelp");
    count++;

    *out_count = count;
    return actions;
}

/* ========== Request Handlers ========== */

/**
 * Handle initialize request.
 * @param server The LSP server.
 * @param id Request ID.
 * @param params Request params.
 */
static void handle_initialize(LspServer* server, int64_t id, const char* params) {
    // FERN_STYLE: allow(assertion-density) request handler with simple flow
    (void)params;

    server->initialized = true;
    lsp_log(server, "Initialize request received");

    const char* result =
        "{"
        "\"capabilities\":{"
            "\"textDocumentSync\":{"
                "\"openClose\":true,"
                "\"change\":1"  /* Full sync */
            "},"
            "\"hoverProvider\":true,"
            "\"definitionProvider\":true,"
            "\"completionProvider\":{\"resolveProvider\":false},"
            "\"renameProvider\":true,"
            "\"codeActionProvider\":true"
        "},"
        "\"serverInfo\":{"
            "\"name\":\"fern-lsp\","
            "\"version\":\"" LSP_VERSION "\""
        "}"
        "}";

    String* response = build_response(server->arena, id, result);
    lsp_write_message(string_cstr(response));
}

/**
 * Handle shutdown request.
 * @param server The LSP server.
 * @param id Request ID.
 */
static void handle_shutdown(LspServer* server, int64_t id) {
    // FERN_STYLE: allow(assertion-density) simple request handler
    server->shutdown_requested = true;
    lsp_log(server, "Shutdown request received");

    String* response = build_response(server->arena, id, "null");
    lsp_write_message(string_cstr(response));
}

/**
 * Handle textDocument/didOpen notification.
 * @param server The LSP server.
 * @param params Notification params.
 */
static void handle_did_open(LspServer* server, const char* params) {
    // FERN_STYLE: allow(assertion-density) notification handler with JSON parsing
    /* Extract textDocument.uri and textDocument.text */
    size_t td_pos = json_find_key(params, "textDocument", 0);
    if (td_pos == 0) return;

    String* uri = json_get_string(server->arena, params + td_pos, "uri");
    String* text = json_get_string(server->arena, params + td_pos, "text");
    int64_t version = json_get_int(params + td_pos, "version", 0);

    if (uri != NULL && text != NULL) {
        lsp_document_open(server, string_cstr(uri), string_cstr(text), (int)version);

        /* Publish diagnostics */
        size_t diag_count;
        LspDiagnostic* diags = lsp_get_diagnostics(server, string_cstr(uri), &diag_count);

        /* Build diagnostics JSON */
        String* diag_json = string_new(server->arena, "[");
        for (size_t i = 0; i < diag_count; i++) {
            if (i > 0) diag_json = string_append_cstr(server->arena, diag_json, ",");

            String* escaped_msg = json_escape_string(server->arena,
                                                      string_cstr(diags[i].message));
            String* d = string_format(server->arena,
                "{\"range\":{\"start\":{\"line\":%d,\"character\":%d},"
                "\"end\":{\"line\":%d,\"character\":%d}},"
                "\"severity\":%d,\"source\":\"fern\",\"message\":\"%s\"}",
                diags[i].range.start.line, diags[i].range.start.character,
                diags[i].range.end.line, diags[i].range.end.character,
                diags[i].severity, string_cstr(escaped_msg));
            diag_json = string_concat(server->arena, diag_json, d);
        }
        diag_json = string_append_cstr(server->arena, diag_json, "]");

        String* escaped_uri = json_escape_string(server->arena, string_cstr(uri));
        String* notif_params = string_format(server->arena,
            "{\"uri\":\"%s\",\"diagnostics\":%s}",
            string_cstr(escaped_uri), string_cstr(diag_json));

        String* notif = build_notification(server->arena,
            "textDocument/publishDiagnostics", string_cstr(notif_params));
        lsp_write_message(string_cstr(notif));
    }
}

/**
 * Handle textDocument/didChange notification.
 * @param server The LSP server.
 * @param params Notification params.
 */
static void handle_did_change(LspServer* server, const char* params) {
    // FERN_STYLE: allow(assertion-density) notification handler with JSON parsing
    size_t td_pos = json_find_key(params, "textDocument", 0);
    if (td_pos == 0) return;

    String* uri = json_get_string(server->arena, params + td_pos, "uri");
    int64_t version = json_get_int(params + td_pos, "version", 0);

    /* Find contentChanges array and get the text */
    size_t cc_pos = json_find_key(params, "contentChanges", 0);
    if (cc_pos == 0) return;

    /* Skip to first object in array */
    while (params[cc_pos] != '\0' && params[cc_pos] != '{') cc_pos++;
    if (params[cc_pos] == '\0') return;

    String* text = json_get_string(server->arena, params + cc_pos, "text");

    if (uri != NULL && text != NULL) {
        lsp_document_change(server, string_cstr(uri), string_cstr(text), (int)version);

        /* Publish diagnostics (same as didOpen) */
        size_t diag_count;
        LspDiagnostic* diags = lsp_get_diagnostics(server, string_cstr(uri), &diag_count);

        String* diag_json = string_new(server->arena, "[");
        for (size_t i = 0; i < diag_count; i++) {
            if (i > 0) diag_json = string_append_cstr(server->arena, diag_json, ",");

            String* escaped_msg = json_escape_string(server->arena,
                                                      string_cstr(diags[i].message));
            String* d = string_format(server->arena,
                "{\"range\":{\"start\":{\"line\":%d,\"character\":%d},"
                "\"end\":{\"line\":%d,\"character\":%d}},"
                "\"severity\":%d,\"source\":\"fern\",\"message\":\"%s\"}",
                diags[i].range.start.line, diags[i].range.start.character,
                diags[i].range.end.line, diags[i].range.end.character,
                diags[i].severity, string_cstr(escaped_msg));
            diag_json = string_concat(server->arena, diag_json, d);
        }
        diag_json = string_append_cstr(server->arena, diag_json, "]");

        String* escaped_uri = json_escape_string(server->arena, string_cstr(uri));
        String* notif_params = string_format(server->arena,
            "{\"uri\":\"%s\",\"diagnostics\":%s}",
            string_cstr(escaped_uri), string_cstr(diag_json));

        String* notif = build_notification(server->arena,
            "textDocument/publishDiagnostics", string_cstr(notif_params));
        lsp_write_message(string_cstr(notif));
    }
}

/**
 * Handle textDocument/didClose notification.
 * @param server The LSP server.
 * @param params Notification params.
 */
static void handle_did_close(LspServer* server, const char* params) {
    // FERN_STYLE: allow(assertion-density) simple notification handler
    size_t td_pos = json_find_key(params, "textDocument", 0);
    if (td_pos == 0) return;

    String* uri = json_get_string(server->arena, params + td_pos, "uri");
    if (uri != NULL) {
        lsp_document_close(server, string_cstr(uri));
    }
}

/**
 * Handle textDocument/hover request.
 * @param server The LSP server.
 * @param id Request ID.
 * @param params Request params.
 */
static void handle_hover(LspServer* server, int64_t id, const char* params) {
    // FERN_STYLE: allow(assertion-density) request handler with JSON parsing
    size_t td_pos = json_find_key(params, "textDocument", 0);
    size_t pos_pos = json_find_key(params, "position", 0);

    if (td_pos == 0 || pos_pos == 0) {
        String* response = build_response(server->arena, id, "null");
        lsp_write_message(string_cstr(response));
        return;
    }

    String* uri = json_get_string(server->arena, params + td_pos, "uri");
    int64_t line = json_get_int(params + pos_pos, "line", 0);
    int64_t character = json_get_int(params + pos_pos, "character", 0);

    LspPosition pos = { .line = (int)line, .character = (int)character };
    LspRange range;

    String* hover = lsp_get_hover(server, string_cstr(uri), pos, &range);

    if (hover == NULL) {
        String* response = build_response(server->arena, id, "null");
        lsp_write_message(string_cstr(response));
        return;
    }

    String* escaped_hover = json_escape_string(server->arena, string_cstr(hover));
    String* result = string_format(server->arena,
        "{\"contents\":{\"kind\":\"markdown\",\"value\":\"%s\"},"
        "\"range\":{\"start\":{\"line\":%d,\"character\":%d},"
        "\"end\":{\"line\":%d,\"character\":%d}}}",
        string_cstr(escaped_hover),
        range.start.line, range.start.character,
        range.end.line, range.end.character);

    String* response = build_response(server->arena, id, string_cstr(result));
    lsp_write_message(string_cstr(response));
}

/**
 * Handle textDocument/definition request.
 * @param server The LSP server.
 * @param id Request ID.
 * @param params Request params.
 */
static void handle_definition(LspServer* server, int64_t id, const char* params) {
    // FERN_STYLE: allow(assertion-density) request handler with JSON parsing
    size_t td_pos = json_find_key(params, "textDocument", 0);
    size_t pos_pos = json_find_key(params, "position", 0);

    if (td_pos == 0 || pos_pos == 0) {
        String* response = build_response(server->arena, id, "null");
        lsp_write_message(string_cstr(response));
        return;
    }

    String* uri = json_get_string(server->arena, params + td_pos, "uri");
    int64_t line = json_get_int(params + pos_pos, "line", 0);
    int64_t character = json_get_int(params + pos_pos, "character", 0);

    LspPosition pos = { .line = (int)line, .character = (int)character };

    LspLocation* loc = lsp_get_definition(server, string_cstr(uri), pos);

    if (loc == NULL) {
        String* response = build_response(server->arena, id, "null");
        lsp_write_message(string_cstr(response));
        return;
    }

    String* escaped_uri = json_escape_string(server->arena, string_cstr(loc->uri));
    String* result = string_format(server->arena,
        "{\"uri\":\"%s\","
        "\"range\":{\"start\":{\"line\":%d,\"character\":%d},"
        "\"end\":{\"line\":%d,\"character\":%d}}}",
        string_cstr(escaped_uri),
        loc->range.start.line, loc->range.start.character,
        loc->range.end.line, loc->range.end.character);

    String* response = build_response(server->arena, id, string_cstr(result));
    lsp_write_message(string_cstr(response));
}

/**
 * Handle textDocument/completion request.
 * @param server The LSP server.
 * @param id Request ID.
 * @param params Request params.
 */
static void handle_completion(LspServer* server, int64_t id, const char* params) {
    assert(server != NULL);
    assert(params != NULL);

    size_t td_pos = json_find_key(params, "textDocument", 0);
    size_t pos_pos = json_find_key(params, "position", 0);
    if (td_pos == 0 || pos_pos == 0) {
        String* response = build_response(server->arena, id, "[]");
        lsp_write_message(string_cstr(response));
        return;
    }

    String* uri = json_get_string(server->arena, params + td_pos, "uri");
    int64_t line = json_get_int(params + pos_pos, "line", 0);
    int64_t character = json_get_int(params + pos_pos, "character", 0);
    if (uri == NULL) {
        String* response = build_response(server->arena, id, "[]");
        lsp_write_message(string_cstr(response));
        return;
    }

    size_t count = 0;
    LspPosition pos = {.line = (int)line, .character = (int)character};
    LspCompletionItem* items = lsp_get_completions(server, string_cstr(uri), pos, &count);
    if (items == NULL || count == 0) {
        String* response = build_response(server->arena, id, "[]");
        lsp_write_message(string_cstr(response));
        return;
    }

    String* array_json = string_new(server->arena, "[");
    for (size_t i = 0; i < count; i++) {
        if (i > 0) {
            array_json = string_append_cstr(server->arena, array_json, ",");
        }
        String* escaped_label = json_escape_string(server->arena, string_cstr(items[i].label));
        String* escaped_detail = json_escape_string(server->arena, string_cstr(items[i].detail));
        String* item = string_format(server->arena,
            "{\"label\":\"%s\",\"kind\":%d,\"detail\":\"%s\"}",
            string_cstr(escaped_label), items[i].kind, string_cstr(escaped_detail));
        array_json = string_concat(server->arena, array_json, item);
    }
    array_json = string_append_cstr(server->arena, array_json, "]");

    String* response = build_response(server->arena, id, string_cstr(array_json));
    lsp_write_message(string_cstr(response));
}

/**
 * Handle textDocument/rename request.
 * @param server The LSP server.
 * @param id Request ID.
 * @param params Request params.
 */
static void handle_rename(LspServer* server, int64_t id, const char* params) {
    assert(server != NULL);
    assert(params != NULL);

    size_t td_pos = json_find_key(params, "textDocument", 0);
    size_t pos_pos = json_find_key(params, "position", 0);
    if (td_pos == 0 || pos_pos == 0) {
        String* response = build_response(server->arena, id, "null");
        lsp_write_message(string_cstr(response));
        return;
    }

    String* uri = json_get_string(server->arena, params + td_pos, "uri");
    String* new_name = json_get_string(server->arena, params, "newName");
    int64_t line = json_get_int(params + pos_pos, "line", 0);
    int64_t character = json_get_int(params + pos_pos, "character", 0);
    if (uri == NULL || new_name == NULL) {
        String* response = build_response(server->arena, id, "null");
        lsp_write_message(string_cstr(response));
        return;
    }

    size_t edit_count = 0;
    LspPosition pos = {.line = (int)line, .character = (int)character};
    LspTextEdit* edits = lsp_get_rename_edits(
        server, string_cstr(uri), pos, string_cstr(new_name), &edit_count);
    if (edits == NULL || edit_count == 0) {
        String* response = build_response(server->arena, id, "null");
        lsp_write_message(string_cstr(response));
        return;
    }

    String* edits_json = string_new(server->arena, "[");
    for (size_t i = 0; i < edit_count; i++) {
        if (i > 0) {
            edits_json = string_append_cstr(server->arena, edits_json, ",");
        }
        String* escaped_new_text = json_escape_string(server->arena, string_cstr(edits[i].new_text));
        String* edit_json = string_format(server->arena,
            "{\"range\":{\"start\":{\"line\":%d,\"character\":%d},"
            "\"end\":{\"line\":%d,\"character\":%d}},"
            "\"newText\":\"%s\"}",
            edits[i].range.start.line, edits[i].range.start.character,
            edits[i].range.end.line, edits[i].range.end.character,
            string_cstr(escaped_new_text));
        edits_json = string_concat(server->arena, edits_json, edit_json);
    }
    edits_json = string_append_cstr(server->arena, edits_json, "]");

    String* escaped_uri = json_escape_string(server->arena, string_cstr(uri));
    String* result = string_format(server->arena,
        "{\"changes\":{\"%s\":%s}}",
        string_cstr(escaped_uri), string_cstr(edits_json));

    String* response = build_response(server->arena, id, string_cstr(result));
    lsp_write_message(string_cstr(response));
}

/**
 * Handle textDocument/codeAction request.
 * @param server The LSP server.
 * @param id Request ID.
 * @param params Request params.
 */
static void handle_code_action(LspServer* server, int64_t id, const char* params) {
    assert(server != NULL);
    assert(params != NULL);

    size_t td_pos = json_find_key(params, "textDocument", 0);
    if (td_pos == 0) {
        String* response = build_response(server->arena, id, "[]");
        lsp_write_message(string_cstr(response));
        return;
    }

    String* uri = json_get_string(server->arena, params + td_pos, "uri");
    if (uri == NULL) {
        String* response = build_response(server->arena, id, "[]");
        lsp_write_message(string_cstr(response));
        return;
    }

    LspRange range = {
        .start = {.line = 0, .character = 0},
        .end = {.line = 0, .character = 0},
    };
    size_t range_pos = json_find_key(params, "range", 0);
    if (range_pos != 0) {
        size_t start_pos = json_find_key(params + range_pos, "start", 0);
        size_t end_pos = json_find_key(params + range_pos, "end", 0);
        if (start_pos != 0) {
            range.start.line = (int)json_get_int(params + range_pos + start_pos, "line", 0);
            range.start.character = (int)json_get_int(params + range_pos + start_pos, "character", 0);
        }
        if (end_pos != 0) {
            range.end.line = (int)json_get_int(params + range_pos + end_pos, "line", 0);
            range.end.character = (int)json_get_int(params + range_pos + end_pos, "character", 0);
        }
    }

    size_t count = 0;
    LspCodeAction* actions = lsp_get_code_actions(server, string_cstr(uri), range, &count);
    if (actions == NULL || count == 0) {
        String* response = build_response(server->arena, id, "[]");
        lsp_write_message(string_cstr(response));
        return;
    }

    String* actions_json = string_new(server->arena, "[");
    for (size_t i = 0; i < count; i++) {
        if (i > 0) {
            actions_json = string_append_cstr(server->arena, actions_json, ",");
        }
        String* escaped_title = json_escape_string(server->arena, string_cstr(actions[i].title));
        String* escaped_kind = json_escape_string(server->arena, string_cstr(actions[i].kind));
        String* escaped_command = json_escape_string(server->arena, string_cstr(actions[i].command));
        String* action_json = string_format(server->arena,
            "{\"title\":\"%s\",\"kind\":\"%s\","
            "\"command\":{\"title\":\"%s\",\"command\":\"%s\"}}",
            string_cstr(escaped_title), string_cstr(escaped_kind),
            string_cstr(escaped_title), string_cstr(escaped_command));
        actions_json = string_concat(server->arena, actions_json, action_json);
    }
    actions_json = string_append_cstr(server->arena, actions_json, "]");

    String* response = build_response(server->arena, id, string_cstr(actions_json));
    lsp_write_message(string_cstr(response));
}

/* ========== Server Lifecycle ========== */

/**
 * Create a new LSP server instance.
 * @param arena Arena for server allocations.
 * @param log_file Path to log file, or NULL to disable logging.
 * @return New LSP server instance.
 */
LspServer* lsp_server_new(Arena* arena, const char* log_file) {
    // FERN_STYLE: allow(assertion-density) simple allocation and initialization
    assert(arena != NULL);

    LspServer* server = arena_alloc(arena, sizeof(LspServer));
    server->arena = arena;
    server->documents = LspDocumentVec_new(arena);
    server->initialized = false;
    server->shutdown_requested = false;

    if (log_file != NULL) {
        server->log = fopen(log_file, "w");
    } else {
        server->log = NULL;
    }

    lsp_log(server, "Fern LSP server started");
    return server;
}

/**
 * Run the LSP server main loop.
 * @param server The LSP server to run.
 * @return 0 on normal shutdown, non-zero on error.
 */
int lsp_server_run(LspServer* server) {
    // FERN_STYLE: allow(assertion-density) main event loop
    // FERN_STYLE: allow(function-length) single dispatch loop over LSP methods
    // FERN_STYLE: allow(bounded-loops) LSP server runs until exit notification
    assert(server != NULL);

    while (true) {
        /* Read next message */
        Arena* msg_arena = arena_create(64 * 1024);
        String* msg = lsp_read_message(msg_arena);

        if (msg == NULL) {
            lsp_log(server, "EOF or error reading message");
            arena_destroy(msg_arena);
            break;
        }

        lsp_log(server, "Received: %s", string_cstr(msg));

        /* Parse method */
        String* method = json_get_method(msg_arena, string_cstr(msg));
        int64_t id = json_get_id(string_cstr(msg));

        if (method == NULL) {
            lsp_log(server, "No method in message");
            arena_destroy(msg_arena);
            continue;
        }

        const char* method_str = string_cstr(method);

        /* Route to handler */
        if (strcmp(method_str, "initialize") == 0) {
            handle_initialize(server, id, string_cstr(msg));
        } else if (strcmp(method_str, "initialized") == 0) {
            /* Notification - no response needed */
            lsp_log(server, "Client initialized");
        } else if (strcmp(method_str, "shutdown") == 0) {
            handle_shutdown(server, id);
        } else if (strcmp(method_str, "exit") == 0) {
            lsp_log(server, "Exit notification received");
            arena_destroy(msg_arena);
            break;
        } else if (strcmp(method_str, "textDocument/didOpen") == 0) {
            handle_did_open(server, string_cstr(msg));
        } else if (strcmp(method_str, "textDocument/didChange") == 0) {
            handle_did_change(server, string_cstr(msg));
        } else if (strcmp(method_str, "textDocument/didClose") == 0) {
            handle_did_close(server, string_cstr(msg));
        } else if (strcmp(method_str, "textDocument/hover") == 0) {
            handle_hover(server, id, string_cstr(msg));
        } else if (strcmp(method_str, "textDocument/definition") == 0) {
            handle_definition(server, id, string_cstr(msg));
        } else if (strcmp(method_str, "textDocument/completion") == 0) {
            handle_completion(server, id, string_cstr(msg));
        } else if (strcmp(method_str, "textDocument/rename") == 0) {
            handle_rename(server, id, string_cstr(msg));
        } else if (strcmp(method_str, "textDocument/codeAction") == 0) {
            handle_code_action(server, id, string_cstr(msg));
        } else {
            /* Unknown method - return error for requests, ignore notifications */
            if (id >= 0) {
                String* err = build_error(server->arena, id, -32601,
                    "Method not found");
                lsp_write_message(string_cstr(err));
            }
            lsp_log(server, "Unknown method: %s", method_str);
        }

        arena_destroy(msg_arena);
    }

    return server->shutdown_requested ? 0 : 1;
}

/**
 * Free LSP server resources.
 * @param server The LSP server to free, or NULL (no-op).
 */
void lsp_server_free(LspServer* server) {
    // FERN_STYLE: allow(assertion-density) cleanup function with null check
    if (server == NULL) return;

    if (server->log != NULL) {
        lsp_log(server, "Fern LSP server stopped");
        fclose(server->log);
    }
}
