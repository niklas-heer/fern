#include "fuzz_generator.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FUZZ_EXPR_DEPTH_BASE 2U
#define FUZZ_EXPR_DEPTH_SPAN 2U
#define FUZZ_MODE_COUNT 6U

typedef struct {
    uint64_t state;
} FuzzRng;

typedef struct {
    char* data;
    size_t len;
    size_t cap;
} Buffer;

static const char* k_seed_paths[] = {
    "tests/fuzz/corpus/basic.fn",
    "tests/fuzz/corpus/call_chain.fn",
    "tests/fuzz/corpus/collections.fn",
    "tests/fuzz/corpus/operators.fn",
    "tests/fuzz/corpus/if_chain.fn",
    "tests/fuzz/corpus/match_with.fn",
    "tests/fuzz/corpus/typed_signature.fn",
};

static bool buffer_init(Buffer* buf, size_t initial_cap) {
    if (!buf || initial_cap == 0) {
        return false;
    }

    buf->data = (char*)malloc(initial_cap);
    if (!buf->data) {
        return false;
    }

    buf->len = 0;
    buf->cap = initial_cap;
    buf->data[0] = '\0';
    return true;
}

static void buffer_free(Buffer* buf) {
    if (!buf) {
        return;
    }

    free(buf->data);
    buf->data = NULL;
    buf->len = 0;
    buf->cap = 0;
}

static bool buffer_reserve(Buffer* buf, size_t extra_len) {
    size_t needed = 0;
    char* grown = NULL;

    if (!buf || !buf->data) {
        return false;
    }

    if (extra_len > (SIZE_MAX - buf->len - 1)) {
        return false;
    }

    needed = buf->len + extra_len + 1;
    if (needed <= buf->cap) {
        return true;
    }

    while (buf->cap < needed) {
        if (buf->cap > (SIZE_MAX / 2)) {
            return false;
        }
        buf->cap *= 2;
    }

    grown = (char*)realloc(buf->data, buf->cap);
    if (!grown) {
        return false;
    }

    buf->data = grown;
    return true;
}

static bool buffer_append(Buffer* buf, const char* text) {
    size_t text_len = 0;

    if (!buf || !text) {
        return false;
    }

    text_len = strlen(text);
    if (!buffer_reserve(buf, text_len)) {
        return false;
    }

    memcpy(buf->data + buf->len, text, text_len);
    buf->len += text_len;
    buf->data[buf->len] = '\0';
    return true;
}

static uint64_t fuzz_rng_next(FuzzRng* rng) {
    uint64_t x = 0;

    if (!rng) {
        return 0;
    }

    x = rng->state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    rng->state = x;
    return x * UINT64_C(2685821657736338717);
}

static void fuzz_rng_seed(FuzzRng* rng, uint64_t seed) {
    if (!rng) {
        return;
    }

    if (seed == 0) {
        seed = UINT64_C(0x9E3779B97F4A7C15);
    }
    rng->state = seed;
    (void)fuzz_rng_next(rng);
}

static uint32_t fuzz_rng_range(FuzzRng* rng, uint32_t limit) {
    if (!rng || limit == 0) {
        return 0;
    }

    return (uint32_t)(fuzz_rng_next(rng) % (uint64_t)limit);
}

static uint64_t derive_case_seed(uint64_t seed, uint32_t case_index) {
    uint64_t mixed = seed + UINT64_C(0x9E3779B97F4A7C15) * (uint64_t)(case_index + 1U);

    mixed ^= mixed >> 30;
    mixed *= UINT64_C(0xBF58476D1CE4E5B9);
    mixed ^= mixed >> 27;
    mixed *= UINT64_C(0x94D049BB133111EB);
    mixed ^= mixed >> 31;

    return mixed;
}

static bool append_expr(Buffer* buf, FuzzRng* rng, uint32_t depth);

static bool append_terminal(Buffer* buf, FuzzRng* rng) {
    static const char* k_identifiers[] = {
        "x", "y", "value", "n", "count"
    };
    static const char* k_strings[] = {
        "\"fern\"", "\"fuzz\"", "\"seed\"", "\"ok\""
    };
    uint32_t choice = fuzz_rng_range(rng, 4);
    char number[32] = {0};

    switch (choice) {
        case 0:
            snprintf(number, sizeof(number), "%u", fuzz_rng_range(rng, 1000));
            return buffer_append(buf, number);
        case 1:
            return buffer_append(buf, fuzz_rng_range(rng, 2) == 0 ? "true" : "false");
        case 2:
            return buffer_append(buf,
                k_identifiers[fuzz_rng_range(rng, (uint32_t)(sizeof(k_identifiers) / sizeof(k_identifiers[0])))]);
        case 3:
            return buffer_append(buf,
                k_strings[fuzz_rng_range(rng, (uint32_t)(sizeof(k_strings) / sizeof(k_strings[0])))]);
        default:
            return false;
    }
}

