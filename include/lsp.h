/**
 * Fern Language Server Protocol (LSP) Implementation
 *
 * Provides IDE features via the Language Server Protocol:
 * - Diagnostics (inline error reporting)
 * - Hover (type information on hover)
 * - Go-to-definition (jump to symbol definitions)
 *
 * The LSP reuses existing compiler infrastructure (lexer, parser, checker)
 * so new language features automatically work without LSP changes.
 */

#ifndef FERN_LSP_H
#define FERN_LSP_H

#include "arena.h"
#include "fern_string.h"
#include "ast.h"
#include "type.h"
#include "checker.h"
#include <stdio.h>
#include <stdbool.h>

/* ========== LSP Server State ========== */

/**
 * Document state cached by the LSP server.
 * Stores parsed AST, type information, and diagnostics for open files.
 */
typedef struct {
    String* uri;           /* Document URI (file://...) */
    String* content;       /* Current document content */
    int version;           /* Document version (incremented on changes) */
    StmtVec* ast;          /* Parsed AST (NULL if parse failed) */
    Checker* checker;      /* Type checker with inferred types */
    bool has_errors;       /* Whether parsing/checking produced errors */
} LspDocument;

/**
 * Dynamic array of LSP documents.
 */
typedef struct {
    LspDocument** data;
    size_t len;
    size_t cap;
} LspDocumentVec;

/**
 * LSP server state.
 */
typedef struct {
    Arena* arena;              /* Memory arena for allocations */
    LspDocumentVec* documents; /* Open documents */
    bool initialized;          /* Whether initialize handshake completed */
    bool shutdown_requested;   /* Whether shutdown was requested */
    FILE* log;                 /* Log file for debugging (NULL to disable) */
} LspServer;

/* ========== LSP Position Types ========== */

/**
 * LSP position (0-indexed line and character).
 */
typedef struct {
    int line;       /* 0-indexed line number */
    int character;  /* 0-indexed character offset (UTF-16 code units) */
} LspPosition;

/**
 * LSP range (start and end positions).
 */
typedef struct {
    LspPosition start;
    LspPosition end;
} LspRange;

/**
 * LSP location (URI + range).
 */
typedef struct {
    String* uri;
    LspRange range;
} LspLocation;

/**
 * LSP diagnostic severity levels.
 */
typedef enum {
    LSP_SEVERITY_ERROR = 1,
    LSP_SEVERITY_WARNING = 2,
    LSP_SEVERITY_INFORMATION = 3,
    LSP_SEVERITY_HINT = 4
} LspSeverity;

/**
 * LSP diagnostic (error/warning with location).
 */
typedef struct {
    LspRange range;
    LspSeverity severity;
    String* message;
    String* source;  /* "fern" */
} LspDiagnostic;

/**
 * LSP completion item.
 */
typedef struct {
    String* label;
    String* detail;
    int kind;
} LspCompletionItem;

/**
 * LSP text edit.
 */
typedef struct {
    LspRange range;
    String* new_text;
} LspTextEdit;

/**
 * LSP code action.
 */
typedef struct {
    String* title;
    String* kind;
    String* command;
} LspCodeAction;

/* ========== Server Lifecycle ========== */

/**
 * Create a new LSP server.
 * @param arena Arena for memory allocation.
 * @param log_file Optional log file path (NULL to disable logging).
 * @return New LSP server instance.
 */
LspServer* lsp_server_new(Arena* arena, const char* log_file);

/**
 * Run the LSP server main loop.
 * Reads JSON-RPC messages from stdin, writes responses to stdout.
 * Returns when the server receives an exit notification.
 * @param server The LSP server.
 * @return 0 on clean exit, 1 on error.
 */
int lsp_server_run(LspServer* server);

/**
 * Free LSP server resources.
 * @param server The LSP server to free.
 */
void lsp_server_free(LspServer* server);

/* ========== Document Management ========== */

/**
 * Open a document (textDocument/didOpen).
 * @param server The LSP server.
 * @param uri Document URI.
 * @param content Document content.
 * @param version Document version.
 */
void lsp_document_open(LspServer* server, const char* uri,
                       const char* content, int version);

/**
 * Update a document (textDocument/didChange).
 * @param server The LSP server.
 * @param uri Document URI.
 * @param content New document content.
 * @param version New document version.
 */
void lsp_document_change(LspServer* server, const char* uri,
                         const char* content, int version);

/**
 * Close a document (textDocument/didClose).
 * @param server The LSP server.
 * @param uri Document URI.
 */
void lsp_document_close(LspServer* server, const char* uri);

/**
 * Find a document by URI.
 * @param server The LSP server.
 * @param uri Document URI to find.
 * @return The document, or NULL if not found.
 */
LspDocument* lsp_document_find(LspServer* server, const char* uri);

/* ========== LSP Features ========== */

/**
 * Get diagnostics for a document.
 * @param server The LSP server.
 * @param uri Document URI.
 * @param out_count Output: number of diagnostics.
 * @return Array of diagnostics (allocated from server arena).
 */
LspDiagnostic* lsp_get_diagnostics(LspServer* server, const char* uri,
                                    size_t* out_count);

/**
 * Get hover information at a position.
 * @param server The LSP server.
 * @param uri Document URI.
 * @param pos Position to query.
 * @param out_range Output: range of the hovered symbol.
 * @return Hover content (Markdown), or NULL if nothing to show.
 */
String* lsp_get_hover(LspServer* server, const char* uri,
                      LspPosition pos, LspRange* out_range);

/**
 * Get definition location for symbol at position.
 * @param server The LSP server.
 * @param uri Document URI.
 * @param pos Position to query.
 * @return Definition location, or NULL if not found.
 */
LspLocation* lsp_get_definition(LspServer* server, const char* uri,
                                 LspPosition pos);

/**
 * Get completion items at a position.
 * @param server The LSP server.
 * @param uri Document URI.
 * @param pos Position to query.
 * @param out_count Output: number of completion items.
 * @return Array of completion items, or NULL if none.
 */
LspCompletionItem* lsp_get_completions(LspServer* server, const char* uri,
                                       LspPosition pos, size_t* out_count);

/**
 * Get rename edits for symbol at a position.
 * @param server The LSP server.
 * @param uri Document URI.
 * @param pos Position of symbol to rename.
 * @param new_name Replacement identifier.
 * @param out_count Output: number of text edits.
 * @return Array of edits, or NULL if rename is unavailable.
 */
LspTextEdit* lsp_get_rename_edits(LspServer* server, const char* uri,
                                  LspPosition pos, const char* new_name,
                                  size_t* out_count);

/**
 * Get code actions for a range.
 * @param server The LSP server.
 * @param uri Document URI.
 * @param range Requested range.
 * @param out_count Output: number of code actions.
 * @return Array of code actions, or NULL if none.
 */
LspCodeAction* lsp_get_code_actions(LspServer* server, const char* uri,
                                    LspRange range, size_t* out_count);

/* ========== JSON-RPC Helpers ========== */

/**
 * Read a JSON-RPC message from stdin.
 * Handles Content-Length header parsing.
 * @param arena Arena for allocating the message.
 * @return Message content, or NULL on EOF/error.
 */
String* lsp_read_message(Arena* arena);

/**
 * Write a JSON-RPC message to stdout.
 * Adds Content-Length header automatically.
 * @param content Message content (JSON).
 */
void lsp_write_message(const char* content);

#endif /* FERN_LSP_H */
