#pragma once

#include <Arduino.h>

#include "transport.h"

class SerialTransport final : public core::ITransport {
public:
    explicit SerialTransport(Print& output);

    bool ready() override;
    bool publish(const char* payload, size_t length) override;

private:
    Print& output_;
};
