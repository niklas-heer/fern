/* Fern Type Environment Implementation
 *
 * Uses a simple linked-list of scopes, each with a linear search
 * for bindings. This is efficient enough for typical program sizes.
 * Could be optimized with hash tables if needed.
 */

#include "type_env.h"
#include <assert.h>
#include <string.h>

/** A single binding (name -> type). */
typedef struct Binding {
    String* name;
    Type* type;
    struct Binding* next;
} Binding;

/** A scope contains bindings and a link to the enclosing scope. */
typedef struct Scope {
    Binding* bindings;       /* Variable bindings */
    Binding* type_bindings;  /* Type definitions */
    struct Scope* parent;    /* Enclosing scope */
    int depth;               /* Scope depth (0 = global) */
} Scope;

/** The type environment. */
struct TypeEnv {
    Arena* arena;
    Scope* current;  /* Current (innermost) scope */
};

/* ========== Helper Functions ========== */

/**
 * Create a new scope with given parent.
 * @param arena The arena to allocate from.
 * @param parent The parent scope (may be NULL for global).
 * @return The new scope.
 */
static Scope* scope_new(Arena* arena, Scope* parent) {
    assert(arena != NULL);
    Scope* scope = arena_alloc(arena, sizeof(Scope));
    assert(scope != NULL);
    scope->bindings = NULL;
    scope->type_bindings = NULL;
    scope->parent = parent;
    scope->depth = parent ? parent->depth + 1 : 0;
    return scope;
}

/**
 * Create a new binding.
 * @param arena The arena to allocate from.
 * @param name The binding name.
 * @param type The bound type.
 * @return The new binding.
 */
static Binding* binding_new(Arena* arena, String* name, Type* type) {
    assert(arena != NULL);
    Binding* binding = arena_alloc(arena, sizeof(Binding));
    assert(binding != NULL);
    binding->name = name;
    binding->type = type;
    binding->next = NULL;
    return binding;
}

/**
 * Search for a variable binding in a single scope.
 * @param scope The scope to search.
 * @param name The variable name to find.
 * @return The bound type, or NULL if not found.
 */
static Type* scope_lookup(Scope* scope, String* name) {
    assert(scope != NULL);
    assert(name != NULL);
    for (Binding* b = scope->bindings; b != NULL; b = b->next) {
        if (string_equal(b->name, name)) {
            return b->type;
        }
    }
    return NULL;
}

/**
 * Search for a type binding in a single scope.
 * @param scope The scope to search.
 * @param name The type name to find.
 * @return The type definition, or NULL if not found.
 */
static Type* scope_lookup_type(Scope* scope, String* name) {
    assert(scope != NULL);
    assert(name != NULL);
    for (Binding* b = scope->type_bindings; b != NULL; b = b->next) {
        if (string_equal(b->name, name)) {
            return b->type;
        }
    }
    return NULL;
}

/* ========== Type Environment Creation ========== */

/**
 * Create a new type environment with a global scope.
 * @param arena The arena to allocate from.
 * @return The new type environment.
 */
TypeEnv* type_env_new(Arena* arena) {
    assert(arena != NULL);
    TypeEnv* env = arena_alloc(arena, sizeof(TypeEnv));
    assert(env != NULL);
    env->arena = arena;
    env->current = scope_new(arena, NULL);  /* Global scope */
    return env;
}

/* ========== Scope Management ========== */

/**
 * Push a new scope onto the environment.
 * @param env The type environment.
 */
void type_env_push_scope(TypeEnv* env) {
    assert(env != NULL);
    assert(env->arena != NULL);
    env->current = scope_new(env->arena, env->current);
}

/**
 * Pop the current scope (does not pop global scope).
 * @param env The type environment.
 */
void type_env_pop_scope(TypeEnv* env) {
    assert(env != NULL);
    assert(env->current != NULL);
    if (env->current->parent != NULL) {
        env->current = env->current->parent;
    }
    /* Don't pop the global scope */
}

/**
 * Get the current scope depth (0 = global).
 * @param env The type environment.
 * @return The current scope depth.
 */
int type_env_depth(TypeEnv* env) {
    assert(env != NULL);
    assert(env->current != NULL);
    return env->current->depth;
}

/* ========== Variable Bindings ========== */

/**
 * Define a variable with a type in the current scope.
 * @param env The type environment.
 * @param name The variable name.
 * @param type The variable's type.
 */
void type_env_define(TypeEnv* env, String* name, Type* type) {
    assert(env != NULL);
    assert(name != NULL);
    Binding* binding = binding_new(env->arena, name, type);
    binding->next = env->current->bindings;
    env->current->bindings = binding;
}

/**
 * Look up a variable's type in all scopes (innermost first).
 * @param env The type environment.
 * @param name The variable name to look up.
 * @return The variable's type, or NULL if not found.
 */
Type* type_env_lookup(TypeEnv* env, String* name) {
    assert(env != NULL);
    assert(name != NULL);
    /* Search from innermost to outermost scope */
    for (Scope* scope = env->current; scope != NULL; scope = scope->parent) {
        Type* found = scope_lookup(scope, name);
        if (found) {
            return found;
        }
    }
    return NULL;
}

/**
 * Check if a variable is defined in any scope.
 * @param env The type environment.
 * @param name The variable name to check.
 * @return True if defined, false otherwise.
 */
bool type_env_is_defined(TypeEnv* env, String* name) {
    assert(env != NULL);
    assert(name != NULL);
    return type_env_lookup(env, name) != NULL;
}

/**
 * Check if a variable is defined in the current scope only.
 * @param env The type environment.
 * @param name The variable name to check.
 * @return True if defined in current scope, false otherwise.
 */
bool type_env_is_defined_in_current_scope(TypeEnv* env, String* name) {
    assert(env != NULL);
    assert(name != NULL);
    return scope_lookup(env->current, name) != NULL;
}

/* ========== Type Definitions ========== */

/**
 * Define a named type in the current scope.
 * @param env The type environment.
 * @param name The type name.
 * @param type The type definition.
 */
void type_env_define_type(TypeEnv* env, String* name, Type* type) {
    assert(env != NULL);
    assert(name != NULL);
    Binding* binding = binding_new(env->arena, name, type);
    binding->next = env->current->type_bindings;
    env->current->type_bindings = binding;
}

/**
 * Look up a named type in all scopes (innermost first).
 * @param env The type environment.
 * @param name The type name to look up.
 * @return The type definition, or NULL if not found.
 */
Type* type_env_lookup_type(TypeEnv* env, String* name) {
    assert(env != NULL);
    assert(name != NULL);
    /* Search from innermost to outermost scope */
    for (Scope* scope = env->current; scope != NULL; scope = scope->parent) {
        Type* found = scope_lookup_type(scope, name);
        if (found) {
            return found;
        }
    }
    return NULL;
}
