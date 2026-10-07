#include <unity.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>

#include "config.h"
#include "fake_clock.h"
#include "fake_sensor.h"
#include "fake_transport.h"
#include "payload.h"
#include "publisher.h"
#include "reading_buffer.h"
#include "sampler.h"
#include "scheduler.h"

namespace {

constexpr char kDeviceId[] = "esp32-test";

core::Reading readingAt(uint32_t uptimeMs) {
    return core::Reading{uptimeMs, core::Measurement{2113, 25264070, 51156}};
}

std::string formatted(const core::Reading& reading, const core::Counters& counters = {0, 0, 0}) {
    char out[core::kPayloadCapacity];
    const size_t length = core::formatPayload(out, sizeof out, kDeviceId, reading, counters);
    return std::string(out, length);
}

std::string formattedTemperature(int32_t centiCelsius) {
    core::Reading reading = readingAt(0);
    reading.measurement.centiCelsius = centiCelsius;
    const std::string payload = formatted(reading);
    const std::string key = "\"temperature_c\":";
    const size_t begin = payload.find(key) + key.size();
    return payload.substr(begin, payload.find(',', begin) - begin);
}

bool contains(const std::string& text, const char* fragment) {
    return text.find(fragment) != std::string::npos;
}

struct Rig {
    FakeSensor sensor;
    FakeTransport transport;
    core::ReadingBuffer buffer;
    core::Sampler sampler{sensor, buffer};
    core::Publisher publisher{kDeviceId, buffer, sampler, transport};
};

struct RigWithUnformattableDeviceId {
    std::string deviceId = std::string(core::kPayloadCapacity, 'x');
    FakeSensor sensor;
    FakeTransport transport;
    core::ReadingBuffer buffer;
    core::Sampler sampler{sensor, buffer};
    core::Publisher publisher{deviceId.c_str(), buffer, sampler, transport};
};

struct ScheduledRig {
    ScheduledRig() {
        scheduler.add(startTask, core::kSampleIntervalMs);
        scheduler.add(collectTask, core::kCollectPollIntervalMs);
        scheduler.add(publishTask, core::kPublishIntervalMs);
    }

    void runFor(uint32_t durationMs) {
        for (uint32_t i = 0; i < durationMs; ++i) {
            ++clock.now;
            scheduler.tick();
            highWaterMark = std::max(highWaterMark, rig.buffer.size());
        }
    }

    Rig rig;
    FakeClock clock;
    core::Scheduler scheduler{clock};
    core::MethodTask<core::Sampler, &core::Sampler::start> startTask{rig.sampler};
    core::MethodTask<core::Sampler, &core::Sampler::collect> collectTask{rig.sampler};
    core::MethodTask<core::Publisher, &core::Publisher::publishPending> publishTask{rig.publisher};
    size_t highWaterMark = 0;
};

constexpr uint32_t kOneMinuteMs = 60000;
constexpr uint32_t kLastCollectionMs = 20;
constexpr size_t kReadingsPerPublish = core::kPublishIntervalMs / core::kSampleIntervalMs;

}

void setUp() {}
void tearDown() {}

void test_payload_is_json_with_fixed_point_values_as_decimals() {
    const std::string payload = formatted(readingAt(123456), core::Counters{3, 2, 1});

    TEST_ASSERT_EQUAL_STRING(
        "{\"device\":\"esp32-test\",\"uptime_ms\":123456,\"temperature_c\":21.13,"
        "\"station_pressure_pa\":98687.77,\"humidity_pct\":49.96,"
        "\"dropped\":3,\"sensor_errors\":2,\"format_errors\":1}",
        payload.c_str());
}

void test_payload_length_matches_the_text_written() {
    char out[core::kPayloadCapacity];
    const size_t length =
        core::formatPayload(out, sizeof out, kDeviceId, readingAt(1), core::Counters{0, 0, 0});

    TEST_ASSERT_EQUAL_UINT32(std::strlen(out), length);
}

void test_temperature_keeps_its_sign_and_leading_zeros() {
    TEST_ASSERT_EQUAL_STRING("0.00", formattedTemperature(0).c_str());
    TEST_ASSERT_EQUAL_STRING("0.05", formattedTemperature(5).c_str());
    TEST_ASSERT_EQUAL_STRING("-0.05", formattedTemperature(-5).c_str());
    TEST_ASSERT_EQUAL_STRING("-12.34", formattedTemperature(-1234).c_str());
    TEST_ASSERT_EQUAL_STRING("85.00", formattedTemperature(8500).c_str());
}

