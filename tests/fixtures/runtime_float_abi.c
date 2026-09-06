#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <gc.h>
extern void fern_print_float(double);
extern void fern_println_float(double);
extern char *fern_float_to_str(double);
int fern_main(void) {
    GC_INIT();
    assert(strcmp(fern_float_to_str(-0.0), "-0") == 0);
    assert(strcmp(fern_float_to_str(1.25), "1.25") == 0);
    assert(strcmp(fern_float_to_str(INFINITY), "inf") == 0);
    assert(strcmp(fern_float_to_str(-INFINITY), "-inf") == 0);
    assert(strstr(fern_float_to_str(NAN), "nan") != NULL);
    fern_print_float(1.25);
    fern_println_float(-0.0);
    return 0;
}
