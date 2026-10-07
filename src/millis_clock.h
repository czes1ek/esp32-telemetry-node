#pragma once

#include <Arduino.h>

#include "clock.h"

class MillisClock final : public core::IClock {
public:
    uint32_t nowMs() const override { return millis(); }
};
