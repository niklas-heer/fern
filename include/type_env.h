/* Fern Type Environment
 *
 * Manages type bindings for variables, functions, and type definitions.
 * Supports nested scopes with proper shadowing semantics.
 */

#ifndef FERN_TYPE_ENV_H
#define FERN_TYPE_ENV_H

#include "arena.h"
#include "fern_string.h"
#include "type.h"
#include <stdbool.h>

/* Forward declaration */
typedef struct TypeEnv TypeEnv;

/* ========== Type Environment Creation ========== */

/* Create a new type environment */
TypeEnv* type_env_new(Arena* arena);

/* ========== Scope Management ========== */

/* Push a new scope (entering a block/function) */
void type_env_push_scope(TypeEnv* env);

/* Pop the current scope (exiting a block/function) */
void type_env_pop_scope(TypeEnv* env);

/* Get the current scope depth (0 = global) */
int type_env_depth(TypeEnv* env);

/* ========== Variable Bindings ========== */

/* Define a variable with its type in the current scope */
void type_env_define(TypeEnv* env, String* name, Type* type);

/* Look up a variable's type (searches all scopes, innermost first) */
Type* type_env_lookup(TypeEnv* env, String* name);

/* Check if a variable is defined (in any scope) */
bool type_env_is_defined(TypeEnv* env, String* name);

/* Check if a variable is defined in the current scope only */
bool type_env_is_defined_in_current_scope(TypeEnv* env, String* name);

/* ========== Type Definitions ========== */

/* Define a named type (sum type, record, newtype) */
void type_env_define_type(TypeEnv* env, String* name, Type* type);

/* Look up a type definition */
Type* type_env_lookup_type(TypeEnv* env, String* name);

#endif /* FERN_TYPE_ENV_H */
