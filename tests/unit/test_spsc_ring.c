// tests/unit/test_spsc_ring.c
//
// Unit tests for SPSC lock-free ring buffer.

#include "unity.h"
#include "monday/core/spsc_ring.h"
#include "monday/core/allocator.h"

void setUp(void) {
    monday_allocator_init(4*1024*1024, 8*1024*1024, 4*1024*1024, 8*1024*1024);
}

void tearDown(void) {}

void test_spsc_ring_init_and_capacity(void) {
    monday_spsc_ring_t ring;
    monday_error_t err = monday_spsc_ring_init(&ring, 1024, sizeof(uint32_t));
    TEST_ASSERT_EQUAL(MONDAY_OK, err.code);
    TEST_ASSERT_EQUAL(1024, monday_spsc_ring_capacity(&ring));
    TEST_ASSERT_EQUAL(0, monday_spsc_ring_count(&ring));
}

void test_spsc_ring_push_pop(void) {
    monday_spsc_ring_t ring;
    monday_spsc_ring_init(&ring, 64, sizeof(uint32_t));

    uint32_t val = 42;
    monday_error_t err = monday_spsc_ring_push(&ring, &val);
    TEST_ASSERT_EQUAL(MONDAY_OK, err.code);
    TEST_ASSERT_EQUAL(1, monday_spsc_ring_count(&ring));

    uint32_t out_val = 0;
    err = monday_spsc_ring_pop(&ring, &out_val);
    TEST_ASSERT_EQUAL(MONDAY_OK, err.code);
    TEST_ASSERT_EQUAL(42, out_val);
    TEST_ASSERT_EQUAL(0, monday_spsc_ring_count(&ring));
}

void test_spsc_ring_empty_and_full(void) {
    monday_spsc_ring_t ring;
    monday_spsc_ring_init(&ring, 4, sizeof(uint32_t));

    uint32_t dummy = 0;
    monday_error_t err = monday_spsc_ring_pop(&ring, &dummy);
    TEST_ASSERT_EQUAL(MONDAY_ERR_NOT_FOUND, err.code);

    for (uint32_t i = 0; i < 4; i++) {
        err = monday_spsc_ring_push(&ring, &i);
        TEST_ASSERT_EQUAL(MONDAY_OK, err.code);
    }

    uint32_t overflow = 999;
    err = monday_spsc_ring_push(&ring, &overflow);
    TEST_ASSERT_EQUAL(MONDAY_ERR_ARENA_FULL, err.code);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_spsc_ring_init_and_capacity);
    RUN_TEST(test_spsc_ring_push_pop);
    RUN_TEST(test_spsc_ring_empty_and_full);
    return UNITY_END();
}
