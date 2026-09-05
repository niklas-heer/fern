/* Deterministic regression scenarios against the real actor runtime C ABI. */
#include "fern_runtime.h"
#include "fernsim.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REQUIRE(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: %s\n", __func__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

static int time_zero_budget(void) {
    REQUIRE(fern_actor_clock_set(0) == 0);
    int64_t supervisor = fern_actor_spawn("supervisor");
    int64_t worker = fern_actor_spawn("worker");
    REQUIRE(fern_result_is_ok(fern_actor_supervise(supervisor, worker, 1, 5)));
    int64_t first = fern_actor_exit(worker, "failure");
    REQUIRE(fern_result_is_ok(first));
    int64_t next = fern_result_unwrap(first);
    REQUIRE(next > worker);
    REQUIRE(!fern_result_is_ok(fern_actor_exit(next, "failure")));
    REQUIRE(fern_actor_mailbox_len(next) == -1);
    REQUIRE(fern_actor_clock_advance(5) == 0);
    int64_t manual = fern_actor_restart(next);
    REQUIRE(fern_result_is_ok(manual));
    REQUIRE(fern_result_is_ok(fern_actor_exit(fern_result_unwrap(manual), "failure")));
    return 0;
}

static int single_replacement(void) {
    int64_t worker = fern_actor_spawn("worker");
    REQUIRE(fern_result_is_ok(fern_actor_exit(worker, "normal")));
    int64_t first = fern_actor_restart(worker);
    REQUIRE(fern_result_is_ok(first));
    REQUIRE(!fern_result_is_ok(fern_actor_restart(worker)));
    int64_t next = fern_result_unwrap(first);
    REQUIRE(fern_result_is_ok(fern_actor_exit(next, "normal")));
    REQUIRE(!fern_result_is_ok(fern_actor_restart(worker)));
    REQUIRE(fern_result_is_ok(fern_actor_restart(next)));
    return 0;
}

static int supervision_forest(void) {
    int64_t root = fern_actor_spawn("root");
    int64_t middle = fern_actor_spawn("middle");
    int64_t leaf = fern_actor_spawn("leaf");
    int64_t other = fern_actor_spawn("other");
    REQUIRE(!fern_result_is_ok(fern_actor_supervise(root, root, 2, 60)));
    REQUIRE(fern_result_is_ok(fern_actor_supervise(root, middle, 2, 60)));
    REQUIRE(fern_result_is_ok(fern_actor_supervise(middle, leaf, 2, 60)));
    REQUIRE(!fern_result_is_ok(fern_actor_supervise(leaf, root, 2, 60)));
    REQUIRE(!fern_result_is_ok(fern_actor_supervise(other, leaf, 2, 60)));
    REQUIRE(fern_result_is_ok(fern_actor_exit(leaf, "failure")));
    REQUIRE(fern_actor_mailbox_len(other) == 0);
    REQUIRE(fern_actor_mailbox_len(middle) == 2);
    return 0;
}

static int invalid_pid(void) {
    REQUIRE(fern_actor_mailbox_len(INT64_MAX) == -1);
    REQUIRE(!fern_result_is_ok(fern_actor_send(INT64_MAX, "hello")));
    REQUIRE(!fern_result_is_ok(fern_actor_receive(INT64_MAX)));
    REQUIRE(!fern_result_is_ok(fern_actor_restart(INT64_MAX)));
    REQUIRE(fern_actor_set_current(INT64_MAX) == -1);
    return 0;
}

static int terminated_sibling(void) {
    int64_t supervisor = fern_actor_spawn("supervisor");
    int64_t a = fern_actor_spawn("a");
    int64_t b = fern_actor_spawn("b");
    REQUIRE(fern_result_is_ok(fern_actor_supervise_one_for_all(supervisor, a, 10, 60)));
    REQUIRE(fern_result_is_ok(fern_actor_supervise_one_for_all(supervisor, b, 10, 60)));
    REQUIRE(fern_result_is_ok(fern_actor_exit(b, "normal")));
    REQUIRE(fern_result_is_ok(fern_actor_receive(supervisor)));
    REQUIRE(fern_result_is_ok(fern_actor_exit(a, "failure")));
    REQUIRE(fern_actor_mailbox_len(supervisor) == 2);
    REQUIRE(fern_result_is_ok(fern_actor_restart(b)));
    return 0;
}

