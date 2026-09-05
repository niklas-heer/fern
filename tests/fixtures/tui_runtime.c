/* Runtime boundary fixture driven by scripts/test_tui.py. */
#include "fern_runtime.h"
#include <stdio.h>
#include <string.h>

int fern_main(void) {
    const char *mode = fern_arg(1);
    if (strcmp(mode, "input") == 0) {
        printf("RESULT:%s\n", fern_prompt_input("name> "));
    } else if (strcmp(mode, "password") == 0) {
        printf("RESULT:%s\n", fern_prompt_password("secret> "));
    } else if (strcmp(mode, "cursor") == 0) {
        fern_term_move_to(2, 3);
        fern_term_up(1);
        fern_term_down(2);
        fern_term_left(3);
        fern_term_right(4);
        fern_term_up(0);
        fern_term_left(-1);
        fern_term_hide_cursor();
        fern_term_show_cursor();
        fern_term_save_cursor();
        fern_term_restore_cursor();
        fern_term_clear();
        puts("DONE");
    } else if (strcmp(mode, "tree") == 0) {
        FernTree *base = fern_tree_new("project");
        FernTree *src = fern_tree_add(fern_tree_new("src"), fern_tree_new("main.fn"));
        FernTree *tree = fern_tree_add(fern_tree_add(base, src), fern_tree_new("README.md"));
        puts(fern_tree_render(tree));
        puts(fern_tree_render(base));
        puts(fern_tree_render(src));
        puts(fern_tree_render(fern_tree_add(fern_tree_new(""), fern_tree_new("a\nb"))));
    } else if (strcmp(mode, "log") == 0) {
        puts(fern_log_debug("details"));
        puts(fern_log_info("ready"));
        puts(fern_log_warn("careful\nsecond line"));
        puts(fern_log_error("bad\033[2J\rspoof"));
    } else {
        return 3;
    }
    return 0;
}
