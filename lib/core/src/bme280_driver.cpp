#include "bme280_driver.h"

namespace bme280 {

Driver::Driver(core::IRegisterBus& bus) : bus_(bus) {}

bool Driver::begin() {
    uint8_t chipId = 0;
    if (!bus_.read(kChipIdRegister, &chipId, 1)) return false;
    if (chipId != kChipId) return false;

    CalibrationBlockA blockA{};
    CalibrationBlockB blockB{};
    if (!bus_.read(kCalibrationBlockARegister, blockA.data(), blockA.size())) return false;
    if (!bus_.read(kCalibrationBlockBRegister, blockB.data(), blockB.size())) return false;
    calibration_ = parseCalibration(blockA, blockB);

    if (!bus_.write(kCtrlHumRegister, kHumidityOversamplingX1)) return false;
    return bus_.write(kCtrlMeasRegister, kSleepModeOversamplingX1);
}

bool Driver::startMeasurement() {
    return bus_.write(kCtrlMeasRegister, kForcedModeOversamplingX1);
}

uint32_t Driver::conversionTimeMs() const {
    return kConversionTimeMs;
}

bool Driver::read(core::Measurement& measurement) {
    SampleBlock block{};
    if (!bus_.read(kSampleRegister, block.data(), block.size())) return false;

    const RawSample raw = parseSample(block);
    if (raw.temperature == kSkippedRaw20Bit) return false;
    if (raw.pressure == kSkippedRaw20Bit) return false;
    if (raw.humidity == kSkippedRaw16Bit) return false;

    const int32_t tFine = fineTemperature(calibration_, raw.temperature);
    measurement.centiCelsius   = temperatureCentiCelsius(tFine);
    measurement.pressureQ24_8  = pressureQ24_8(calibration_, raw.pressure, tFine);
    measurement.humidityQ22_10 = humidityQ22_10(calibration_, raw.humidity, tFine);
    return true;
}

}
