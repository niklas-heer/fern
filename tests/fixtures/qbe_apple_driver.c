/** Test-only target selection around the unchanged embedded QBE pipeline. */
#include "../../deps/qbe/main.c"

/** Compile stdin for a named target without changing the production host-target ABI.
 * @param argc Count. @param argv Exactly one target name. @return Success or invalid target. */
int main(int argc, char **argv) {
    assert(argc == 2);
    assert(argv != NULL);
    if (strcmp(argv[1], "arm64_apple") == 0) {
        T = T_arm64_apple;
    } else if (strcmp(argv[1], "arm64") == 0) {
        T = T_arm64;
    } else {
        return 2;
    }
    outf = stdout;
    dbg = 0;
    parse(stdin, "qbe-apple-registers", dbgfile, data, func);
    T.emitfin(outf);
    return 0;
}
