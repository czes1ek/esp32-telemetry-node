#pragma once

#include <cstdint>

#include "clock.h"

class FakeClock final : public core::IClock {
public:
    uint32_t nowMs() const override { return now; }

    uint32_t now = 0;
};
