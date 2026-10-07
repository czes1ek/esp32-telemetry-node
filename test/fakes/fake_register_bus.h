#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "register_bus.h"

class FakeRegisterBus final : public core::IRegisterBus {
public:
    struct ReadCall {
        uint8_t reg;
        size_t  length;
    };

    struct WriteCall {
        uint8_t reg;
        uint8_t value;
    };

    static constexpr int kNoRegister = -1;

    bool read(uint8_t reg, uint8_t* data, size_t length) override {
        reads.push_back(ReadCall{reg, length});
        if (reg == failReadOf) return false;
        for (size_t i = 0; i < length; ++i) data[i] = registers[(reg + i) % registers.size()];
        return true;
    }

    bool write(uint8_t reg, uint8_t value) override {
        writes.push_back(WriteCall{reg, value});
        if (reg == failWriteOf) return false;
        registers[reg] = value;
        return true;
    }

    template <size_t N>
    void load(uint8_t reg, const std::array<uint8_t, N>& bytes) {
        for (size_t i = 0; i < N; ++i) registers[(reg + i) % registers.size()] = bytes[i];
    }

    std::array<uint8_t, 256> registers{};
    std::vector<ReadCall> reads;
    std::vector<WriteCall> writes;
    int failReadOf = kNoRegister;
    int failWriteOf = kNoRegister;
};
