/* Fern Type System
 *
 * Represents types for the Fern type checker.
 * Uses arena allocation for memory safety.
 */

#ifndef FERN_TYPE_H
#define FERN_TYPE_H

#include "arena.h"
#include "fern_string.h"
#include "vec.h"
#include <stdbool.h>

/* Forward declarations */
typedef struct Type Type;

/* Vector of types */
VEC_TYPE(TypeVec, Type*)

/* Type kinds */
typedef enum {
    /* Primitive types */
    TYPE_INT,           // Int
    TYPE_FLOAT,         // Float
    TYPE_STRING,        // String
    TYPE_BOOL,          // Bool
    TYPE_UNIT,          // () - unit type
    
    /* Type variable (for generics) */
    TYPE_VAR,           // a, b, etc.
    
    /* Constructed types */
    TYPE_CON,           // Named type with optional args: List(Int), Result(T, E)
    
    /* Function type */
    TYPE_FN,            // (Int, String) -> Bool
    
    /* Tuple type */
    TYPE_TUPLE,         // (Int, String, Bool)
    
    /* Error type (for type errors) */
    TYPE_ERROR,         // Represents a type error
} TypeKind;

/* Type variable */
typedef struct {
    String* name;       // Variable name: "a", "b", etc.
    int id;             // Unique ID for unification
    Type* bound;        // NULL if unbound, or the type it's bound to
} TypeVar;

/* Type constructor (named type with optional type arguments) */
typedef struct {
    String* name;       // Type name: "List", "Result", "User"
    TypeVec* args;      // Type arguments: [Int] for List(Int)
} TypeCon;

/* Function type */
typedef struct {
    TypeVec* params;    // Parameter types
    Type* result;       // Return type
} TypeFn;

/* Tuple type */
typedef struct {
    TypeVec* elements;  // Element types
} TypeTuple;

/* Type structure */
struct Type {
    TypeKind kind;
    union {
        TypeVar var;        // TYPE_VAR
        TypeCon con;        // TYPE_CON
        TypeFn fn;          // TYPE_FN
        TypeTuple tuple;    // TYPE_TUPLE
        String* error_msg;  // TYPE_ERROR
    } data;
};

/* ========== Type Creation ========== */

/* Primitive types (singletons) */
Type* type_int(Arena* arena);
Type* type_float(Arena* arena);
Type* type_string(Arena* arena);
Type* type_bool(Arena* arena);
Type* type_unit(Arena* arena);

/* Type variable */
Type* type_var(Arena* arena, String* name, int id);

/* Type constructor */
Type* type_con(Arena* arena, String* name, TypeVec* args);

/* Function type */
Type* type_fn(Arena* arena, TypeVec* params, Type* result);

/* Tuple type */
Type* type_tuple(Arena* arena, TypeVec* elements);

/* Error type */
Type* type_error(Arena* arena, String* message);

/* ========== Common Type Constructors ========== */

/* List(a) */
Type* type_list(Arena* arena, Type* elem_type);

/* Map(k, v) */
Type* type_map(Arena* arena, Type* key_type, Type* value_type);

/* Option(a) */
Type* type_option(Arena* arena, Type* inner_type);

/* Result(ok, err) */
Type* type_result(Arena* arena, Type* ok_type, Type* err_type);

/* ========== Type Predicates ========== */

bool type_is_primitive(Type* type);
bool type_is_numeric(Type* type);
bool type_is_comparable(Type* type);
bool type_is_result(Type* type);
bool type_is_option(Type* type);

/* ========== Type Comparison ========== */

/* Check if two types are equal (structural equality) */
bool type_equals(Type* a, Type* b);

/* Check if type a is assignable to type b */
bool type_assignable(Type* from, Type* to);

/* ========== Type Utilities ========== */

/* Get a human-readable string representation of a type */
String* type_to_string(Arena* arena, Type* type);

/* Deep copy a type */
Type* type_clone(Arena* arena, Type* type);

/* Fresh type variable counter */
int type_fresh_var_id(void);

#endif /* FERN_TYPE_H */
