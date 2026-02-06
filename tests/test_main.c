/* Test Runner - Main Entry Point */

#include "test.h"

/* Test suite declarations */
void run_arena_tests(void);
void run_string_tests(void);
void run_lexer_tests(void);
void run_parser_tests(void);
void run_ast_validate_tests(void);
void run_cli_parse_tests(void);
void run_cli_main_tests(void);
void run_type_tests(void);
void run_checker_tests(void);
void run_codegen_tests(void);
void run_repl_tests(void);

int main(void) {
    test_init();
    
    printf("================================================================================\n");
    printf("Fern Compiler Test Suite\n");
    printf("================================================================================\n");
    
    run_arena_tests();
    run_string_tests();
    run_lexer_tests();
    run_parser_tests();
    run_ast_validate_tests();
    run_cli_parse_tests();
    run_cli_main_tests();
    run_type_tests();
    run_checker_tests();
    run_codegen_tests();
    run_repl_tests();
    
    return TEST_FINISH();
}
