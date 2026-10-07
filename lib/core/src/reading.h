#pragma once

#include <cstdint>

namespace core {

struct Measurement {
    int32_t  centiCelsius;
    uint32_t pressureQ24_8;
    uint32_t humidityQ22_10;
};

struct Reading {
    uint32_t    uptimeMs;
    Measurement measurement;
};

}
