#pragma once

#include <cstdint>

#include "reading_buffer.h"
#include "sensor.h"

namespace core {

class Sampler {
public:
    Sampler(ISensor& sensor, ReadingBuffer& buffer);

    void start(uint32_t nowMs);
    void collect(uint32_t nowMs);
    uint32_t errorCount() const;

private:
    void recordFailure();

    ISensor& sensor_;
    ReadingBuffer& buffer_;
    bool sensorReady_ = false;
    bool pending_ = false;
    uint32_t startedAtMs_ = 0;
    uint32_t errorCount_ = 0;
};

}
