#pragma once

#include <cstddef>
#include <cstdint>

#include "reading_buffer.h"
#include "sampler.h"
#include "transport.h"

namespace core {

class Publisher {
public:
    static constexpr size_t kMaxReadingsPerRun = 16;

    Publisher(const char* deviceId, ReadingBuffer& buffer, const Sampler& sampler,
              ITransport& transport);

    void publishPending(uint32_t nowMs);
    uint32_t formatErrorCount() const;

private:
    const char* deviceId_;
    ReadingBuffer& buffer_;
    const Sampler& sampler_;
    ITransport& transport_;
    uint32_t formatErrorCount_ = 0;
};

}
