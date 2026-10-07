#include <unity.h>

#include <cstdint>

#include "fake_sensor.h"
#include "reading_buffer.h"
#include "sampler.h"

void setUp() {}
void tearDown() {}

void test_first_start_begins_the_sensor_then_starts_a_measurement() {
    FakeSensor sensor;
    core::ReadingBuffer buffer;
    core::Sampler sampler(sensor, buffer);

    sampler.start(1000);

    TEST_ASSERT_EQUAL_UINT32(1, sensor.beginCalls);
    TEST_ASSERT_EQUAL_UINT32(1, sensor.startCalls);
    TEST_ASSERT_EQUAL_UINT32(0, sampler.errorCount());
}

void test_healthy_sensor_is_begun_only_once() {
    FakeSensor sensor;
    core::ReadingBuffer buffer;
    core::Sampler sampler(sensor, buffer);

    for (uint32_t now = 1000; now <= 5000; now += 1000) {
        sampler.start(now);
        sampler.collect(now + 10);
    }

    TEST_ASSERT_EQUAL_UINT32(1, sensor.beginCalls);
    TEST_ASSERT_EQUAL_UINT32(5, sensor.startCalls);
    TEST_ASSERT_EQUAL_UINT32(5, buffer.size());
}

void test_collect_waits_for_the_sensors_conversion_time() {
    FakeSensor sensor;
    sensor.conversionTime = 25;
    core::ReadingBuffer buffer;
    core::Sampler sampler(sensor, buffer);
    sampler.start(1000);

    sampler.collect(1024);
    TEST_ASSERT_EQUAL_UINT32(0, sensor.readCalls);
    TEST_ASSERT_TRUE(buffer.empty());

    sampler.collect(1025);
    TEST_ASSERT_EQUAL_UINT32(1, sensor.readCalls);
    TEST_ASSERT_EQUAL_UINT32(1, buffer.size());
}

void test_reading_carries_the_start_time_and_the_sensors_measurement() {
    FakeSensor sensor;
    core::ReadingBuffer buffer;
    core::Sampler sampler(sensor, buffer);

    sampler.start(1000);
    sampler.collect(1013);

    const core::Reading& reading = buffer.front();
    TEST_ASSERT_EQUAL_UINT32(1000, reading.uptimeMs);
    TEST_ASSERT_EQUAL_INT32(2113, reading.measurement.centiCelsius);
    TEST_ASSERT_EQUAL_UINT32(25264070, reading.measurement.pressureQ24_8);
    TEST_ASSERT_EQUAL_UINT32(51156, reading.measurement.humidityQ22_10);
}

void test_collect_without_a_pending_measurement_does_nothing() {
    FakeSensor sensor;
    core::ReadingBuffer buffer;
    core::Sampler sampler(sensor, buffer);

    sampler.collect(5000);

    TEST_ASSERT_EQUAL_UINT32(0, sensor.readCalls);
    TEST_ASSERT_TRUE(buffer.empty());
}

void test_each_measurement_is_collected_once() {
    FakeSensor sensor;
    core::ReadingBuffer buffer;
    core::Sampler sampler(sensor, buffer);
    sampler.start(1000);

    sampler.collect(1010);
    sampler.collect(1015);
    sampler.collect(1020);

    TEST_ASSERT_EQUAL_UINT32(1, sensor.readCalls);
    TEST_ASSERT_EQUAL_UINT32(1, buffer.size());
}

void test_conversion_wait_survives_millisecond_counter_rollover() {
    FakeSensor sensor;
    core::ReadingBuffer buffer;
    core::Sampler sampler(sensor, buffer);
    sampler.start(0xFFFFFFFCu);

    sampler.collect(0x00000005u);
    TEST_ASSERT_TRUE(buffer.empty());

    sampler.collect(0x00000006u);
    TEST_ASSERT_EQUAL_UINT32(1, buffer.size());
}

void test_failed_begin_counts_an_error_and_is_retried_on_the_next_start() {
    FakeSensor sensor;
    sensor.beginSucceeds = false;
    core::ReadingBuffer buffer;
    core::Sampler sampler(sensor, buffer);

    sampler.start(1000);
    TEST_ASSERT_EQUAL_UINT32(1, sampler.errorCount());
    TEST_ASSERT_EQUAL_UINT32(0, sensor.startCalls);

    sensor.beginSucceeds = true;
    sampler.start(2000);
    sampler.collect(2010);

    TEST_ASSERT_EQUAL_UINT32(2, sensor.beginCalls);
    TEST_ASSERT_EQUAL_UINT32(1, sampler.errorCount());
    TEST_ASSERT_EQUAL_UINT32(1, buffer.size());
}

void test_failed_start_counts_an_error_and_forces_a_new_begin() {
    FakeSensor sensor;
    sensor.startSucceeds = false;
    core::ReadingBuffer buffer;
    core::Sampler sampler(sensor, buffer);

    sampler.start(1000);
    sampler.collect(1010);
    TEST_ASSERT_EQUAL_UINT32(1, sampler.errorCount());
    TEST_ASSERT_EQUAL_UINT32(0, sensor.readCalls);

    sensor.startSucceeds = true;
    sampler.start(2000);

    TEST_ASSERT_EQUAL_UINT32(2, sensor.beginCalls);
}

void test_failed_read_counts_an_error_pushes_nothing_and_forces_a_new_begin() {
    FakeSensor sensor;
    sensor.readSucceeds = false;
    core::ReadingBuffer buffer;
    core::Sampler sampler(sensor, buffer);

    sampler.start(1000);
    sampler.collect(1010);
    TEST_ASSERT_EQUAL_UINT32(1, sampler.errorCount());
    TEST_ASSERT_TRUE(buffer.empty());

    sensor.readSucceeds = true;
    sampler.start(2000);
    sampler.collect(2010);

    TEST_ASSERT_EQUAL_UINT32(2, sensor.beginCalls);
    TEST_ASSERT_EQUAL_UINT32(1, buffer.size());
}

void test_start_while_a_measurement_is_uncollected_counts_an_error() {
    FakeSensor sensor;
    core::ReadingBuffer buffer;
    core::Sampler sampler(sensor, buffer);

    sampler.start(1000);
    sampler.start(2000);
    sampler.collect(2010);

    TEST_ASSERT_EQUAL_UINT32(1, sampler.errorCount());
    TEST_ASSERT_EQUAL_UINT32(1, buffer.size());
    TEST_ASSERT_EQUAL_UINT32(2000, buffer.front().uptimeMs);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_first_start_begins_the_sensor_then_starts_a_measurement);
    RUN_TEST(test_healthy_sensor_is_begun_only_once);
    RUN_TEST(test_collect_waits_for_the_sensors_conversion_time);
    RUN_TEST(test_reading_carries_the_start_time_and_the_sensors_measurement);
    RUN_TEST(test_collect_without_a_pending_measurement_does_nothing);
    RUN_TEST(test_each_measurement_is_collected_once);
    RUN_TEST(test_conversion_wait_survives_millisecond_counter_rollover);
    RUN_TEST(test_failed_begin_counts_an_error_and_is_retried_on_the_next_start);
    RUN_TEST(test_failed_start_counts_an_error_and_forces_a_new_begin);
    RUN_TEST(test_failed_read_counts_an_error_pushes_nothing_and_forces_a_new_begin);
    RUN_TEST(test_start_while_a_measurement_is_uncollected_counts_an_error);
    return UNITY_END();
}
