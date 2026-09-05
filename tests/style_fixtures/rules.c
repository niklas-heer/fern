/** Healthy function with whitespace in calls.
 * @param n Positive value.
 * @return The supplied value.
 */
CustomType spaced_calls(int n) {
    assert (n > 0);
    assert	(n < 10);
    return n;
}

/** Missing parameter and return documentation. */
int missing_docs(int zebra, int alpha) {
    assert(zebra);
    assert(alpha);
    return zebra;
}

/** Allocation calls with token boundaries and whitespace.
 * @return A value.
 */
int allocation(void) {
    assert(1);
    assert(2);
    malloc (1);
    free (0);
    return 0;
}

/** Library cleanup is not direct allocation. */
void library_cleanup(void) {
    assert(1);
    assert(2);
    sdsfree(0);
    arena_free(0);
    custom_malloc(0);
}

/** A bounded loop and an unbounded predicate loop. */
void loops(void) {
    assert(1);
    assert(2);
    while (ready()) { break; }
    while (i < 10) { break; }
}

/** Mixed const and mutable parameters.
 * @param input Read-only input.
 * @param output Writable output.
 */
void raw_params(const char *input, char *output) {
    assert(input);
    assert(output);
}

/** Local character pointers are fine. */
void local_char(void) {
    assert(1);
    assert(2);
    char *buffer = 0;
}

/** Canonical exceptions apply to specific rules. */
void exceptions(char *buffer) {
    // FERN_STYLE: allow(assertion-density, no-raw-char, doc-params, bounded-loops)
    while (ready()) { break; }
}

/** Exception names outside allow do not suppress unrelated rules. */
void precise_exceptions(void) {
    // FERN_STYLE: allow(no-malloc) unrelated
    // assertion-density) must not count as an exception.
}

void missing_comment(void) {
    assert(1);
    assert(2);
}

/** Multiple assertions on one physical line count once. */
void assertion_lines(void) {
    assert(1); assert(2);
}

enum { RED } kind;
enum { BLUE } tag;
struct { int field; } type;