void test_payload_reports_zero_length_when_it_does_not_fit() {
    char out[32];

    TEST_ASSERT_EQUAL_UINT32(
        0, core::formatPayload(out, sizeof out, kDeviceId, readingAt(1), core::Counters{0, 0, 0}));
}

void test_largest_possible_payload_fits_the_payload_capacity() {
    const std::string longestId(core::kMaxDeviceIdLength, 'f');
    const uint32_t max = std::numeric_limits<uint32_t>::max();
    const core::Reading reading{
        max, core::Measurement{std::numeric_limits<int32_t>::min(), max, max}};
    char out[core::kPayloadCapacity];

    const size_t length = core::formatPayload(out, sizeof out, longestId.c_str(), reading,
                                              core::Counters{max, max, max});

    TEST_ASSERT_GREATER_THAN_UINT32(0, length);
    TEST_ASSERT_TRUE(contains(out, "\"temperature_c\":-21474836.48,"));
}

void test_one_run_drains_every_pending_reading_oldest_first() {
    Rig rig;
    for (uint32_t i = 1; i <= 5; ++i) rig.buffer.push(readingAt(i * 1000));

    rig.publisher.publishPending(0);

    TEST_ASSERT_TRUE(rig.buffer.empty());
    TEST_ASSERT_EQUAL_UINT32(5, rig.transport.payloads.size());
    TEST_ASSERT_TRUE(contains(rig.transport.payloads.front(), "\"uptime_ms\":1000,"));
    TEST_ASSERT_TRUE(contains(rig.transport.payloads.back(), "\"uptime_ms\":5000,"));
}

void test_one_run_sends_at_most_the_per_run_cap() {
    Rig rig;
    for (uint32_t i = 1; i <= 40; ++i) rig.buffer.push(readingAt(i));

    rig.publisher.publishPending(0);

    TEST_ASSERT_EQUAL_UINT32(core::Publisher::kMaxReadingsPerRun, rig.transport.payloads.size());
    TEST_ASSERT_EQUAL_UINT32(40 - core::Publisher::kMaxReadingsPerRun, rig.buffer.size());
    TEST_ASSERT_EQUAL_UINT32(core::Publisher::kMaxReadingsPerRun + 1,
                             rig.buffer.front().uptimeMs);
}

void test_nothing_is_sent_or_removed_while_the_transport_is_not_ready() {
    Rig rig;
    rig.transport.isReady = false;
    rig.buffer.push(readingAt(1));

    rig.publisher.publishPending(0);

    TEST_ASSERT_EQUAL_UINT32(0, rig.transport.publishCalls);
    TEST_ASSERT_EQUAL_UINT32(1, rig.buffer.size());
}

void test_failed_publish_stops_the_run_and_keeps_that_reading_for_retry() {
    Rig rig;
    for (uint32_t i = 1; i <= 5; ++i) rig.buffer.push(readingAt(i));
    rig.transport.failFromCall = 3;

    rig.publisher.publishPending(0);

    TEST_ASSERT_EQUAL_UINT32(3, rig.transport.publishCalls);
    TEST_ASSERT_EQUAL_UINT32(3, rig.buffer.size());
    TEST_ASSERT_EQUAL_UINT32(3, rig.buffer.front().uptimeMs);
}

void test_reading_that_cannot_be_formatted_is_dropped_and_counted() {
    RigWithUnformattableDeviceId rig;
    for (uint32_t i = 1; i <= 3; ++i) rig.buffer.push(readingAt(i));

    rig.publisher.publishPending(0);

    TEST_ASSERT_TRUE(rig.buffer.empty());
    TEST_ASSERT_EQUAL_UINT32(3, rig.publisher.formatErrorCount());
    TEST_ASSERT_EQUAL_UINT32(0, rig.transport.publishCalls);
}

void test_format_error_count_starts_at_zero_and_stays_there_when_payloads_fit() {
    Rig rig;
    rig.buffer.push(readingAt(1));

    rig.publisher.publishPending(0);

    TEST_ASSERT_EQUAL_UINT32(0, rig.publisher.formatErrorCount());
    TEST_ASSERT_TRUE(contains(rig.transport.payloads.front(), "\"format_errors\":0}"));
}

void test_empty_buffer_never_touches_the_transport() {
    Rig rig;

    rig.publisher.publishPending(0);

    TEST_ASSERT_EQUAL_UINT32(0, rig.transport.publishCalls);
}

