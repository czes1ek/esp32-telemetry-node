#pragma once

#include <cstdint>

namespace core {

class IClock {
public:
    virtual uint32_t nowMs() const = 0;

protected:
    ~IClock() = default;
};

}