static bool append_unary_expr(Buffer* buf, FuzzRng* rng, uint32_t depth) {
    if (fuzz_rng_range(rng, 2) == 0) {
        return buffer_append(buf, "-(") && append_expr(buf, rng, depth - 1) && buffer_append(buf, ")");
    }

    return buffer_append(buf, "not (") && append_expr(buf, rng, depth - 1) && buffer_append(buf, ")");
}

static bool append_binary_expr(Buffer* buf, FuzzRng* rng, uint32_t depth) {
    static const char* k_ops[] = {
        "+", "-", "*", "/", "==", "!=", "<", ">", "and", "or"
    };
    const char* op = k_ops[fuzz_rng_range(rng, (uint32_t)(sizeof(k_ops) / sizeof(k_ops[0])))];

    return buffer_append(buf, "(") &&
        append_expr(buf, rng, depth - 1) &&
        buffer_append(buf, " ") &&
        buffer_append(buf, op) &&
        buffer_append(buf, " ") &&
        append_expr(buf, rng, depth - 1) &&
        buffer_append(buf, ")");
}

static bool append_call_expr(Buffer* buf, FuzzRng* rng, uint32_t depth) {
    static const char* k_functions[] = {
        "add", "compute", "mix", "f", "g"
    };
    uint32_t arg_count = fuzz_rng_range(rng, 3);

    if (!buffer_append(buf,
            k_functions[fuzz_rng_range(rng, (uint32_t)(sizeof(k_functions) / sizeof(k_functions[0])))])) {
        return false;
    }

    if (!buffer_append(buf, "(")) {
        return false;
    }

    for (uint32_t i = 0; i < arg_count; i++) {
        if (i > 0 && !buffer_append(buf, ", ")) {
            return false;
        }
        if (!append_expr(buf, rng, depth - 1)) {
            return false;
        }
    }

    return buffer_append(buf, ")");
}

static bool append_list_expr(Buffer* buf, FuzzRng* rng, uint32_t depth) {
    uint32_t count = fuzz_rng_range(rng, 3);

    if (!buffer_append(buf, "[")) {
        return false;
    }

    for (uint32_t i = 0; i < count; i++) {
        if (i > 0 && !buffer_append(buf, ", ")) {
            return false;
        }
        if (!append_expr(buf, rng, depth - 1)) {
            return false;
        }
    }

    return buffer_append(buf, "]");
}

static bool append_tuple_expr(Buffer* buf, FuzzRng* rng, uint32_t depth) {
    uint32_t count = 2U + fuzz_rng_range(rng, 2);

    if (!buffer_append(buf, "(")) {
        return false;
    }

    for (uint32_t i = 0; i < count; i++) {
        if (i > 0 && !buffer_append(buf, ", ")) {
            return false;
        }
        if (!append_expr(buf, rng, depth - 1)) {
            return false;
        }
    }

    return buffer_append(buf, ")");
}

static bool append_expr(Buffer* buf, FuzzRng* rng, uint32_t depth) {
    uint32_t choice = 0;

    if (!buf || !rng) {
        return false;
    }

    if (depth == 0) {
        return append_terminal(buf, rng);
    }

    choice = fuzz_rng_range(rng, 7);
    switch (choice) {
        case 0:
            return append_terminal(buf, rng);
        case 1:
            return append_unary_expr(buf, rng, depth);
        case 2:
            return append_binary_expr(buf, rng, depth);
        case 3:
            return append_call_expr(buf, rng, depth);
        case 4:
            return append_list_expr(buf, rng, depth);
        case 5:
            return append_tuple_expr(buf, rng, depth);
        case 6:
            return buffer_append(buf, "(") && append_expr(buf, rng, depth - 1) && buffer_append(buf, ")");
        default:
            return false;
    }
}

static bool append_program_expression_main(Buffer* buf, FuzzRng* rng, uint32_t depth) {
    return buffer_append(buf, "fn main() -> Int:\n\tlet x: Int = ") &&
        append_expr(buf, rng, depth) &&
        buffer_append(buf, "\n\t1\n");
}

