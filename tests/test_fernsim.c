/* FernSim deterministic simulation scaffolding tests */

#include "test.h"
#include "arena.h"
#include "fernsim.h"

void test_fernsim_rng_is_deterministic(void) {
    Arena* arena = arena_create(8192);
    FernSim* sim_a = fernsim_new(arena, 0xDEADBEEF);
    FernSim* sim_b = fernsim_new(arena, 0xDEADBEEF);

    ASSERT_NOT_NULL(sim_a);
    ASSERT_NOT_NULL(sim_b);

    for (int i = 0; i < 16; i++) {
        uint64_t a = fernsim_next_u64(sim_a);
        uint64_t b = fernsim_next_u64(sim_b);
        ASSERT_EQ(a, b);
    }

    arena_destroy(arena);
}

void test_fernsim_rng_changes_with_seed(void) {
    Arena* arena = arena_create(8192);
    FernSim* sim_a = fernsim_new(arena, 0xABCDEF01);
    FernSim* sim_b = fernsim_new(arena, 0xABCDEF02);

    ASSERT_NOT_NULL(sim_a);
    ASSERT_NOT_NULL(sim_b);

    uint64_t a = fernsim_next_u64(sim_a);
    uint64_t b = fernsim_next_u64(sim_b);
    ASSERT_NE(a, b);

    arena_destroy(arena);
}

void test_fernsim_clock_is_monotonic(void) {
    Arena* arena = arena_create(4096);
    FernSim* sim = fernsim_new(arena, 1234);

    ASSERT_NOT_NULL(sim);
    ASSERT_EQ(fernsim_now_ms(sim), (uint64_t)0);

    fernsim_advance_ms(sim, 5);
    ASSERT_EQ(fernsim_now_ms(sim), (uint64_t)5);

    fernsim_advance_ms(sim, 17);
    ASSERT_EQ(fernsim_now_ms(sim), (uint64_t)22);

    arena_destroy(arena);
}

void test_fernsim_scheduler_orders_by_deadline(void) {
    Arena* arena = arena_create(8192);
    FernSim* sim = fernsim_new(arena, 777);
    FernSimEvent event = {0};

    ASSERT_NOT_NULL(sim);
    ASSERT_TRUE(fernsim_schedule_actor(sim, 10, 12));
    ASSERT_TRUE(fernsim_schedule_actor(sim, 20, 3));
    ASSERT_TRUE(fernsim_schedule_actor(sim, 30, 7));

    ASSERT_TRUE(fernsim_step(sim, &event));
    ASSERT_EQ(event.actor_id, (uint32_t)20);
    ASSERT_EQ(event.deliver_at_ms, (uint64_t)3);

    ASSERT_TRUE(fernsim_step(sim, &event));
    ASSERT_EQ(event.actor_id, (uint32_t)30);
    ASSERT_EQ(event.deliver_at_ms, (uint64_t)7);

    ASSERT_TRUE(fernsim_step(sim, &event));
    ASSERT_EQ(event.actor_id, (uint32_t)10);
    ASSERT_EQ(event.deliver_at_ms, (uint64_t)12);

    ASSERT_FALSE(fernsim_has_pending(sim));
    arena_destroy(arena);
}

void test_fernsim_tie_break_is_seeded_and_reproducible(void) {
    Arena* arena = arena_create(16384);
    FernSim* sim_a = fernsim_new(arena, 0x12345678);
    FernSim* sim_b = fernsim_new(arena, 0x12345678);
    FernSimEvent a[3] = {0};
    FernSimEvent b[3] = {0};

    ASSERT_NOT_NULL(sim_a);
    ASSERT_NOT_NULL(sim_b);

    for (uint32_t id = 1; id <= 3; id++) {
        ASSERT_TRUE(fernsim_schedule_actor(sim_a, id, 0));
        ASSERT_TRUE(fernsim_schedule_actor(sim_b, id, 0));
    }

    for (int i = 0; i < 3; i++) {
        ASSERT_TRUE(fernsim_step(sim_a, &a[i]));
        ASSERT_TRUE(fernsim_step(sim_b, &b[i]));
        ASSERT_EQ(a[i].actor_id, b[i].actor_id);
        ASSERT_EQ(a[i].deliver_at_ms, b[i].deliver_at_ms);
    }

    arena_destroy(arena);
}

void run_fernsim_tests(void) {
    printf("\n=== FernSim Tests ===\n");
    TEST_RUN(test_fernsim_rng_is_deterministic);
    TEST_RUN(test_fernsim_rng_changes_with_seed);
    TEST_RUN(test_fernsim_clock_is_monotonic);
    TEST_RUN(test_fernsim_scheduler_orders_by_deadline);
    TEST_RUN(test_fernsim_tie_break_is_seeded_and_reproducible);
}
