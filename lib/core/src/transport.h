#pragma once

#include <cstddef>

namespace core {

class ITransport {
public:
    virtual bool ready() = 0;
    virtual bool publish(const char* payload, size_t length) = 0;

protected:
    ~ITransport() = default;
};

}