static bool append_program_if(Buffer* buf, FuzzRng* rng, uint32_t depth) {
    char number[32] = {0};

    snprintf(number, sizeof(number), "%u", 1U + fuzz_rng_range(rng, 100));

    return buffer_append(buf, "fn choose(n: Int) -> Int:\n\tif n > 0: ") &&
        append_expr(buf, rng, depth) &&
        buffer_append(buf, "\n\telse: ") &&
        append_expr(buf, rng, depth) &&
        buffer_append(buf, "\n\nfn main() -> Int:\n\tchoose(") &&
        buffer_append(buf, number) &&
        buffer_append(buf, ")\n");
}

static bool append_program_match(Buffer* buf, FuzzRng* rng, uint32_t depth) {
    char number[32] = {0};

    snprintf(number, sizeof(number), "%u", fuzz_rng_range(rng, 5));

    return buffer_append(buf, "fn classify(n: Int) -> Int:\n\tmatch n: 0 -> ") &&
        append_expr(buf, rng, depth) &&
        buffer_append(buf, ", _ -> ") &&
        append_expr(buf, rng, depth) &&
        buffer_append(buf, "\n\nfn main() -> Int:\n\tclassify(") &&
        buffer_append(buf, number) &&
        buffer_append(buf, ")\n");
}

static bool append_program_with(Buffer* buf, FuzzRng* rng, uint32_t depth) {
    return buffer_append(buf, "fn combine(x: Int, y: Int):\n\twith a <- Ok(x), b <- Ok(y) do Ok(a + b) else Err(e) -> ") &&
        append_expr(buf, rng, depth) &&
        buffer_append(buf, "\n\nfn main():\n\tcombine(1, 2)\n");
}

static bool append_program_typed_signature(Buffer* buf, FuzzRng* rng, uint32_t depth) {
    return buffer_append(buf, "fn id(n: Int) -> Int:\n\tn\n\nfn main() -> Int:\n\tlet value: Int = id(") &&
        append_expr(buf, rng, depth) &&
        buffer_append(buf, ")\n\tvalue\n");
}

static bool append_program_layout_sensitive(Buffer* buf) {
    return buffer_append(buf,
        "fn layout_demo(n: Int) -> Int:\n"
        "\tif n > 10:\n"
        "\t\tmatch n:\n"
        "\t\t\t11 -> 11\n"
        "\t\t\t_ -> n\n"
        "\telse:\n"
        "\t\twith x <- Ok(n) do x else Err(e) -> 0\n\n"
        "fn main() -> Int:\n"
        "\tlayout_demo(11)\n");
}

static bool append_program_for_mode(Buffer* buf, FuzzRng* rng, uint32_t depth, uint32_t mode) {
    switch (mode) {
        case 0:
            return append_program_expression_main(buf, rng, depth);
        case 1:
            return append_program_if(buf, rng, depth);
        case 2:
            return append_program_match(buf, rng, depth);
        case 3:
            return append_program_with(buf, rng, depth);
        case 4:
            return append_program_typed_signature(buf, rng, depth);
        case 5:
            return append_program_layout_sensitive(buf);
        default:
            return false;
    }
}

static char* read_file_all(const char* path) {
    FILE* file = NULL;
    long file_size = 0;
    size_t read_size = 0;
    char* data = NULL;

    if (!path) {
        return NULL;
    }

    file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    file_size = ftell(file);
    if (file_size < 0) {
        fclose(file);
        return NULL;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    data = (char*)malloc((size_t)file_size + 1);
    if (!data) {
        fclose(file);
        return NULL;
    }

    read_size = fread(data, 1, (size_t)file_size, file);
    fclose(file);

    if (read_size != (size_t)file_size) {
        free(data);
        return NULL;
    }

    data[file_size] = '\0';
    return data;
}

size_t fuzz_seed_program_count(void) {
    return sizeof(k_seed_paths) / sizeof(k_seed_paths[0]);
}

char* fuzz_load_seed_program(size_t index) {
    if (index >= fuzz_seed_program_count()) {
        return NULL;
    }

    return read_file_all(k_seed_paths[index]);
}

char* fuzz_generate_program(uint64_t seed, uint32_t case_index) {
    FuzzRng rng = {0};
    Buffer buf = {0};
    uint32_t depth = FUZZ_EXPR_DEPTH_BASE;
    uint32_t mode = case_index % FUZZ_MODE_COUNT;

    fuzz_rng_seed(&rng, derive_case_seed(seed, case_index));
    depth += fuzz_rng_range(&rng, FUZZ_EXPR_DEPTH_SPAN);

    if (!buffer_init(&buf, 512)) {
        return NULL;
    }

    if (!append_program_for_mode(&buf, &rng, depth, mode)) {
        buffer_free(&buf);
        return NULL;
    }

    return buf.data;
}
