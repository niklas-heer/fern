/* Fern Type Environment Implementation
 *
 * Uses a simple linked-list of scopes, each with a linear search
 * for bindings. This is efficient enough for typical program sizes.
 * Could be optimized with hash tables if needed.
 */

#include "type_env.h"
#include <string.h>

/* A single binding (name -> type) */
typedef struct Binding {
    String* name;
    Type* type;
    struct Binding* next;
} Binding;

/* A scope contains bindings and a link to the enclosing scope */
typedef struct Scope {
    Binding* bindings;       /* Variable bindings */
    Binding* type_bindings;  /* Type definitions */
    struct Scope* parent;    /* Enclosing scope */
    int depth;               /* Scope depth (0 = global) */
} Scope;

/* The type environment */
struct TypeEnv {
    Arena* arena;
    Scope* current;  /* Current (innermost) scope */
};

/* ========== Helper Functions ========== */

static Scope* scope_new(Arena* arena, Scope* parent) {
    Scope* scope = arena_alloc(arena, sizeof(Scope));
    scope->bindings = NULL;
    scope->type_bindings = NULL;
    scope->parent = parent;
    scope->depth = parent ? parent->depth + 1 : 0;
    return scope;
}

static Binding* binding_new(Arena* arena, String* name, Type* type) {
    Binding* binding = arena_alloc(arena, sizeof(Binding));
    binding->name = name;
    binding->type = type;
    binding->next = NULL;
    return binding;
}

/* Search for a binding in a single scope */
static Type* scope_lookup(Scope* scope, String* name) {
    for (Binding* b = scope->bindings; b != NULL; b = b->next) {
        if (string_equal(b->name, name)) {
            return b->type;
        }
    }
    return NULL;
}

/* Search for a type binding in a single scope */
static Type* scope_lookup_type(Scope* scope, String* name) {
    for (Binding* b = scope->type_bindings; b != NULL; b = b->next) {
        if (string_equal(b->name, name)) {
            return b->type;
        }
    }
    return NULL;
}

/* ========== Type Environment Creation ========== */

TypeEnv* type_env_new(Arena* arena) {
    TypeEnv* env = arena_alloc(arena, sizeof(TypeEnv));
    env->arena = arena;
    env->current = scope_new(arena, NULL);  /* Global scope */
    return env;
}

/* ========== Scope Management ========== */

void type_env_push_scope(TypeEnv* env) {
    env->current = scope_new(env->arena, env->current);
}

void type_env_pop_scope(TypeEnv* env) {
    if (env->current->parent != NULL) {
        env->current = env->current->parent;
    }
    /* Don't pop the global scope */
}

int type_env_depth(TypeEnv* env) {
    return env->current->depth;
}

/* ========== Variable Bindings ========== */

void type_env_define(TypeEnv* env, String* name, Type* type) {
    Binding* binding = binding_new(env->arena, name, type);
    binding->next = env->current->bindings;
    env->current->bindings = binding;
}

Type* type_env_lookup(TypeEnv* env, String* name) {
    /* Search from innermost to outermost scope */
    for (Scope* scope = env->current; scope != NULL; scope = scope->parent) {
        Type* found = scope_lookup(scope, name);
        if (found) {
            return found;
        }
    }
    return NULL;
}

bool type_env_is_defined(TypeEnv* env, String* name) {
    return type_env_lookup(env, name) != NULL;
}

bool type_env_is_defined_in_current_scope(TypeEnv* env, String* name) {
    return scope_lookup(env->current, name) != NULL;
}

/* ========== Type Definitions ========== */

void type_env_define_type(TypeEnv* env, String* name, Type* type) {
    Binding* binding = binding_new(env->arena, name, type);
    binding->next = env->current->type_bindings;
    env->current->type_bindings = binding;
}

Type* type_env_lookup_type(TypeEnv* env, String* name) {
    /* Search from innermost to outermost scope */
    for (Scope* scope = env->current; scope != NULL; scope = scope->parent) {
        Type* found = scope_lookup_type(scope, name);
        if (found) {
            return found;
        }
    }
    return NULL;
}
