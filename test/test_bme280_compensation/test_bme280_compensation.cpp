#include <unity.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include "bme280_compensation.h"

namespace {

constexpr char kBmp280DatasheetExample[] =
    "Bosch BMP280 datasheet BST-BMP280-DS001, worked example in "
    "'Calculating pressure and temperature'";

constexpr int32_t kExampleRawTemperature = 519888;
constexpr int32_t kExampleRawPressure    = 415148;
constexpr int32_t kExampleFineTemperature = 128422;
constexpr int32_t kExampleCentiCelsius    = 2508;

constexpr int32_t kFineTemperatureTolerance = 1;
constexpr int32_t kCentiCelsiusTolerance    = 1;

bme280::Calibration bmp280DatasheetCalibration() {
    bme280::Calibration c{};
    c.t1 = 27504;
    c.t2 = 26435;
    c.t3 = -1000;
    c.p1 = 36477;
    c.p2 = -10685;
    c.p3 = 3024;
    c.p4 = 2855;
    c.p5 = 140;
    c.p6 = -7;
    c.p7 = 15500;
    c.p8 = -14600;
    c.p9 = 6000;
    return c;
}

void putLE(bme280::CalibrationBlockA& block, size_t index, int32_t value) {
    block[index]     = static_cast<uint8_t>(value & 0xFF);
    block[index + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
}

bme280::CalibrationBlockA blockAFrom(const bme280::Calibration& c) {
    bme280::CalibrationBlockA block{};
    putLE(block, 0, c.t1);
    putLE(block, 2, c.t2);
    putLE(block, 4, c.t3);
    putLE(block, 6, c.p1);
    putLE(block, 8, c.p2);
    putLE(block, 10, c.p3);
    putLE(block, 12, c.p4);
    putLE(block, 14, c.p5);
    putLE(block, 16, c.p6);
    putLE(block, 18, c.p7);
    putLE(block, 20, c.p8);
    putLE(block, 22, c.p9);
    block[25] = c.h1;
    return block;
}

}

void setUp() {}
void tearDown() {}

void test_calibration_block_a_is_little_endian_words() {
    bme280::Calibration expected = bmp280DatasheetCalibration();
    expected.h1 = 75;

    const bme280::Calibration parsed =
        bme280::parseCalibration(blockAFrom(expected), bme280::CalibrationBlockB{});

    TEST_ASSERT_EQUAL_UINT16(expected.t1, parsed.t1);
    TEST_ASSERT_EQUAL_INT16(expected.t2, parsed.t2);
    TEST_ASSERT_EQUAL_INT16(expected.t3, parsed.t3);
    TEST_ASSERT_EQUAL_UINT16(expected.p1, parsed.p1);
    TEST_ASSERT_EQUAL_INT16(expected.p2, parsed.p2);
    TEST_ASSERT_EQUAL_INT16(expected.p3, parsed.p3);
    TEST_ASSERT_EQUAL_INT16(expected.p4, parsed.p4);
    TEST_ASSERT_EQUAL_INT16(expected.p5, parsed.p5);
    TEST_ASSERT_EQUAL_INT16(expected.p6, parsed.p6);
    TEST_ASSERT_EQUAL_INT16(expected.p7, parsed.p7);
    TEST_ASSERT_EQUAL_INT16(expected.p8, parsed.p8);
    TEST_ASSERT_EQUAL_INT16(expected.p9, parsed.p9);
    TEST_ASSERT_EQUAL_UINT8(expected.h1, parsed.h1);
}

void test_calibration_block_a_skips_reserved_byte_before_h1() {
    bme280::CalibrationBlockA blockA{};
    blockA[24] = 0xEE;
    blockA[25] = 0x4B;

    const bme280::Calibration parsed =
        bme280::parseCalibration(blockA, bme280::CalibrationBlockB{});

    TEST_ASSERT_EQUAL_UINT8(0x4B, parsed.h1);
}

void test_calibration_block_b_unpacks_shared_nibbles() {
    const bme280::CalibrationBlockB blockB{0x6E, 0x01, 0x00, 0x14, 0x5A, 0x03, 0x1E};

    const bme280::Calibration parsed =
        bme280::parseCalibration(bme280::CalibrationBlockA{}, blockB);

    TEST_ASSERT_EQUAL_INT16(366, parsed.h2);
    TEST_ASSERT_EQUAL_UINT8(0, parsed.h3);
    TEST_ASSERT_EQUAL_INT16(0x14A, parsed.h4);
    TEST_ASSERT_EQUAL_INT16(0x035, parsed.h5);
    TEST_ASSERT_EQUAL_INT8(30, parsed.h6);
}

void test_calibration_block_b_sign_extends_from_upper_byte() {
    const bme280::CalibrationBlockB blockB{0xFE, 0xFF, 0xFF, 0xF0, 0x73, 0x80, 0xFF};

    const bme280::Calibration parsed =
        bme280::parseCalibration(bme280::CalibrationBlockA{}, blockB);

    TEST_ASSERT_EQUAL_INT16(-2, parsed.h2);
    TEST_ASSERT_EQUAL_UINT8(255, parsed.h3);
    TEST_ASSERT_EQUAL_INT16(-253, parsed.h4);
    TEST_ASSERT_EQUAL_INT16(-2041, parsed.h5);
    TEST_ASSERT_EQUAL_INT8(-1, parsed.h6);
}

void test_sample_block_unpacks_20_bit_and_16_bit_fields() {
    const bme280::SampleBlock block{0x65, 0x5A, 0xC0, 0x7E, 0xED, 0x00, 0x80, 0x01};

    const bme280::RawSample raw = bme280::parseSample(block);

    TEST_ASSERT_EQUAL_INT32(kExampleRawPressure, raw.pressure);
    TEST_ASSERT_EQUAL_INT32(kExampleRawTemperature, raw.temperature);
    TEST_ASSERT_EQUAL_INT32(0x8001, raw.humidity);
}

void test_sample_block_ignores_low_nibble_of_xlsb() {
    const bme280::SampleBlock block{0x65, 0x5A, 0xCF, 0x7E, 0xED, 0x0F, 0x00, 0x00};

    const bme280::RawSample raw = bme280::parseSample(block);

    TEST_ASSERT_EQUAL_INT32(kExampleRawPressure, raw.pressure);
    TEST_ASSERT_EQUAL_INT32(kExampleRawTemperature, raw.temperature);
}

void test_sample_block_all_ones_stays_positive() {
    const bme280::SampleBlock block{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    const bme280::RawSample raw = bme280::parseSample(block);

    TEST_ASSERT_EQUAL_INT32(0xFFFFF, raw.pressure);
    TEST_ASSERT_EQUAL_INT32(0xFFFFF, raw.temperature);
    TEST_ASSERT_EQUAL_INT32(0xFFFF, raw.humidity);
}

void test_fine_temperature_matches_bmp280_datasheet_example() {
    const int32_t tFine =
        bme280::fineTemperature(bmp280DatasheetCalibration(), kExampleRawTemperature);

    TEST_ASSERT_INT32_WITHIN_MESSAGE(kFineTemperatureTolerance, kExampleFineTemperature,
                                     tFine, kBmp280DatasheetExample);
}

void test_temperature_matches_bmp280_datasheet_example() {
    const int32_t tFine =
        bme280::fineTemperature(bmp280DatasheetCalibration(), kExampleRawTemperature);

    TEST_ASSERT_INT32_WITHIN_MESSAGE(kCentiCelsiusTolerance, kExampleCentiCelsius,
                                     bme280::temperatureCentiCelsius(tFine),
                                     kBmp280DatasheetExample);
}

void test_temperature_rounds_to_nearest_centidegree() {
    TEST_ASSERT_EQUAL_INT32(0, bme280::temperatureCentiCelsius(0));
    TEST_ASSERT_EQUAL_INT32(2500, bme280::temperatureCentiCelsius(128000));
    TEST_ASSERT_EQUAL_INT32(2500, bme280::temperatureCentiCelsius(128025));
    TEST_ASSERT_EQUAL_INT32(2501, bme280::temperatureCentiCelsius(128026));
    TEST_ASSERT_EQUAL_INT32(-1000, bme280::temperatureCentiCelsius(-51200));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_calibration_block_a_is_little_endian_words);
    RUN_TEST(test_calibration_block_a_skips_reserved_byte_before_h1);
    RUN_TEST(test_calibration_block_b_unpacks_shared_nibbles);
    RUN_TEST(test_calibration_block_b_sign_extends_from_upper_byte);
    RUN_TEST(test_sample_block_unpacks_20_bit_and_16_bit_fields);
    RUN_TEST(test_sample_block_ignores_low_nibble_of_xlsb);
    RUN_TEST(test_sample_block_all_ones_stays_positive);
    RUN_TEST(test_fine_temperature_matches_bmp280_datasheet_example);
    RUN_TEST(test_temperature_matches_bmp280_datasheet_example);
    RUN_TEST(test_temperature_rounds_to_nearest_centidegree);
    return UNITY_END();
}
