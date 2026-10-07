#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "transport.h"

class FakeTransport final : public core::ITransport {
public:
    static constexpr uint32_t kNeverFail = 0;

    bool ready() override { return isReady; }

    bool publish(const char* payload, size_t length) override {
        ++publishCalls;
        if (failFromCall != kNeverFail && publishCalls >= failFromCall) return false;
        payloads.emplace_back(payload, length);
        return true;
    }

    bool isReady = true;
    uint32_t failFromCall = kNeverFail;
    uint32_t publishCalls = 0;
    std::vector<std::string> payloads;
};
