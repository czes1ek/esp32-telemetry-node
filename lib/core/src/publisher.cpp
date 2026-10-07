#include "publisher.h"

#include "payload.h"

namespace core {

Publisher::Publisher(const char* deviceId, ReadingBuffer& buffer, const Sampler& sampler,
                     ITransport& transport)
    : deviceId_(deviceId), buffer_(buffer), sampler_(sampler), transport_(transport) {}

void Publisher::publishPending(uint32_t) {
    for (size_t sent = 0; sent < kMaxReadingsPerRun && !buffer_.empty(); ++sent) {
        if (!transport_.ready()) return;

        char payload[kPayloadCapacity];
        const Counters counters{buffer_.dropped(), sampler_.errorCount(), formatErrorCount_};
        const size_t length =
            formatPayload(payload, sizeof payload, deviceId_, buffer_.front(), counters);
        if (length == 0) {
            ++formatErrorCount_;
            buffer_.pop();
            continue;
        }
        if (!transport_.publish(payload, length)) return;

        buffer_.pop();
    }
}

uint32_t Publisher::formatErrorCount() const {
    return formatErrorCount_;
}

}
