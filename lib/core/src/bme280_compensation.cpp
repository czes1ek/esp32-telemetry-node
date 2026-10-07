#include "bme280_compensation.h"

namespace bme280 {

static_assert((-1 >> 1) == -1,
              "datasheet compensation relies on arithmetic right shift of negative values");

namespace {

uint16_t unsignedLE(uint8_t lo, uint8_t hi) {
    return static_cast<uint16_t>(hi << 8 | lo);
}

int16_t signedLE(uint8_t lo, uint8_t hi) {
    return static_cast<int16_t>(unsignedLE(lo, hi));
}

int16_t signed12(uint8_t upper8, uint8_t lower4) {
    return static_cast<int16_t>((static_cast<int8_t>(upper8) * 16) | lower4);
}

int32_t unsigned20(uint8_t msb, uint8_t lsb, uint8_t xlsb) {
    return (static_cast<int32_t>(msb) << 12) |
           (static_cast<int32_t>(lsb) << 4) |
           (xlsb >> 4);
}

}

Calibration parseCalibration(const CalibrationBlockA& a,
                             const CalibrationBlockB& b) {
    Calibration c{};
    c.t1 = unsignedLE(a[0], a[1]);
    c.t2 = signedLE(a[2], a[3]);
    c.t3 = signedLE(a[4], a[5]);
    c.p1 = unsignedLE(a[6], a[7]);
    c.p2 = signedLE(a[8], a[9]);
    c.p3 = signedLE(a[10], a[11]);
    c.p4 = signedLE(a[12], a[13]);
    c.p5 = signedLE(a[14], a[15]);
    c.p6 = signedLE(a[16], a[17]);
    c.p7 = signedLE(a[18], a[19]);
    c.p8 = signedLE(a[20], a[21]);
    c.p9 = signedLE(a[22], a[23]);
    c.h1 = a[25];
    c.h2 = signedLE(b[0], b[1]);
    c.h3 = b[2];
    c.h4 = signed12(b[3], b[4] & 0x0F);
    c.h5 = signed12(b[5], b[4] >> 4);
    c.h6 = static_cast<int8_t>(b[6]);
    return c;
}

RawSample parseSample(const SampleBlock& d) {
    RawSample raw{};
    raw.pressure    = unsigned20(d[0], d[1], d[2]);
    raw.temperature = unsigned20(d[3], d[4], d[5]);
    raw.humidity    = (static_cast<int32_t>(d[6]) << 8) | d[7];
    return raw;
}

int32_t fineTemperature(const Calibration& calibration, int32_t rawTemperature) {
    const int32_t t1 = calibration.t1;
    const int32_t var1 = (((rawTemperature >> 3) - (t1 << 1)) * calibration.t2) >> 11;
    const int32_t offset = (rawTemperature >> 4) - t1;
    const int32_t var2 = (((offset * offset) >> 12) * calibration.t3) >> 14;
    return var1 + var2;
}

int32_t temperatureCentiCelsius(int32_t tFine) {
    return (tFine * 5 + 128) >> 8;
}

}
