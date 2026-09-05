/** Pointer and array parameter documentation.
 * @param values The values. @param name The name.
 */
void documented_arrays(int **values, char name[10]) {
    assert(values);
    assert(name);
}

/** Return words need token boundaries.
 * @returning This is not a return tag.
 */
int misleading_return(void) {
    assert(1);
    assert(2);
    // @return inside a body is not API documentation.
    return 0;
}

/** Standard plural return tag.
 * @returns The returned value.
 */
int plural_return(void) {
    assert(1);
    assert(2);
    return 0;
}

/** All loop spacing forms are recognized. */
void spaced_loops(void) {
    assert(1);
    assert(2);
    for ( ; ; ) { break; }
}

/** Explicitly false loops are bounded. */
void false_loops(void) {
    assert(1);
    assert(2);
    while ( false ) { break; }
    while ( 0 ) { break; }
}
