/** Internal budget boundary tests; including the unit exposes no extra ABI. */
#include "../../runtime/fern_json.c"
#include "fern_gc.h"
#include <stdio.h>

static size_t checks;
#define CHECK(condition) do { checks++; if (!(condition)) { \
    fprintf(stderr, "JSON budget check failed at line%d: %s\n", __LINE__, #condition); \
    exit(1); } } while (0)

/** Logical allocation charging includes all growth, even GC-abandoned buffers. */
static void allocations(void) {
    JsonParser parser = {.allocated = JSON_ALLOC_MAX - 8};
    CHECK(json_allocate(&parser, 8) != NULL);
    CHECK(parser.allocated == JSON_ALLOC_MAX);
    CHECK(json_allocate(&parser, 0) == NULL);
    CHECK(parser.code == 4);
    CHECK(parser.allocated == JSON_ALLOC_MAX);
    json_fail(&parser, 1, 0);
    CHECK(parser.code == 4);
}

/** Work is charged before execution and subtraction never underflows. */
static void work(void) {
    JsonParser parser = {.work = 17};
    CHECK(json_work(&parser, 17));
    CHECK(parser.work == 0);
    CHECK(!json_work(&parser, 1));
    CHECK(parser.code == 4);
    CHECK(parser.work == 0);
    JsonParser overflow = {.work = 1};
    CHECK(!json_work(&overflow, SIZE_MAX));
    CHECK(overflow.code == 4);
}

/** Seal metadata rejects expanded encoded output before any publication. */
static void encoded(void) {
    JsonParser parser = {0};
    FernJsonValue child = {.kind = J_NUMBER, .encoded = JSON_OUTPUT_MAX - 2, .height = 1, .nodes = 1};
    FernJsonValue* children[] = {&child};
    FernJsonValue parent = {.kind = J_ARRAY, .length = 1, .children = children, .height = 1, .nodes = 1};
    CHECK(json_seal(&parser, &parent));
    CHECK(parent.encoded == JSON_OUTPUT_MAX);
    CHECK(parent.height == 2);
    CHECK(parent.nodes == 2);
    child.encoded++;
    CHECK(!json_seal(&parser, &parent));
    CHECK(parser.code == 4);
}

/** Limit successful node creation exactly, without leaking a partial value. */
static void nodes(void) {
    JsonParser parser = {.nodes = JSON_NODES_MAX - 1};
    CHECK(json_node(&parser, J_NULL) != NULL);
    CHECK(parser.nodes == JSON_NODES_MAX);
    CHECK(json_node(&parser, J_NULL) == NULL);
    CHECK(parser.nodes == JSON_NODES_MAX);
    CHECK(parser.code == 4);
}

/** Run internal primitive boundary tests using the actual runtime allocator. */
int fern_main(void) {
    fern_gc_init();
    allocations(); work(); encoded(); nodes();
    printf("JSON budgets: %zu checks passed\n", checks);
    return 0;
}
