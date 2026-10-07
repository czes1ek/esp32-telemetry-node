#pragma once

#include <cstdint>

#include "sensor.h"

class FakeSensor final : public core::ISensor {
public:
    bool begin() override {
        ++beginCalls;
        return beginSucceeds;
    }

    bool startMeasurement() override {
        ++startCalls;
        return startSucceeds;
    }

    uint32_t conversionTimeMs() const override { return conversionTime; }

    bool read(core::Measurement& out) override {
        ++readCalls;
        if (!readSucceeds) return false;
        out = measurement;
        return true;
    }

    bool beginSucceeds = true;
    bool startSucceeds = true;
    bool readSucceeds = true;
    uint32_t conversionTime = 10;
    core::Measurement measurement{2113, 25264070, 51156};

    uint32_t beginCalls = 0;
    uint32_t startCalls = 0;
    uint32_t readCalls = 0;
};
