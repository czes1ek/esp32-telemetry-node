#include "payload.h"

#include <cstdio>

namespace core {

namespace {

unsigned long whole(uint64_t hundredths) {
    return static_cast<unsigned long>(hundredths / 100);
}

unsigned long fraction(uint64_t hundredths) {
    return static_cast<unsigned long>(hundredths % 100);
}

}

size_t formatPayload(char* out, size_t capacity, const char* deviceId, const Reading& reading,
                     const Counters& counters) {
    const Measurement& m = reading.measurement;

    const char* sign = m.centiCelsius < 0 ? "-" : "";
    const uint64_t centiCelsius = m.centiCelsius < 0 ? -static_cast<int64_t>(m.centiCelsius)
                                                     : static_cast<int64_t>(m.centiCelsius);
    const uint64_t centiPascal = (static_cast<uint64_t>(m.pressureQ24_8) * 100 + 128) / 256;
    const uint64_t centiPercent = (static_cast<uint64_t>(m.humidityQ22_10) * 100 + 512) / 1024;

    const int written = std::snprintf(
        out, capacity,
        "{\"device\":\"%s\",\"uptime_ms\":%lu,\"temperature_c\":%s%lu.%02lu,"
        "\"station_pressure_pa\":%lu.%02lu,\"humidity_pct\":%lu.%02lu,"
        "\"dropped\":%lu,\"sensor_errors\":%lu}",
        deviceId, static_cast<unsigned long>(reading.uptimeMs),
        sign, whole(centiCelsius), fraction(centiCelsius),
        whole(centiPascal), fraction(centiPascal),
        whole(centiPercent), fraction(centiPercent),
        static_cast<unsigned long>(counters.dropped),
        static_cast<unsigned long>(counters.sensorErrors));

    if (written < 0 || static_cast<size_t>(written) >= capacity) return 0;
    return static_cast<size_t>(written);
}

}
