#include <Arduino.h>
#include <Wire.h>

namespace reg {
    constexpr uint8_t CHIP_ID   = 0xD0;
    constexpr uint8_t CTRL_HUM  = 0xF2;
    constexpr uint8_t CTRL_MEAS = 0xF4;
    constexpr uint8_t TEMP_MSB  = 0xFA;
    constexpr uint8_t CAL_T1    = 0x88;
}

constexpr uint8_t kAddr   = 0x76;
constexpr int     kSdaPin = 21;
constexpr int     kSclPin = 22;

uint16_t dig_T1;
int16_t  dig_T2, dig_T3;
int32_t  tFine;

uint16_t read16LE(uint8_t reg) {
    Wire.beginTransmission(kAddr);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom(kAddr, (uint8_t)2);
    uint8_t lo = Wire.read();
    uint8_t hi = Wire.read();
    return (uint16_t)(hi << 8 | lo);
}

bool readBurst(uint8_t reg, uint8_t* buf, uint8_t len) {
    Wire.beginTransmission(kAddr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(kAddr, len) != len) return false;
    for (uint8_t i = 0; i < len; ++i) buf[i] = Wire.read();
    return true;
}

int32_t compensateT(int32_t adcT) {
    int32_t v1 = ((((adcT >> 3) - ((int32_t)dig_T1 << 1))) *
                  (int32_t)dig_T2) >> 11;
    int32_t v2 = (((((adcT >> 4) - (int32_t)dig_T1) *
                    ((adcT >> 4) - (int32_t)dig_T1)) >> 12) *
                  (int32_t)dig_T3) >> 14;
    tFine = v1 + v2;
    return (tFine * 5 + 128) >> 8;
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println();
    Serial.println("=== BME280 temperature ===");

    Wire.begin(kSdaPin, kSclPin);
    Wire.setClock(100000);
    delay(100);

    uint8_t id = 0;
    if (!readBurst(reg::CHIP_ID, &id, 1)) {
        Serial.println("FATAL: no response from sensor");
        while (true) delay(1000);
    }
    if (id != 0x60) {
        Serial.print("FATAL: unexpected chip id 0x");
        Serial.println(id, HEX);
        while (true) delay(1000);
    }

    dig_T1 = read16LE(reg::CAL_T1);
    dig_T2 = (int16_t)read16LE(reg::CAL_T1 + 2);
    dig_T3 = (int16_t)read16LE(reg::CAL_T1 + 4);

    Serial.print("calibration  T1=");
    Serial.print(dig_T1);
    Serial.print("  T2=");
    Serial.print(dig_T2);
    Serial.print("  T3=");
    Serial.println(dig_T3);

    Wire.beginTransmission(kAddr);
    Wire.write(reg::CTRL_HUM);
    Wire.write(0x01);
    Wire.endTransmission();

    Wire.beginTransmission(kAddr);
    Wire.write(reg::CTRL_MEAS);
    Wire.write(0x27);
    Wire.endTransmission();

    delay(100);
    Serial.println("ready");
}

void loop() {
    uint8_t d[3];
    if (!readBurst(reg::TEMP_MSB, d, 3)) {
        Serial.println("read failed");
        delay(2000);
        return;
    }

    int32_t adcT = ((int32_t)d[0] << 12) |
                   ((int32_t)d[1] << 4)  |
                   (d[2] >> 4);

    int32_t t = compensateT(adcT);

    float tempC = t / 100.0f;
    float tempF = tempC * 1.8f + 32.0f;

    Serial.print("Temp: ");
    Serial.print(tempC);
    Serial.print(" C  /  ");
    Serial.print(tempF);
    Serial.println(" F");

    delay(2000);
}