void test_payload_carries_current_drop_and_sensor_error_counts() {
    Rig rig;
    for (size_t i = 0; i < core::kReadingBufferCapacity + 3; ++i) rig.buffer.push(readingAt(1));
    rig.sensor.beginSucceeds = false;
    rig.sampler.start(0);
    rig.sampler.start(0);

    rig.publisher.publishPending(0);

    TEST_ASSERT_TRUE(contains(rig.transport.payloads.front(), "\"dropped\":3,"));
    TEST_ASSERT_TRUE(contains(rig.transport.payloads.front(), "\"sensor_errors\":2,"));
}

void test_steady_state_with_production_intervals_drops_nothing() {
    ScheduledRig scheduled;

    scheduled.runFor(10 * kOneMinuteMs + kLastCollectionMs);

    const Rig& rig = scheduled.rig;
    const size_t produced = rig.sensor.readCalls;
    TEST_ASSERT_EQUAL_UINT32(10 * kOneMinuteMs / core::kSampleIntervalMs, produced);
    TEST_ASSERT_EQUAL_UINT32(0, rig.buffer.dropped());
    TEST_ASSERT_EQUAL_UINT32(0, rig.sampler.errorCount());
    TEST_ASSERT_EQUAL_UINT32(produced, rig.transport.payloads.size() + rig.buffer.size());
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(kReadingsPerPublish, scheduled.highWaterMark);
}

void test_backlog_from_a_short_outage_is_delivered_without_drops() {
    ScheduledRig scheduled;
    Rig& rig = scheduled.rig;

    rig.transport.isReady = false;
    scheduled.runFor(100000 + kLastCollectionMs);
    TEST_ASSERT_EQUAL_UINT32(50, rig.buffer.size());

    rig.transport.isReady = true;
    scheduled.runFor(kOneMinuteMs);

    TEST_ASSERT_EQUAL_UINT32(0, rig.buffer.dropped());
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(kReadingsPerPublish, rig.buffer.size());
    TEST_ASSERT_EQUAL_UINT32(rig.sensor.readCalls,
                             rig.transport.payloads.size() + rig.buffer.size());
    TEST_ASSERT_TRUE(contains(rig.transport.payloads.front(), "\"uptime_ms\":2000,"));
}

void test_outage_longer_than_the_buffer_drops_oldest_and_reports_the_count() {
    ScheduledRig scheduled;
    Rig& rig = scheduled.rig;

    rig.transport.isReady = false;
    scheduled.runFor(200000 - 1);
    TEST_ASSERT_EQUAL_UINT32(core::kReadingBufferCapacity, rig.buffer.size());
    const uint32_t droppedDuringOutage = rig.buffer.dropped();
    TEST_ASSERT_EQUAL_UINT32(99 - core::kReadingBufferCapacity, droppedDuringOutage);

    rig.transport.isReady = true;
    scheduled.runFor(1);

    const std::string expected = "\"dropped\":" + std::to_string(droppedDuringOutage) + ",";
    TEST_ASSERT_TRUE(contains(rig.transport.payloads.front(), expected.c_str()));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_payload_is_json_with_fixed_point_values_as_decimals);
    RUN_TEST(test_payload_length_matches_the_text_written);
    RUN_TEST(test_temperature_keeps_its_sign_and_leading_zeros);
    RUN_TEST(test_payload_reports_zero_length_when_it_does_not_fit);
    RUN_TEST(test_largest_possible_payload_fits_the_payload_capacity);
    RUN_TEST(test_one_run_drains_every_pending_reading_oldest_first);
    RUN_TEST(test_one_run_sends_at_most_the_per_run_cap);
    RUN_TEST(test_nothing_is_sent_or_removed_while_the_transport_is_not_ready);
    RUN_TEST(test_failed_publish_stops_the_run_and_keeps_that_reading_for_retry);
    RUN_TEST(test_reading_that_cannot_be_formatted_is_dropped_and_counted);
    RUN_TEST(test_format_error_count_starts_at_zero_and_stays_there_when_payloads_fit);
    RUN_TEST(test_empty_buffer_never_touches_the_transport);
    RUN_TEST(test_payload_carries_current_drop_and_sensor_error_counts);
    RUN_TEST(test_steady_state_with_production_intervals_drops_nothing);
    RUN_TEST(test_backlog_from_a_short_outage_is_delivered_without_drops);
    RUN_TEST(test_outage_longer_than_the_buffer_drops_oldest_and_reports_the_count);
    return UNITY_END();
}
