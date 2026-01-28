/* Fern Type System Implementation */

#include "type.h"
#include <stdio.h>
#include <string.h>

/** Global counter for fresh type variables. */
static int g_type_var_counter = 0;

/**
 * Generate a fresh unique type variable ID.
 * @return A new unique type variable ID.
 */
int type_fresh_var_id(void) {
    // FERN_STYLE: allow(assertion-density) trivial counter increment
    return g_type_var_counter++;
}

/* ========== Primitive Types ========== */

/**
 * Create an Int type.
 * @param arena The arena to allocate from.
 * @return The Int type.
 */
Type* type_int(Arena* arena) {
    // FERN_STYLE: allow(assertion-density) trivial type constructor
    Type* t = arena_alloc(arena, sizeof(Type));
    t->kind = TYPE_INT;
    return t;
}

/**
 * Create a Float type.
 * @param arena The arena to allocate from.
 * @return The Float type.
 */
Type* type_float(Arena* arena) {
    // FERN_STYLE: allow(assertion-density) trivial type constructor
    Type* t = arena_alloc(arena, sizeof(Type));
    t->kind = TYPE_FLOAT;
    return t;
}

/**
 * Create a String type.
 * @param arena The arena to allocate from.
 * @return The String type.
 */
Type* type_string(Arena* arena) {
    // FERN_STYLE: allow(assertion-density) trivial type constructor
    Type* t = arena_alloc(arena, sizeof(Type));
    t->kind = TYPE_STRING;
    return t;
}

/**
 * Create a Bool type.
 * @param arena The arena to allocate from.
 * @return The Bool type.
 */
Type* type_bool(Arena* arena) {
    // FERN_STYLE: allow(assertion-density) trivial type constructor
    Type* t = arena_alloc(arena, sizeof(Type));
    t->kind = TYPE_BOOL;
    return t;
}

/**
 * Create a Unit type (()).
 * @param arena The arena to allocate from.
 * @return The Unit type.
 */
Type* type_unit(Arena* arena) {
    // FERN_STYLE: allow(assertion-density) trivial type constructor
    Type* t = arena_alloc(arena, sizeof(Type));
    t->kind = TYPE_UNIT;
    return t;
}

/* ========== Type Variable ========== */

/**
 * Create a type variable with given name and ID.
 * @param arena The arena to allocate from.
 * @param name The variable name.
 * @param id The unique variable ID.
 * @return The type variable.
 */
Type* type_var(Arena* arena, String* name, int id) {
    // FERN_STYLE: allow(assertion-density) trivial type constructor
    Type* t = arena_alloc(arena, sizeof(Type));
    t->kind = TYPE_VAR;
    t->data.var.name = name;
    t->data.var.id = id;
    t->data.var.bound = NULL;
    return t;
}

/* ========== Type Constructor ========== */

/**
 * Create a type constructor (e.g., List, Result).
 * @param arena The arena to allocate from.
 * @param name The constructor name.
 * @param args The type arguments (may be NULL).
 * @return The type constructor.
 */
Type* type_con(Arena* arena, String* name, TypeVec* args) {
    // FERN_STYLE: allow(assertion-density) trivial type constructor
    Type* t = arena_alloc(arena, sizeof(Type));
    t->kind = TYPE_CON;
    t->data.con.name = name;
    t->data.con.args = args;
    return t;
}

/* ========== Function Type ========== */

/**
 * Create a function type with parameters and result type.
 * @param arena The arena to allocate from.
 * @param params The parameter types.
 * @param result The result type.
 * @return The function type.
 */
Type* type_fn(Arena* arena, TypeVec* params, Type* result) {
    // FERN_STYLE: allow(assertion-density) trivial type constructor
    Type* t = arena_alloc(arena, sizeof(Type));
    t->kind = TYPE_FN;
    t->data.fn.params = params;
    t->data.fn.result = result;
    return t;
}

/* ========== Tuple Type ========== */

/**
 * Create a tuple type with element types.
 * @param arena The arena to allocate from.
 * @param elements The element types.
 * @return The tuple type.
 */
Type* type_tuple(Arena* arena, TypeVec* elements) {
    // FERN_STYLE: allow(assertion-density) trivial type constructor
    Type* t = arena_alloc(arena, sizeof(Type));
    t->kind = TYPE_TUPLE;
    t->data.tuple.elements = elements;
    return t;
}

