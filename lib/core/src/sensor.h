#pragma once

#include <cstdint>

#include "reading.h"

namespace core {

class ISensor {
public:
    virtual bool begin() = 0;
    virtual bool startMeasurement() = 0;
    virtual uint32_t conversionTimeMs() const = 0;
    virtual bool read(Measurement& measurement) = 0;

protected:
    ~ISensor() = default;
};

}
