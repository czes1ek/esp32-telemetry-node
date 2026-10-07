#include <unity.h>

#include <cstdint>

#include "bme280_driver.h"
#include "fake_register_bus.h"

namespace {

constexpr bme280::CalibrationBlockA kCapturedBlockA{
    0x43, 0x6F, 0x6A, 0x68, 0x32, 0x00, 0xF2, 0x90, 0xAC, 0xD6, 0xD0, 0x0B, 0xB8,
    0x25, 0x5C, 0xFF, 0xF9, 0xFF, 0xB4, 0x2D, 0xE8, 0xD1, 0x88, 0x13, 0x00, 0x4B};
constexpr bme280::CalibrationBlockB kCapturedBlockB{0x6D, 0x01, 0x00, 0x13, 0x27, 0x03, 0x1E};
constexpr bme280::SampleBlock kCapturedSampleBlock{0x4A, 0x35, 0x00, 0x7F,
                                                   0x73, 0x00, 0x70, 0xEE};
constexpr bme280::SampleBlock kResetSampleBlock{0x80, 0x00, 0x00, 0x80, 0x00, 0x00, 0x80, 0x00};
constexpr bme280::SampleBlock kHumiditySkippedSampleBlock{0x4A, 0x35, 0x00, 0x7F,
                                                          0x73, 0x00, 0x80, 0x00};

constexpr uint8_t kBmp280ChipId = 0x58;

FakeRegisterBus busWithCapturedSensor() {
    FakeRegisterBus bus;
    bus.registers[bme280::kChipIdRegister] = bme280::kChipId;
    bus.load(bme280::kCalibrationBlockARegister, kCapturedBlockA);
    bus.load(bme280::kCalibrationBlockBRegister, kCapturedBlockB);
    bus.load(bme280::kSampleRegister, kCapturedSampleBlock);
    return bus;
}

}

void setUp() {}
void tearDown() {}

void test_begin_succeeds_with_a_bme280_on_the_bus() {
    FakeRegisterBus bus = busWithCapturedSensor();
    bme280::Driver driver(bus);

    TEST_ASSERT_TRUE(driver.begin());
}

void test_begin_reads_chip_id_then_calibration_in_two_bursts() {
    FakeRegisterBus bus = busWithCapturedSensor();
    bme280::Driver driver(bus);

    driver.begin();

    TEST_ASSERT_EQUAL_UINT32(3, bus.reads.size());
    TEST_ASSERT_EQUAL_HEX8(0xD0, bus.reads[0].reg);
    TEST_ASSERT_EQUAL_UINT32(1, bus.reads[0].length);
    TEST_ASSERT_EQUAL_HEX8(0x88, bus.reads[1].reg);
    TEST_ASSERT_EQUAL_UINT32(26, bus.reads[1].length);
    TEST_ASSERT_EQUAL_HEX8(0xE1, bus.reads[2].reg);
    TEST_ASSERT_EQUAL_UINT32(7, bus.reads[2].length);
}

void test_begin_writes_ctrl_hum_before_ctrl_meas_and_leaves_sensor_asleep() {
    FakeRegisterBus bus = busWithCapturedSensor();
    bme280::Driver driver(bus);

    driver.begin();

    TEST_ASSERT_EQUAL_UINT32(2, bus.writes.size());
    TEST_ASSERT_EQUAL_HEX8(0xF2, bus.writes[0].reg);
    TEST_ASSERT_EQUAL_HEX8(0x01, bus.writes[0].value);
    TEST_ASSERT_EQUAL_HEX8(0xF4, bus.writes[1].reg);
    TEST_ASSERT_EQUAL_HEX8(0x24, bus.writes[1].value);
}

void test_begin_rejects_a_different_chip_without_writing_to_it() {
    FakeRegisterBus bus = busWithCapturedSensor();
    bus.registers[bme280::kChipIdRegister] = kBmp280ChipId;
    bme280::Driver driver(bus);

    TEST_ASSERT_FALSE(driver.begin());
    TEST_ASSERT_EQUAL_UINT32(0, bus.writes.size());
}

void test_begin_fails_when_any_read_fails() {
    for (int reg : {0xD0, 0x88, 0xE1}) {
        FakeRegisterBus bus = busWithCapturedSensor();
        bus.failReadOf = reg;
        bme280::Driver driver(bus);

        TEST_ASSERT_FALSE(driver.begin());
    }
}

void test_begin_fails_when_any_write_fails() {
    for (int reg : {0xF2, 0xF4}) {
        FakeRegisterBus bus = busWithCapturedSensor();
        bus.failWriteOf = reg;
        bme280::Driver driver(bus);

        TEST_ASSERT_FALSE(driver.begin());
    }
}

