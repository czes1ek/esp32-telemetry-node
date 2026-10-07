#include <Arduino.h>
#include <Wire.h>

#include <cstdio>

#include "bme280_driver.h"
#include "config.h"
#include "millis_clock.h"
#include "payload.h"
#include "publisher.h"
#include "reading_buffer.h"
#include "sampler.h"
#include "scheduler.h"
#include "serial_transport.h"
#include "wire_bus.h"

namespace {

constexpr uint8_t  kSensorAddress      = 0x76;
constexpr int      kSdaPin             = 21;
constexpr int      kSclPin             = 22;
constexpr uint32_t kI2cClockHz         = 100000;
constexpr uint32_t kSerialBaud         = 115200;
constexpr size_t   kSerialTxBufferSize = 4096;

char deviceId[core::kMaxDeviceIdLength + 1];

MillisClock systemClock;
WireBus sensorBus(Wire, kSensorAddress);
bme280::Driver sensor(sensorBus);
core::ReadingBuffer readings;
core::Sampler sampler(sensor, readings);
SerialTransport transport(Serial);
core::Publisher publisher(deviceId, readings, sampler, transport);

core::Scheduler scheduler(systemClock);
core::MethodTask<core::Sampler, &core::Sampler::start> startTask(sampler);
core::MethodTask<core::Sampler, &core::Sampler::collect> collectTask(sampler);
core::MethodTask<core::Publisher, &core::Publisher::publishPending> publishTask(publisher);

void formatDeviceId() {
    const uint64_t mac = ESP.getEfuseMac();
    std::snprintf(deviceId, sizeof deviceId, "esp32-%02x%02x%02x%02x%02x%02x",
                  (unsigned)(mac & 0xFF), (unsigned)((mac >> 8) & 0xFF),
                  (unsigned)((mac >> 16) & 0xFF), (unsigned)((mac >> 24) & 0xFF),
                  (unsigned)((mac >> 32) & 0xFF), (unsigned)((mac >> 40) & 0xFF));
}

}

void setup() {
    const bool txBufferResized =
        Serial.setTxBufferSize(kSerialTxBufferSize) == kSerialTxBufferSize;
    Serial.begin(kSerialBaud);
    delay(2000);

    Wire.begin(kSdaPin, kSclPin);
    Wire.setClock(kI2cClockHz);

    formatDeviceId();
    Serial.println();
    Serial.print("=== telemetry node ");
    Serial.print(deviceId);
    Serial.println(" ===");
    if (!txBufferResized) Serial.println("WARN: serial TX buffer was not resized");

    const bool scheduled = scheduler.add(startTask, core::kSampleIntervalMs) &&
                           scheduler.add(collectTask, core::kCollectPollIntervalMs) &&
                           scheduler.add(publishTask, core::kPublishIntervalMs);
    if (!scheduled) Serial.println("FATAL: scheduler task table is full");
}

void loop() {
    scheduler.tick();
    delay(scheduler.timeUntilNextDueMs());
}
