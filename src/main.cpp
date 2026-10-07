#include <Arduino.h>
#include <Wire.h>

#include <array>

#include "bme280_compensation.h"

namespace reg {
    constexpr uint8_t CHIP_ID   = 0xD0;
    constexpr uint8_t CTRL_HUM  = 0xF2;
    constexpr uint8_t CTRL_MEAS = 0xF4;
}

constexpr uint8_t kAddr   = 0x76;
constexpr uint8_t kChipId = 0x60;
constexpr int     kSdaPin = 21;
constexpr int     kSclPin = 22;

bme280::Calibration calibration;

bool readBurst(uint8_t reg, uint8_t* buf, uint8_t len) {
    Wire.beginTransmission(kAddr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(kAddr, len) != len) return false;
    for (uint8_t i = 0; i < len; ++i) buf[i] = Wire.read();
    return true;
}

template <size_t N>
bool readBlock(uint8_t reg, std::array<uint8_t, N>& block) {
    return readBurst(reg, block.data(), (uint8_t)N);
}

bool writeRegister(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(kAddr);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

template <size_t N>
void printHex(const std::array<uint8_t, N>& block) {
    for (uint8_t byte : block) {
        if (byte < 0x10) Serial.print('0');
        Serial.print(byte, HEX);
        Serial.print(' ');
    }
}

[[noreturn]] void halt(const char* reason) {
    Serial.print("FATAL: ");
    Serial.println(reason);
    while (true) delay(1000);
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println();
    Serial.println("=== BME280 temperature, pressure, humidity ===");

    Wire.begin(kSdaPin, kSclPin);
    Wire.setClock(100000);
    delay(100);

    uint8_t id = 0;
    if (!readBurst(reg::CHIP_ID, &id, 1)) halt("no response from sensor");
    if (id != kChipId) {
        Serial.print("chip id 0x");
        Serial.println(id, HEX);
        halt("unexpected chip id");
    }

    bme280::CalibrationBlockA blockA;
    bme280::CalibrationBlockB blockB;
    if (!readBlock(bme280::kCalibrationBlockARegister, blockA)) halt("calibration block A read failed");
    if (!readBlock(bme280::kCalibrationBlockBRegister, blockB)) halt("calibration block B read failed");
    calibration = bme280::parseCalibration(blockA, blockB);

    Serial.print("calibration A: ");
    printHex(blockA);
    Serial.println();
    Serial.print("calibration B: ");
    printHex(blockB);
    Serial.println();

    if (!writeRegister(reg::CTRL_HUM, 0x01)) halt("ctrl_hum write failed");
    if (!writeRegister(reg::CTRL_MEAS, 0x27)) halt("ctrl_meas write failed");

    delay(100);
    Serial.println("ready");
}

void loop() {
    bme280::SampleBlock block;
    if (!readBlock(bme280::kSampleRegister, block)) {
        Serial.println("read failed");
        delay(2000);
        return;
    }

    const bme280::RawSample raw = bme280::parseSample(block);
    const int32_t tFine = bme280::fineTemperature(calibration, raw.temperature);

    const float tempC = bme280::temperatureCentiCelsius(tFine) / 100.0f;
    const float tempF = tempC * 1.8f + 32.0f;
    const float stationHpa = bme280::pressureQ24_8(calibration, raw.pressure, tFine) / 25600.0f;
    const float humidity = bme280::humidityQ22_10(calibration, raw.humidity, tFine) / 1024.0f;

    Serial.print("raw: ");
    printHex(block);
    Serial.print(" Temp: ");
    Serial.print(tempC);
    Serial.print(" C  /  ");
    Serial.print(tempF);
    Serial.print(" F   Station pressure: ");
    Serial.print(stationHpa);
    Serial.print(" hPa   Humidity: ");
    Serial.print(humidity);
    Serial.println(" %RH");

    delay(2000);
}
