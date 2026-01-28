/* Result Type for Fern Compiler
 * 
 * Type-safe error handling using tagged unions.
 * Every operation that can fail returns a Result.
 * 
 * Usage:
 *   RESULT_TYPE(IntResult, int, Error);
 *   
 *   IntResult divide(int a, int b) {
 *       if (b == 0) {
 *           return IntResult_err((Error){.msg = "Division by zero"});
 *       }
 *       return IntResult_ok(a / b);
 *   }
 *   
 *   IntResult result = divide(10, 2);
 *   if (result.is_ok) {
 *       printf("Result: %d\n", result.ok);
 *   } else {
 *       printf("Error: %s\n", result.err.msg);
 *   }
 */

#ifndef FERN_RESULT_H
#define FERN_RESULT_H

#include <stdbool.h>

/* Define a Result type for a specific Ok and Err type.
 * 
 * Example:
 *   RESULT_TYPE(ParseResult, AstNode*, ParseError);
 * 
 * This creates:
 *   - Type: ParseResult
 *   - Constructor: ParseResult_ok(value)
 *   - Constructor: ParseResult_err(error)
 *   - Field: .is_ok (bool)
 *   - Field: .ok (AstNode*)
 *   - Field: .err (ParseError)
 */
#define RESULT_TYPE(Name, OkType, ErrType) \
    typedef struct Name { \
        bool is_ok; \
        union { \
            OkType ok; \
            ErrType err; \
        }; \
    } Name; \
    \
    static inline Name Name##_ok(OkType value) { \
        return (Name){.is_ok = true, .ok = value}; \
    } \
    \
    static inline Name Name##_err(ErrType error) { \
        return (Name){.is_ok = false, .err = error}; \
    }

/* Macro to check and unwrap Ok value, or return Err */
#define TRY(result) \
    do { \
        if (!(result).is_ok) { \
            return result; \
        } \
    } while (0)

/* Macro to unwrap Ok value or return custom Err */
#define TRY_OR(result, error_value) \
    do { \
        if (!(result).is_ok) { \
            return error_value; \
        } \
    } while (0)

/* Get Ok value from result (use only when is_ok is true) */
#define UNWRAP_OK(result) ((result).ok)

/* Get Err value from result (use only when is_ok is false) */
#define UNWRAP_ERR(result) ((result).err)

#endif /* FERN_RESULT_H */
