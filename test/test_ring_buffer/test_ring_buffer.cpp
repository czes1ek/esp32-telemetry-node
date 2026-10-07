#include <unity.h>

#include <cstdint>

#include "ring_buffer.h"

namespace {

using Buffer = core::RingBuffer<int, 4>;

}

void setUp() {}
void tearDown() {}

void test_new_buffer_is_empty() {
    const Buffer buffer;

    TEST_ASSERT_TRUE(buffer.empty());
    TEST_ASSERT_FALSE(buffer.full());
    TEST_ASSERT_EQUAL_UINT32(0, buffer.size());
    TEST_ASSERT_EQUAL_UINT32(4, buffer.capacity());
    TEST_ASSERT_EQUAL_UINT32(0, buffer.dropped());
}

void test_items_come_out_in_insertion_order() {
    Buffer buffer;
    buffer.push(10);
    buffer.push(20);
    buffer.push(30);

    TEST_ASSERT_EQUAL_INT(10, buffer.front());
    buffer.pop();
    TEST_ASSERT_EQUAL_INT(20, buffer.front());
    buffer.pop();
    TEST_ASSERT_EQUAL_INT(30, buffer.front());
    buffer.pop();
    TEST_ASSERT_TRUE(buffer.empty());
}

void test_reports_full_at_capacity() {
    Buffer buffer;
    for (int i = 0; i < 4; ++i) buffer.push(i);

    TEST_ASSERT_TRUE(buffer.full());
    TEST_ASSERT_EQUAL_UINT32(4, buffer.size());
    TEST_ASSERT_EQUAL_UINT32(0, buffer.dropped());
}

void test_order_survives_wrapping_past_the_end_of_storage() {
    Buffer buffer;
    for (int i = 0; i < 100; ++i) {
        buffer.push(i);
        buffer.push(i + 1000);
        TEST_ASSERT_EQUAL_INT(i, buffer.front());
        buffer.pop();
        TEST_ASSERT_EQUAL_INT(i + 1000, buffer.front());
        buffer.pop();
    }

    TEST_ASSERT_TRUE(buffer.empty());
    TEST_ASSERT_EQUAL_UINT32(0, buffer.dropped());
}

void test_push_when_full_overwrites_oldest_and_counts_the_drop() {
    Buffer buffer;
    for (int i = 1; i <= 6; ++i) buffer.push(i);

    TEST_ASSERT_EQUAL_UINT32(4, buffer.size());
    TEST_ASSERT_EQUAL_UINT32(2, buffer.dropped());
    for (int expected = 3; expected <= 6; ++expected) {
        TEST_ASSERT_EQUAL_INT(expected, buffer.front());
        buffer.pop();
    }
    TEST_ASSERT_TRUE(buffer.empty());
}

void test_pop_on_empty_buffer_does_nothing() {
    Buffer buffer;
    buffer.pop();
    buffer.push(7);

    TEST_ASSERT_EQUAL_UINT32(1, buffer.size());
    TEST_ASSERT_EQUAL_INT(7, buffer.front());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_new_buffer_is_empty);
    RUN_TEST(test_items_come_out_in_insertion_order);
    RUN_TEST(test_reports_full_at_capacity);
    RUN_TEST(test_order_survives_wrapping_past_the_end_of_storage);
    RUN_TEST(test_push_when_full_overwrites_oldest_and_counts_the_drop);
    RUN_TEST(test_pop_on_empty_buffer_does_nothing);
    return UNITY_END();
}
