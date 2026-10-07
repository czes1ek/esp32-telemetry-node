#pragma once

#include <cstddef>
#include <cstdint>

namespace core {

class IRegisterBus {
public:
    virtual bool read(uint8_t reg, uint8_t* data, size_t length) = 0;
    virtual bool write(uint8_t reg, uint8_t value) = 0;

protected:
    ~IRegisterBus() = default;
};

}
