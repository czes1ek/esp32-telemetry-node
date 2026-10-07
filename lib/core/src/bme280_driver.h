#pragma once

#include <cstdint>

#include "bme280_compensation.h"
#include "register_bus.h"
#include "sensor.h"

namespace bme280 {

constexpr uint8_t kChipIdRegister   = 0xD0;
constexpr uint8_t kCtrlHumRegister  = 0xF2;
constexpr uint8_t kCtrlMeasRegister = 0xF4;

constexpr uint8_t kChipId                   = 0x60;
constexpr uint8_t kHumidityOversamplingX1   = 0x01;
constexpr uint8_t kSleepModeOversamplingX1  = 0x24;
constexpr uint8_t kForcedModeOversamplingX1 = 0x25;

constexpr int32_t kSkippedRaw20Bit = 0x80000;
constexpr int32_t kSkippedRaw16Bit = 0x8000;

constexpr uint32_t kConversionTimeMs = 10;

class Driver final : public core::ISensor {
public:
    explicit Driver(core::IRegisterBus& bus);

    bool begin() override;
    bool startMeasurement() override;
    uint32_t conversionTimeMs() const override;
    bool read(core::Measurement& measurement) override;

private:
    core::IRegisterBus& bus_;
    Calibration calibration_{};
};

}