/* ========== Error Type ========== */

/**
 * Create an error type with a message.
 * @param arena The arena to allocate from.
 * @param message The error message.
 * @return The error type.
 */
Type* type_error(Arena* arena, String* message) {
    // FERN_STYLE: allow(assertion-density) trivial type constructor
    Type* t = arena_alloc(arena, sizeof(Type));
    t->kind = TYPE_ERROR;
    t->data.error_msg = message;
    return t;
}

/* ========== Common Type Constructors ========== */

/**
 * Create a List type with given element type.
 * @param arena The arena to allocate from.
 * @param elem_type The element type.
 * @return The List type.
 */
Type* type_list(Arena* arena, Type* elem_type) {
    // FERN_STYLE: allow(assertion-density) trivial type constructor wrapper
    TypeVec* args = TypeVec_new(arena);
    TypeVec_push(arena, args, elem_type);
    return type_con(arena, string_new(arena, "List"), args);
}

/**
 * Create a Map type with key and value types.
 * @param arena The arena to allocate from.
 * @param key_type The key type.
 * @param value_type The value type.
 * @return The Map type.
 */
Type* type_map(Arena* arena, Type* key_type, Type* value_type) {
    // FERN_STYLE: allow(assertion-density) trivial type constructor wrapper
    TypeVec* args = TypeVec_new(arena);
    TypeVec_push(arena, args, key_type);
    TypeVec_push(arena, args, value_type);
    return type_con(arena, string_new(arena, "Map"), args);
}

/**
 * Create an Option type with inner type.
 * @param arena The arena to allocate from.
 * @param inner_type The inner type.
 * @return The Option type.
 */
Type* type_option(Arena* arena, Type* inner_type) {
    // FERN_STYLE: allow(assertion-density) trivial type constructor wrapper
    TypeVec* args = TypeVec_new(arena);
    TypeVec_push(arena, args, inner_type);
    return type_con(arena, string_new(arena, "Option"), args);
}

/**
 * Create a Result type with Ok and Err types.
 * @param arena The arena to allocate from.
 * @param ok_type The Ok type.
 * @param err_type The Err type.
 * @return The Result type.
 */
Type* type_result(Arena* arena, Type* ok_type, Type* err_type) {
    // FERN_STYLE: allow(assertion-density) trivial type constructor wrapper
    TypeVec* args = TypeVec_new(arena);
    TypeVec_push(arena, args, ok_type);
    TypeVec_push(arena, args, err_type);
    return type_con(arena, string_new(arena, "Result"), args);
}

/* ========== Type Predicates ========== */

/**
 * Check if type is a primitive (Int, Float, String, Bool, Unit).
 * @param type The type to check.
 * @return True if primitive, false otherwise.
 */
bool type_is_primitive(Type* type) {
    // FERN_STYLE: allow(assertion-density) simple predicate check
    if (!type) return false;
    return type->kind == TYPE_INT ||
           type->kind == TYPE_FLOAT ||
           type->kind == TYPE_STRING ||
           type->kind == TYPE_BOOL ||
           type->kind == TYPE_UNIT;
}

/**
 * Check if type is numeric (Int or Float).
 * @param type The type to check.
 * @return True if numeric, false otherwise.
 */
bool type_is_numeric(Type* type) {
    // FERN_STYLE: allow(assertion-density) simple predicate check
    if (!type) return false;
    return type->kind == TYPE_INT || type->kind == TYPE_FLOAT;
}

/**
 * Check if type is comparable (not function or error).
 * @param type The type to check.
 * @return True if comparable, false otherwise.
 */
bool type_is_comparable(Type* type) {
    assert(type == NULL || type->kind >= TYPE_INT);
    if (!type) return false;
    assert(type->kind <= TYPE_ERROR);
    // All types except functions and errors are comparable
    return type->kind != TYPE_FN && type->kind != TYPE_ERROR;
}

/**
 * Check if type is a Result type constructor.
 * @param type The type to check.
 * @return True if Result type, false otherwise.
 */
