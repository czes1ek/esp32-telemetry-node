#pragma once

#include <array>
#include <cstdint>

namespace bme280 {

constexpr uint8_t kCalibrationBlockARegister = 0x88;
constexpr uint8_t kCalibrationBlockBRegister = 0xE1;
constexpr uint8_t kSampleRegister            = 0xF7;

using CalibrationBlockA = std::array<uint8_t, 26>;
using CalibrationBlockB = std::array<uint8_t, 7>;
using SampleBlock       = std::array<uint8_t, 8>;

struct Calibration {
    uint16_t t1;
    int16_t  t2;
    int16_t  t3;
    uint16_t p1;
    int16_t  p2;
    int16_t  p3;
    int16_t  p4;
    int16_t  p5;
    int16_t  p6;
    int16_t  p7;
    int16_t  p8;
    int16_t  p9;
    uint8_t  h1;
    int16_t  h2;
    uint8_t  h3;
    int16_t  h4;
    int16_t  h5;
    int8_t   h6;
};

struct RawSample {
    int32_t pressure;
    int32_t temperature;
    int32_t humidity;
};

Calibration parseCalibration(const CalibrationBlockA& blockA,
                             const CalibrationBlockB& blockB);
RawSample parseSample(const SampleBlock& block);

int32_t fineTemperature(const Calibration& calibration, int32_t rawTemperature);
int32_t temperatureCentiCelsius(int32_t tFine);
uint32_t pressureQ24_8(const Calibration& calibration, int32_t rawPressure, int32_t tFine);
uint32_t humidityQ22_10(const Calibration& calibration, int32_t rawHumidity, int32_t tFine);

}
