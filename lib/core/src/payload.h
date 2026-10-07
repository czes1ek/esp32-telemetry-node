#pragma once

#include <cstddef>
#include <cstdint>

#include "reading.h"

namespace core {

constexpr size_t kPayloadCapacity   = 256;
constexpr size_t kMaxDeviceIdLength = 32;

struct Counters {
    uint32_t dropped;
    uint32_t sensorErrors;
    uint32_t formatErrors;
};

size_t formatPayload(char* out, size_t capacity, const char* deviceId, const Reading& reading,
                     const Counters& counters);

}
