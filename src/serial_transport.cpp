#include "serial_transport.h"

SerialTransport::SerialTransport(Print& output) : output_(output) {}

bool SerialTransport::ready() {
    return true;
}

bool SerialTransport::publish(const char* payload, size_t length) {
    output_.write(payload, length);
    output_.println();
    return true;
}