bool type_is_result(Type* type) {
    assert(type == NULL || type->kind >= TYPE_INT);
    if (!type || type->kind != TYPE_CON) return false;
    assert(type->data.con.name != NULL);
    return strcmp(string_cstr(type->data.con.name), "Result") == 0;
}

/**
 * Check if type is an Option type constructor.
 * @param type The type to check.
 * @return True if Option type, false otherwise.
 */
bool type_is_option(Type* type) {
    assert(type == NULL || type->kind >= TYPE_INT);
    if (!type || type->kind != TYPE_CON) return false;
    assert(type->data.con.name != NULL);
    return strcmp(string_cstr(type->data.con.name), "Option") == 0;
}

/* ========== Type Comparison ========== */

/**
 * Check if two types are structurally equal.
 * @param a The first type.
 * @param b The second type.
 * @return True if equal, false otherwise.
 */
bool type_equals(Type* a, Type* b) {
    assert(a == NULL || a->kind >= TYPE_INT);
    assert(b == NULL || b->kind >= TYPE_INT);
    if (!a || !b) return a == b;
    if (a->kind != b->kind) return false;
    
    switch (a->kind) {
        case TYPE_INT:
        case TYPE_FLOAT:
        case TYPE_STRING:
        case TYPE_BOOL:
        case TYPE_UNIT:
        case TYPE_ERROR:
            return true;  // Same kind = equal for primitives
            
        case TYPE_VAR:
            // If bound, compare bound types
            if (a->data.var.bound && b->data.var.bound) {
                return type_equals(a->data.var.bound, b->data.var.bound);
            }
            // Otherwise compare by ID
            return a->data.var.id == b->data.var.id;
            
        case TYPE_CON:
            // Compare name and all type arguments
            if (!string_equal(a->data.con.name, b->data.con.name)) {
                return false;
            }
            if (!a->data.con.args && !b->data.con.args) return true;
            if (!a->data.con.args || !b->data.con.args) return false;
            if (a->data.con.args->len != b->data.con.args->len) return false;
            for (size_t i = 0; i < a->data.con.args->len; i++) {
                if (!type_equals(a->data.con.args->data[i], b->data.con.args->data[i])) {
                    return false;
                }
            }
            return true;
            
        case TYPE_FN:
            // Compare parameter count and types
            if (a->data.fn.params->len != b->data.fn.params->len) return false;
            for (size_t i = 0; i < a->data.fn.params->len; i++) {
                if (!type_equals(a->data.fn.params->data[i], b->data.fn.params->data[i])) {
                    return false;
                }
            }
            return type_equals(a->data.fn.result, b->data.fn.result);
            
        case TYPE_TUPLE:
            if (a->data.tuple.elements->len != b->data.tuple.elements->len) return false;
            for (size_t i = 0; i < a->data.tuple.elements->len; i++) {
                if (!type_equals(a->data.tuple.elements->data[i], b->data.tuple.elements->data[i])) {
                    return false;
                }
            }
            return true;
    }
    
    return false;
}

/**
 * Check if a value of type 'from' can be assigned to type 'to'.
 * @param from The source type.
 * @param to The target type.
 * @return True if assignable, false otherwise.
 */
bool type_assignable(Type* from, Type* to) {
    assert(from == NULL || from->kind >= TYPE_INT);
    assert(to == NULL || to->kind >= TYPE_INT);
    // For now, types must be equal to be assignable
    // Future: subtyping rules could go here
    return type_equals(from, to);
}

/* ========== Type Utilities ========== */

/**
 * Convert a type to its string representation.
 * @param arena The arena to allocate from.
 * @param type The type to convert.
 * @return The string representation.
 */
