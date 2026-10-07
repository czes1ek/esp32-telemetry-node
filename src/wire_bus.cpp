#include "wire_bus.h"

WireBus::WireBus(TwoWire& wire, uint8_t address) : wire_(wire), address_(address) {}

bool WireBus::read(uint8_t reg, uint8_t* data, size_t length) {
    wire_.beginTransmission(address_);
    wire_.write(reg);
    if (wire_.endTransmission(false) != 0) return false;
    if (wire_.requestFrom(address_, length) != length) return false;
    for (size_t i = 0; i < length; ++i) data[i] = wire_.read();
    return true;
}

bool WireBus::write(uint8_t reg, uint8_t value) {
    wire_.beginTransmission(address_);
    wire_.write(reg);
    wire_.write(value);
    return wire_.endTransmission() == 0;
}
