#include "sampler.h"

namespace core {

Sampler::Sampler(ISensor& sensor, ReadingBuffer& buffer) : sensor_(sensor), buffer_(buffer) {}

void Sampler::start(uint32_t nowMs) {
    if (pending_) ++errorCount_;
    pending_ = false;

    if (!sensorReady_) sensorReady_ = sensor_.begin();
    if (!sensorReady_ || !sensor_.startMeasurement()) {
        recordFailure();
        return;
    }

    pending_ = true;
    startedAtMs_ = nowMs;
}

void Sampler::collect(uint32_t nowMs) {
    if (!pending_) return;
    if (nowMs - startedAtMs_ < sensor_.conversionTimeMs()) return;
    pending_ = false;

    Reading reading{};
    reading.uptimeMs = startedAtMs_;
    if (!sensor_.read(reading.measurement)) {
        recordFailure();
        return;
    }
    buffer_.push(reading);
}

uint32_t Sampler::errorCount() const {
    return errorCount_;
}

void Sampler::recordFailure() {
    ++errorCount_;
    sensorReady_ = false;
    pending_ = false;
}

}