String* type_to_string(Arena* arena, Type* type) {
    // FERN_STYLE: allow(function-length) one case per type kind
    assert(arena != NULL);
    if (!type) return string_new(arena, "<null>");
    assert(type->kind >= TYPE_INT && type->kind <= TYPE_ERROR);
    
    switch (type->kind) {
        case TYPE_INT:
            return string_new(arena, "Int");
        case TYPE_FLOAT:
            return string_new(arena, "Float");
        case TYPE_STRING:
            return string_new(arena, "String");
        case TYPE_BOOL:
            return string_new(arena, "Bool");
        case TYPE_UNIT:
            return string_new(arena, "()");
        case TYPE_ERROR:
            return string_format(arena, "<error: %s>", 
                type->data.error_msg ? string_cstr(type->data.error_msg) : "unknown");
            
        case TYPE_VAR:
            if (type->data.var.bound) {
                return type_to_string(arena, type->data.var.bound);
            }
            return type->data.var.name;
            
        case TYPE_CON: {
            if (!type->data.con.args || type->data.con.args->len == 0) {
                return type->data.con.name;
            }
            
            // Build "Name(arg1, arg2, ...)"
            String* result = string_concat(arena, type->data.con.name, 
                                          string_new(arena, "("));
            for (size_t i = 0; i < type->data.con.args->len; i++) {
                if (i > 0) {
                    result = string_concat(arena, result, string_new(arena, ", "));
                }
                result = string_concat(arena, result, 
                    type_to_string(arena, type->data.con.args->data[i]));
            }
            result = string_concat(arena, result, string_new(arena, ")"));
            return result;
        }
            
        case TYPE_FN: {
            // Build "(param1, param2) -> result"
            String* result = string_new(arena, "(");
            for (size_t i = 0; i < type->data.fn.params->len; i++) {
                if (i > 0) {
                    result = string_concat(arena, result, string_new(arena, ", "));
                }
                result = string_concat(arena, result, 
                    type_to_string(arena, type->data.fn.params->data[i]));
            }
            result = string_concat(arena, result, string_new(arena, ") -> "));
            result = string_concat(arena, result, 
                type_to_string(arena, type->data.fn.result));
            return result;
        }
            
        case TYPE_TUPLE: {
            // Build "(elem1, elem2, ...)"
            String* result = string_new(arena, "(");
            for (size_t i = 0; i < type->data.tuple.elements->len; i++) {
                if (i > 0) {
                    result = string_concat(arena, result, string_new(arena, ", "));
                }
                result = string_concat(arena, result, 
                    type_to_string(arena, type->data.tuple.elements->data[i]));
            }
            result = string_concat(arena, result, string_new(arena, ")"));
            return result;
        }
    }
    
    return string_new(arena, "<unknown>");
}

/**
 * Create a deep copy of a type.
 * @param arena The arena to allocate from.
 * @param type The type to clone.
 * @return The cloned type.
 */
Type* type_clone(Arena* arena, Type* type) {
    assert(arena != NULL);
    if (!type) return NULL;
    assert(type->kind >= TYPE_INT && type->kind <= TYPE_ERROR);
    
    switch (type->kind) {
        case TYPE_INT:
            return type_int(arena);
        case TYPE_FLOAT:
            return type_float(arena);
        case TYPE_STRING:
            return type_string(arena);
        case TYPE_BOOL:
            return type_bool(arena);
        case TYPE_UNIT:
            return type_unit(arena);
        case TYPE_ERROR:
            return type_error(arena, type->data.error_msg);
            
        case TYPE_VAR: {
            Type* cloned = type_var(arena, type->data.var.name, type->data.var.id);
            if (type->data.var.bound) {
                cloned->data.var.bound = type_clone(arena, type->data.var.bound);
            }
            return cloned;
        }
            
        case TYPE_CON: {
            TypeVec* args = NULL;
            if (type->data.con.args) {
                args = TypeVec_new(arena);
                for (size_t i = 0; i < type->data.con.args->len; i++) {
                    TypeVec_push(arena, args, type_clone(arena, type->data.con.args->data[i]));
                }
            }
            return type_con(arena, type->data.con.name, args);
        }
            
        case TYPE_FN: {
            TypeVec* params = TypeVec_new(arena);
            for (size_t i = 0; i < type->data.fn.params->len; i++) {
                TypeVec_push(arena, params, type_clone(arena, type->data.fn.params->data[i]));
            }
            return type_fn(arena, params, type_clone(arena, type->data.fn.result));
        }
            
        case TYPE_TUPLE: {
            TypeVec* elements = TypeVec_new(arena);
            for (size_t i = 0; i < type->data.tuple.elements->len; i++) {
                TypeVec_push(arena, elements, type_clone(arena, type->data.tuple.elements->data[i]));
            }
            return type_tuple(arena, elements);
        }
    }
    
    return NULL;
}
