/* FernSim deterministic simulation scaffolding.
 *
 * Provides a seed-driven PRNG, virtual time, and a deterministic scheduler
 * queue for actor-style simulation tests.
 */

#ifndef FERN_FERNSIM_H
#define FERN_FERNSIM_H

#include "arena.h"
#include "vec.h"

#include <stdbool.h>
#include <stdint.h>

typedef uint32_t FernSimActorId;

typedef struct {
    FernSimActorId actor_id;
    uint64_t deliver_at_ms;
    uint64_t sequence;
} FernSimEvent;

VEC_TYPE(FernSimEventVec, FernSimEvent)

typedef struct FernSim {
    Arena* arena;
    uint64_t rng_state;
    uint64_t now_ms;
    uint64_t next_sequence;
    FernSimEventVec* queue;
} FernSim;

/* Create a new simulation context.
 *
 * @param arena Arena used for all FernSim allocations.
 * @param seed Deterministic seed; zero is mapped to a fixed non-zero seed.
 * @return New FernSim context, or NULL on allocation failure.
 */
FernSim* fernsim_new(Arena* arena, uint64_t seed);

/* Generate the next deterministic random 64-bit value.
 *
 * @param sim Simulation context.
 * @return Next pseudo-random value.
 */
uint64_t fernsim_next_u64(FernSim* sim);

/* Generate a bounded deterministic random value in [0, limit).
 *
 * @param sim Simulation context.
 * @param limit Exclusive upper bound; must be > 0.
 * @return Bounded pseudo-random value.
 */
uint32_t fernsim_next_u32(FernSim* sim, uint32_t limit);

/* Get current virtual time in milliseconds.
 *
 * @param sim Simulation context.
 * @return Current virtual time.
 */
uint64_t fernsim_now_ms(const FernSim* sim);

/* Advance virtual time by a fixed delta.
 *
 * @param sim Simulation context.
 * @param delta_ms Milliseconds to add.
 * @return Nothing.
 */
void fernsim_advance_ms(FernSim* sim, uint64_t delta_ms);

/* Schedule an actor event at now + delay.
 *
 * @param sim Simulation context.
 * @param actor_id Actor identifier to schedule.
 * @param delay_ms Delay from current virtual time.
 * @return true on success, false on overflow/allocation failure.
 */
bool fernsim_schedule_actor(FernSim* sim, FernSimActorId actor_id, uint64_t delay_ms);

/* Check whether queued events exist.
 *
 * @param sim Simulation context.
 * @return true when there are pending events.
 */
bool fernsim_has_pending(const FernSim* sim);

/* Execute one scheduler step.
 *
 * Picks the earliest deadline. Ties are broken using the deterministic PRNG.
 * The selected event is removed from the queue and returned.
 *
 * @param sim Simulation context.
 * @param out_event Destination for selected event.
 * @return true when an event was produced, false when queue is empty.
 */
bool fernsim_step(FernSim* sim, FernSimEvent* out_event);

#endif /* FERN_FERNSIM_H */