void test_start_measurement_writes_forced_mode() {
    FakeRegisterBus bus = busWithCapturedSensor();
    bme280::Driver driver(bus);
    driver.begin();
    bus.writes.clear();

    TEST_ASSERT_TRUE(driver.startMeasurement());

    TEST_ASSERT_EQUAL_UINT32(1, bus.writes.size());
    TEST_ASSERT_EQUAL_HEX8(0xF4, bus.writes[0].reg);
    TEST_ASSERT_EQUAL_HEX8(0x25, bus.writes[0].value);
}

void test_start_measurement_fails_when_the_write_fails() {
    FakeRegisterBus bus = busWithCapturedSensor();
    bme280::Driver driver(bus);
    driver.begin();
    bus.failWriteOf = bme280::kCtrlMeasRegister;

    TEST_ASSERT_FALSE(driver.startMeasurement());
}

void test_conversion_time_is_ten_milliseconds() {
    FakeRegisterBus bus;
    const bme280::Driver driver(bus);

    TEST_ASSERT_EQUAL_UINT32(10, driver.conversionTimeMs());
}

void test_read_takes_one_eight_byte_burst_from_0xF7() {
    FakeRegisterBus bus = busWithCapturedSensor();
    bme280::Driver driver(bus);
    driver.begin();
    bus.reads.clear();

    core::Measurement measurement{};
    TEST_ASSERT_TRUE(driver.read(measurement));

    TEST_ASSERT_EQUAL_UINT32(1, bus.reads.size());
    TEST_ASSERT_EQUAL_HEX8(0xF7, bus.reads[0].reg);
    TEST_ASSERT_EQUAL_UINT32(8, bus.reads[0].length);
}

void test_read_compensates_with_the_calibration_loaded_by_begin() {
    FakeRegisterBus bus = busWithCapturedSensor();
    bme280::Driver driver(bus);
    driver.begin();

    core::Measurement measurement{};
    driver.read(measurement);

    const bme280::Calibration c = bme280::parseCalibration(kCapturedBlockA, kCapturedBlockB);
    const bme280::RawSample raw = bme280::parseSample(kCapturedSampleBlock);
    const int32_t tFine = bme280::fineTemperature(c, raw.temperature);
    TEST_ASSERT_EQUAL_INT32(bme280::temperatureCentiCelsius(tFine), measurement.centiCelsius);
    TEST_ASSERT_EQUAL_UINT32(bme280::pressureQ24_8(c, raw.pressure, tFine),
                             measurement.pressureQ24_8);
    TEST_ASSERT_EQUAL_UINT32(bme280::humidityQ22_10(c, raw.humidity, tFine),
                             measurement.humidityQ22_10);
    TEST_ASSERT_EQUAL_INT32(2113, measurement.centiCelsius);
}

void test_read_fails_when_the_bus_read_fails() {
    FakeRegisterBus bus = busWithCapturedSensor();
    bme280::Driver driver(bus);
    driver.begin();
    bus.failReadOf = bme280::kSampleRegister;

    core::Measurement measurement{};
    TEST_ASSERT_FALSE(driver.read(measurement));
}

void test_read_fails_when_no_measurement_has_run() {
    FakeRegisterBus bus = busWithCapturedSensor();
    bus.load(bme280::kSampleRegister, kResetSampleBlock);
    bme280::Driver driver(bus);
    driver.begin();

    core::Measurement measurement{};
    TEST_ASSERT_FALSE(driver.read(measurement));
}

void test_read_fails_when_humidity_was_skipped() {
    FakeRegisterBus bus = busWithCapturedSensor();
    bus.load(bme280::kSampleRegister, kHumiditySkippedSampleBlock);
    bme280::Driver driver(bus);
    driver.begin();

    core::Measurement measurement{};
    TEST_ASSERT_FALSE(driver.read(measurement));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_begin_succeeds_with_a_bme280_on_the_bus);
    RUN_TEST(test_begin_reads_chip_id_then_calibration_in_two_bursts);
    RUN_TEST(test_begin_writes_ctrl_hum_before_ctrl_meas_and_leaves_sensor_asleep);
    RUN_TEST(test_begin_rejects_a_different_chip_without_writing_to_it);
    RUN_TEST(test_begin_fails_when_any_read_fails);
    RUN_TEST(test_begin_fails_when_any_write_fails);
    RUN_TEST(test_start_measurement_writes_forced_mode);
    RUN_TEST(test_start_measurement_fails_when_the_write_fails);
    RUN_TEST(test_conversion_time_is_ten_milliseconds);
    RUN_TEST(test_read_takes_one_eight_byte_burst_from_0xF7);
    RUN_TEST(test_read_compensates_with_the_calibration_loaded_by_begin);
    RUN_TEST(test_read_fails_when_the_bus_read_fails);
    RUN_TEST(test_read_fails_when_no_measurement_has_run);
    RUN_TEST(test_read_fails_when_humidity_was_skipped);
    return UNITY_END();
}
