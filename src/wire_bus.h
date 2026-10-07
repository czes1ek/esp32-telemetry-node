#pragma once

#include <Wire.h>

#include "register_bus.h"

class WireBus final : public core::IRegisterBus {
public:
    WireBus(TwoWire& wire, uint8_t address);

    bool read(uint8_t reg, uint8_t* data, size_t length) override;
    bool write(uint8_t reg, uint8_t value) override;

private:
    TwoWire& wire_;
    uint8_t address_;
};
