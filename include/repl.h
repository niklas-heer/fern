/**
 * @file repl.h
 * @brief Fern REPL - Interactive Read-Eval-Print Loop.
 *
 * The REPL provides an interactive environment for evaluating Fern
 * expressions and statements. It maintains state across inputs, allowing
 * users to define variables and functions that persist through the session.
 *
 * Features:
 * - Line editing with arrow keys, backspace, delete (via linenoise)
 * - Command history with persistence across sessions
 * - Tab completion for keywords and built-in functions
 * - Special commands: :help, :quit, :type, :clear
 * - Type display for evaluated expressions
 *
 * @example
 *     Arena* arena = arena_create(4 * 1024 * 1024);
 *     Repl* repl = repl_new(arena);
 *     int exit_code = repl_run(repl);
 *     arena_destroy(arena);
 *
 * @see checker.h, parser.h
 */

#ifndef FERN_REPL_H
#define FERN_REPL_H

#include "arena.h"
#include "checker.h"
#include "type_env.h"
#include <stdbool.h>

/** @brief Opaque REPL state. */
typedef struct Repl Repl;

/* ========== REPL Creation ========== */

/**
 * @brief Create a new REPL instance.
 *
 * Initializes the REPL with a fresh type environment and loads
 * command history from ~/.fern_history if it exists.
 *
 * @param arena Memory arena for allocations (must not be NULL)
 * @return New Repl instance, or NULL on failure
 */
Repl* repl_new(Arena* arena);

/* ========== REPL Execution ========== */

/**
 * @brief Run the REPL main loop.
 *
 * Starts the interactive loop, reading lines from stdin until
 * the user exits with :quit or EOF (Ctrl+D).
 *
 * @param repl REPL instance (must not be NULL)
 * @return Exit code (0 for normal exit, non-zero on error)
 */
int repl_run(Repl* repl);

/**
 * @brief Evaluate a single line of input.
 *
 * Parses, type checks, and evaluates the input. For expressions,
 * prints the result with its type. For statements, executes them
 * and updates the environment.
 *
 * @param repl REPL instance (must not be NULL)
 * @param line Input line to evaluate (must not be NULL)
 * @return true if evaluation succeeded, false on error
 *
 * @note Errors are printed to stderr but don't stop the REPL.
 */
bool repl_eval_line(Repl* repl, const char* line);

/* ========== REPL State ========== */

/**
 * @brief Get the REPL's type environment.
 *
 * Useful for testing. Contains all defined variables and their types.
 *
 * @param repl REPL instance (must not be NULL)
 * @return Type environment
 */
TypeEnv* repl_type_env(Repl* repl);

/**
 * @brief Check if the REPL should exit.
 *
 * Returns true after :quit command or EOF.
 *
 * @param repl REPL instance (must not be NULL)
 * @return true if REPL should exit
 */
bool repl_should_exit(Repl* repl);

#endif /* FERN_REPL_H */
