#ifndef FERN_FUZZ_GENERATOR_H
#define FERN_FUZZ_GENERATOR_H

#include <stddef.h>
#include <stdint.h>

/* Return number of on-disk seed corpus programs. */
size_t fuzz_seed_program_count(void);

/* Load a seed corpus program. Returns heap-allocated string or NULL. */
char* fuzz_load_seed_program(size_t index);

/* Generate a deterministic valid Fern program from seed + case index. */
char* fuzz_generate_program(uint64_t seed, uint32_t case_index);

#endif /* FERN_FUZZ_GENERATOR_H */
