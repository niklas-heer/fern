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

    /* For now, report a single diagnostic with the first error message.
     * A full implementation would expose error iteration from the checker. */
    const char* error_msg = NULL;
    if (doc->checker != NULL) {
        error_msg = checker_first_error(doc->checker);
    }
    if (error_msg == NULL) {
        error_msg = "Parse or type error";
    }

    LspDiagnostic* diags = arena_alloc(server->arena, sizeof(LspDiagnostic));

    /* Report error at start of file for now */
    diags[0].range.start.line = 0;
    diags[0].range.start.character = 0;
    diags[0].range.end.line = 0;
    diags[0].range.end.character = 1;
    diags[0].severity = LSP_SEVERITY_ERROR;
    diags[0].message = string_new(server->arena, error_msg);
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

    /* Find the word at the position */
    const char* content = string_cstr(doc->content);
    size_t line_start = 0;
    int current_line = 0;

    /* Find start of the target line */
    for (size_t i = 0; content[i] != '\0' && current_line < pos.line; i++) {
        if (content[i] == '\n') {
            current_line++;
            line_start = i + 1;
        }
    }

    /* Find word boundaries */
    size_t word_start = line_start + pos.character;
    size_t word_end = word_start;

    /* Move back to start of word */
    while (word_start > line_start) {
        char c = content[word_start - 1];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_')) {
            break;
        }
        word_start--;
    }

    /* Move forward to end of word */
    while (content[word_end] != '\0' && content[word_end] != '\n') {
        char c = content[word_end];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_')) {
            break;
        }
        word_end++;
    }

    if (word_end <= word_start) {
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

    /* Find the word at position (same logic as hover) */
    const char* content = string_cstr(doc->content);
    size_t line_start = 0;
    int current_line = 0;

    for (size_t i = 0; content[i] != '\0' && current_line < pos.line; i++) {
        if (content[i] == '\n') {
            current_line++;
            line_start = i + 1;
        }
    }

    size_t word_start = line_start + pos.character;
    size_t word_end = word_start;

    while (word_start > line_start) {
        char c = content[word_start - 1];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_')) {
            break;
        }
        word_start--;
    }

    while (content[word_end] != '\0' && content[word_end] != '\n') {
        char c = content[word_end];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_')) {
            break;
        }
        word_end++;
    }

    if (word_end <= word_start) {
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
                loc->range.end = loc->range.start;
                return loc;
            }
        }
    }

    return NULL;
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
            "\"definitionProvider\":true"
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
