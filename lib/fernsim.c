#include "fernsim.h"

#include <assert.h>
#include <stddef.h>

#define FERNSIM_DEFAULT_SEED UINT64_C(0x9E3779B97F4A7C15)

/**
 * Normalize a seed so the PRNG state is never zero.
 * @param seed The incoming seed value.
 * @return A non-zero seed.
 */
static uint64_t normalize_seed(uint64_t seed) {
    assert(seed <= UINT64_MAX);
    assert(FERNSIM_DEFAULT_SEED != 0);

    if (seed == 0) {
        return FERNSIM_DEFAULT_SEED;
    }

    return seed;
}

/**
 * Advance xorshift64* and produce the next pseudo-random value.
 * @param state PRNG state pointer.
 * @return Next pseudo-random 64-bit value.
 */
static uint64_t xorshift64star(uint64_t* state) {
    uint64_t x = 0;

    assert(state != NULL);
    assert(*state != 0);

    x = *state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    *state = x;

    return x * UINT64_C(2685821657736338717);
}

/**
 * Create a new simulation context.
 * @param arena Arena used for allocations.
 * @param seed Deterministic seed value.
 * @return New simulation context or NULL on allocation failure.
 */
FernSim* fernsim_new(Arena* arena, uint64_t seed) {
    FernSim* sim = NULL;

    assert(arena != NULL);
    assert(seed <= UINT64_MAX);

    sim = arena_alloc(arena, sizeof(FernSim));
    if (!sim) {
        return NULL;
    }

    sim->queue = FernSimEventVec_new(arena);
    if (!sim->queue) {
        return NULL;
    }

    sim->arena = arena;
    sim->rng_state = normalize_seed(seed);
    sim->now_ms = 0;
    sim->next_sequence = 0;

    return sim;
}

/**
 * Generate the next deterministic random 64-bit value.
 * @param sim Simulation context.
 * @return Next pseudo-random value.
 */
uint64_t fernsim_next_u64(FernSim* sim) {
    assert(sim != NULL);
    assert(sim->rng_state != 0);

    return xorshift64star(&sim->rng_state);
}

/**
 * Generate a deterministic bounded value in [0, limit).
 * @param sim Simulation context.
 * @param limit Exclusive upper bound.
 * @return Bounded pseudo-random value.
 */
uint32_t fernsim_next_u32(FernSim* sim, uint32_t limit) {
    uint64_t value = 0;

    assert(sim != NULL);
    assert(limit > 0);

    value = fernsim_next_u64(sim);
    return (uint32_t)(value % (uint64_t)limit);
}

/**
 * Get current virtual time in milliseconds.
 * @param sim Simulation context.
 * @return Current virtual time.
 */
uint64_t fernsim_now_ms(const FernSim* sim) {
    assert(sim != NULL);
    assert(sim->now_ms <= UINT64_MAX);

    return sim->now_ms;
}

/**
 * Advance virtual time by a fixed delta.
 * @param sim Simulation context.
 * @param delta_ms Milliseconds to add.
 * @return Nothing.
 */
void fernsim_advance_ms(FernSim* sim, uint64_t delta_ms) {
    assert(sim != NULL);
    assert(sim->now_ms <= UINT64_MAX - delta_ms);

    sim->now_ms += delta_ms;
}

/**
 * Schedule an actor event at now + delay.
 * @param sim Simulation context.
 * @param actor_id Actor identifier to schedule.
 * @param delay_ms Relative delay in milliseconds.
 * @return true on success, false on overflow/allocation failure.
 */
bool fernsim_schedule_actor(FernSim* sim, FernSimActorId actor_id, uint64_t delay_ms) {
    FernSimEvent event = {0};

    assert(sim != NULL);
    assert(sim->queue != NULL);

    if (sim->now_ms > UINT64_MAX - delay_ms) {
        return false;
    }

    event.actor_id = actor_id;
    event.deliver_at_ms = sim->now_ms + delay_ms;
    event.sequence = sim->next_sequence++;

    return FernSimEventVec_push(sim->arena, sim->queue, event);
}

/**
 * Check whether queued events exist.
 * @param sim Simulation context.
 * @return true if queue is non-empty.
 */
bool fernsim_has_pending(const FernSim* sim) {
    assert(sim != NULL);
    assert(sim->queue != NULL);

    return sim->queue->len > 0;
}

/**
 * Select one due event with deterministic tie-breaking.
 * @param sim Simulation context.
 * @return Queue index of selected event.
 */
static size_t select_next_index(FernSim* sim) {
    size_t chosen = 0;
    uint64_t best_deadline = 0;
    uint32_t tie_count = 1;

    assert(sim != NULL);
    assert(sim->queue != NULL);
    assert(sim->queue->len > 0);

    best_deadline = sim->queue->data[0].deliver_at_ms;
    for (size_t i = 1; i < sim->queue->len; i++) {
        FernSimEvent* event = &sim->queue->data[i];

        if (event->deliver_at_ms < best_deadline) {
            best_deadline = event->deliver_at_ms;
            chosen = i;
            tie_count = 1;
        } else if (event->deliver_at_ms == best_deadline) {
            tie_count++;
            if (fernsim_next_u32(sim, tie_count) == 0) {
                chosen = i;
            }
        }
    }

    return chosen;
}

/**
 * Execute one scheduler step and remove the selected event.
 * @param sim Simulation context.
 * @param out_event Destination for selected event.
 * @return true when an event is produced, false if queue is empty.
 */
bool fernsim_step(FernSim* sim, FernSimEvent* out_event) {
    size_t chosen = 0;
    size_t last = 0;
    FernSimEvent event = {0};

    assert(sim != NULL);
    assert(sim->queue != NULL);
    assert(out_event != NULL);

    if (!fernsim_has_pending(sim)) {
        return false;
    }

    chosen = select_next_index(sim);
    last = sim->queue->len - 1;
    event = sim->queue->data[chosen];

    sim->queue->data[chosen] = sim->queue->data[last];
    sim->queue->len = last;

    if (event.deliver_at_ms > sim->now_ms) {
        sim->now_ms = event.deliver_at_ms;
    }

    *out_event = event;
    return true;
}