/* Apply seeded crashes to real strategy groups, checking every replacement and mailbox. */
static int simulation_strategy(uint64_t seed, int strategy) {
    Arena* arena = arena_create(8192);
    REQUIRE(arena != NULL);
    FernSim* sim = fernsim_new(arena, seed);
    REQUIRE(sim != NULL);
    int64_t supervisor = fern_actor_spawn("supervisor");
    int64_t children[3];
    for (int i = 0; i < 3; i++) {
        children[i] = fern_actor_spawn("worker");
        int64_t status = strategy == 1 ? fern_actor_supervise(supervisor, children[i], 500, 60) :
            strategy == 2 ? fern_actor_supervise_one_for_all(supervisor, children[i], 500, 60) :
            fern_actor_supervise_rest_for_one(supervisor, children[i], 500, 60);
        REQUIRE(fern_result_is_ok(status));
    }
    for (int step = 0; step < 64; step++) {
        REQUIRE(fernsim_schedule_actor(sim, fernsim_next_u32(sim, 3),
            fernsim_next_u32(sim, 4)));
        FernSimEvent event = {0};
        REQUIRE(fernsim_step(sim, &event));
        REQUIRE(fern_actor_clock_set((int64_t)(fernsim_now_ms(sim) / 1000)) == 0);
        int crashed = (int)event.actor_id;
        int64_t old[3];
        memcpy(old, children, sizeof(old));
        for (int i = 0; i < 3; i++) {
            REQUIRE(fern_result_is_ok(fern_actor_send(children[i], "pending")));
        }
        int64_t result = fern_actor_exit(children[crashed], "injected");
        REQUIRE(fern_result_is_ok(result));
        int expected = strategy == 1 ? 1 : strategy == 2 ? 3 : 3 - crashed;
        int replacements = 0;
        int64_t signals = fern_actor_mailbox_len(supervisor);
        REQUIRE(signals == expected * 2);
        for (int64_t i = 0; i < signals; i++) {
            int64_t message = fern_actor_receive(supervisor);
            REQUIRE(fern_result_is_ok(message));
            const char* text = (const char*)(intptr_t)fern_result_unwrap(message);
            int64_t previous = 0, next = 0;
            if (sscanf(text, "RESTART(%" SCNd64 ",%" SCNd64 ")", &previous, &next) == 2) {
                int found = 0;
                for (int j = 0; j < 3; j++) {
                    if (old[j] == previous) {
                        children[j] = next;
                        found++;
                    }
                }
                REQUIRE(found == 1);
                REQUIRE(fern_actor_mailbox_len(previous) == -1);
                REQUIRE(!fern_result_is_ok(fern_actor_send(previous, "stale")));
                REQUIRE(!fern_result_is_ok(fern_actor_restart(previous)));
                REQUIRE(fern_actor_mailbox_len(next) == 0);
                replacements++;
            }
        }
        REQUIRE(replacements == expected);
        REQUIRE(children[crashed] == fern_result_unwrap(result));
        for (int i = 0; i < 3; i++) {
            int changed = strategy == 1 ? i == crashed : strategy == 2 || i >= crashed;
            REQUIRE((children[i] != old[i]) == changed);
            if (!changed) REQUIRE(fern_result_is_ok(fern_actor_receive(children[i])));
        }
        REQUIRE(fern_actor_scheduler_next() == 0);
    }
    arena_destroy(arena);
    return 0;
}

int fern_main(void) {
    const char* scenario = getenv("FERN_ACTOR_SCENARIO");
    if (scenario == NULL) return 2;
    if (strcmp(scenario, "time-zero") == 0) return time_zero_budget();
    if (strcmp(scenario, "single-replacement") == 0) return single_replacement();
    if (strcmp(scenario, "forest") == 0) return supervision_forest();
    if (strcmp(scenario, "invalid-pid") == 0) return invalid_pid();
    if (strcmp(scenario, "terminated-sibling") == 0) return terminated_sibling();
    if (strcmp(scenario, "simulation") == 0) {
        for (uint64_t seed = 1; seed <= 8; seed++) {
            for (int strategy = 1; strategy <= 3; strategy++) {
                if (simulation_strategy(seed, strategy) != 0) {
                    fprintf(stderr, "seed=%" PRIu64 " strategy=%d\n", seed, strategy);
                    return 1;
                }
            }
        }
        return 0;
    }
    return 2;
}
