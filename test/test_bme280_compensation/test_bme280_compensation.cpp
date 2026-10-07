#include <unity.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "bme280_compensation.h"

namespace {

constexpr char kBmp280DatasheetExample[] =
    "Bosch BMP280 datasheet BST-BMP280-DS001, worked example in section 3.12 "
    "'Calculating pressure and temperature'";

constexpr int32_t kExampleRawTemperature = 519888;
constexpr int32_t kExampleRawPressure    = 415148;
constexpr int32_t kExampleFineTemperature = 128422;
constexpr int32_t kExampleCentiCelsius    = 2508;

constexpr int32_t kExampleCentiPascal     = 10065327;

constexpr char kBme280DatasheetDoubleFormulas[] =
    "Bosch BME280 datasheet BST-BME280-DS001, section 8.1 "
    "'Compensation formulas in double precision floating point'";

constexpr int32_t kFineTemperatureTolerance = 1;
constexpr int32_t kCentiCelsiusTolerance    = 1;
constexpr int32_t kCentiPascalTolerance     = 2;
constexpr int32_t kHumidityToleranceQ22_10  = 16;

constexpr int32_t kColdestFineTemperature = -204800;
constexpr int32_t kHottestFineTemperature = 435200;
constexpr int32_t kFineTemperatureStep    = 6400;

constexpr uint32_t kHundredPercentQ22_10 = 100 * 1024;

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

bme280::Calibration withHumidity(uint8_t h1, int16_t h2, uint8_t h3,
                                 int16_t h4, int16_t h5, int8_t h6) {
    bme280::Calibration c = bmp280DatasheetCalibration();
    c.h1 = h1;
    c.h2 = h2;
    c.h3 = h3;
    c.h4 = h4;
    c.h5 = h5;
    c.h6 = h6;
    return c;
}

bme280::Calibration typicalHumidityCalibration() {
    return withHumidity(75, 366, 0, 309, 50, 30);
}

int32_t centiPascal(uint32_t pressureQ24_8) {
    return static_cast<int32_t>((static_cast<uint64_t>(pressureQ24_8) * 100 + 128) / 256);
}

double datasheetPressureDouble(const bme280::Calibration& c, int32_t rawPressure,
                               int32_t tFine) {
    double var1 = tFine / 2.0 - 64000.0;
    double var2 = var1 * var1 * c.p6 / 32768.0;
    var2 = var2 + var1 * c.p5 * 2.0;
    var2 = var2 / 4.0 + c.p4 * 65536.0;
    var1 = (c.p3 * var1 * var1 / 524288.0 + c.p2 * var1) / 524288.0;
    var1 = (1.0 + var1 / 32768.0) * c.p1;
    double p = 1048576.0 - rawPressure;
    p = (p - var2 / 4096.0) * 6250.0 / var1;
    var1 = c.p9 * p * p / 2147483648.0;
    var2 = p * c.p8 / 32768.0;
    return p + (var1 + var2 + c.p7) / 16.0;
}

double datasheetHumidityDouble(const bme280::Calibration& c, int32_t rawHumidity,
                               int32_t tFine) {
    double h = tFine - 76800.0;
    h = (rawHumidity - (c.h4 * 64.0 + c.h5 / 16384.0 * h)) *
        (c.h2 / 65536.0 * (1.0 + c.h6 / 67108864.0 * h * (1.0 + c.h3 / 67108864.0 * h)));
    h = h * (1.0 - c.h1 * h / 524288.0);
    if (h > 100.0) return 100.0;
    if (h < 0.0) return 0.0;
    return h;
}

void assertHumidityTracksDatasheetDouble(const bme280::Calibration& c) {
    for (int32_t raw = 15000; raw <= 50000; raw += 250) {
        for (int32_t tFine = kColdestFineTemperature; tFine <= kHottestFineTemperature;
             tFine += kFineTemperatureStep) {
            const long expected = std::lround(datasheetHumidityDouble(c, raw, tFine) * 1024.0);
            const uint32_t actual = bme280::humidityQ22_10(c, raw, tFine);
            TEST_ASSERT_INT32_WITHIN_MESSAGE(kHumidityToleranceQ22_10,
                                             static_cast<int32_t>(expected),
                                             static_cast<int32_t>(actual),
                                             kBme280DatasheetDoubleFormulas);
        }
    }
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

void test_pressure_matches_bmp280_datasheet_example() {
    const bme280::Calibration c = bmp280DatasheetCalibration();
    const int32_t tFine = bme280::fineTemperature(c, kExampleRawTemperature);

    const uint32_t pressure = bme280::pressureQ24_8(c, kExampleRawPressure, tFine);

    TEST_ASSERT_INT32_WITHIN_MESSAGE(kCentiPascalTolerance, kExampleCentiPascal,
                                     centiPascal(pressure), kBmp280DatasheetExample);
}

void test_pressure_tracks_datasheet_double_formula_across_temperature() {
    const bme280::Calibration c = bmp280DatasheetCalibration();

    for (int32_t raw = 250000; raw <= 600000; raw += 2500) {
        for (int32_t tFine = kColdestFineTemperature; tFine <= kHottestFineTemperature;
             tFine += kFineTemperatureStep) {
            const long expected = std::lround(datasheetPressureDouble(c, raw, tFine) * 100.0);
            const uint32_t actual = bme280::pressureQ24_8(c, raw, tFine);
            TEST_ASSERT_INT32_WITHIN_MESSAGE(kCentiPascalTolerance,
                                             static_cast<int32_t>(expected),
                                             centiPascal(actual),
                                             kBme280DatasheetDoubleFormulas);
        }
    }
}

void test_pressure_is_zero_when_calibration_would_divide_by_zero() {
    bme280::Calibration c = bmp280DatasheetCalibration();
    c.p1 = 0;

    TEST_ASSERT_EQUAL_UINT32(0, bme280::pressureQ24_8(c, kExampleRawPressure,
                                                      kExampleFineTemperature));
}

void test_humidity_tracks_datasheet_double_formula() {
    assertHumidityTracksDatasheetDouble(typicalHumidityCalibration());
}

void test_humidity_tracks_datasheet_double_formula_with_negative_h6() {
    assertHumidityTracksDatasheetDouble(withHumidity(75, 370, 0, 300, 50, -20));
}

void test_humidity_tracks_datasheet_double_formula_with_nonzero_h3() {
    assertHumidityTracksDatasheetDouble(withHumidity(75, 366, 5, 309, 50, 30));
}

void test_humidity_clamps_at_zero_percent() {
    TEST_ASSERT_EQUAL_UINT32(0, bme280::humidityQ22_10(typicalHumidityCalibration(), 0,
                                                       kExampleFineTemperature));
}

void test_humidity_clamps_at_hundred_percent() {
    TEST_ASSERT_EQUAL_UINT32(kHundredPercentQ22_10,
                             bme280::humidityQ22_10(typicalHumidityCalibration(), 65535,
                                                    kExampleFineTemperature));
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
    RUN_TEST(test_pressure_matches_bmp280_datasheet_example);
    RUN_TEST(test_pressure_tracks_datasheet_double_formula_across_temperature);
    RUN_TEST(test_pressure_is_zero_when_calibration_would_divide_by_zero);
    RUN_TEST(test_humidity_tracks_datasheet_double_formula);
    RUN_TEST(test_humidity_tracks_datasheet_double_formula_with_negative_h6);
    RUN_TEST(test_humidity_tracks_datasheet_double_formula_with_nonzero_h3);
    RUN_TEST(test_humidity_clamps_at_zero_percent);
    RUN_TEST(test_humidity_clamps_at_hundred_percent);
    return UNITY_END();
}
