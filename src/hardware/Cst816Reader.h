#pragma once

#include <Wire.h>

#include "../input/TouchSample.h"

// Owns CST816 I2C setup, presence detection and raw register sampling.
// Gesture interpretation and view-specific touch ownership remain in Weather.
class Cst816Reader {
public:
  explicit Cst816Reader(TwoWire& wire = Wire)
    : wire_(wire) {}

  bool begin(bool initBus = true) {
    available_ = false;

    if (initBus) {
      wire_.begin(TOUCH_SDA_PIN, TOUCH_SCL_PIN);
      wire_.setClock(I2C_CLOCK_HZ);
    }

    wire_.beginTransmission(TOUCH_ADDRESS);
    available_ = wire_.endTransmission() == 0;
    return available_;
  }

  bool available() const {
    return available_;
  }

  bool readSample(TouchSample& sample) {
    if (!available_) return false;

    wire_.beginTransmission(TOUCH_ADDRESS);
    wire_.write(REG_GESTURE);
    if (wire_.endTransmission(false) != 0) return false;

    uint8_t data[SAMPLE_SIZE];
    const uint8_t sampleSize = sizeof(data);
    const size_t received = wire_.requestFrom(
      (uint8_t)TOUCH_ADDRESS,
      sampleSize);
    if (received != sampleSize) {
      while (wire_.available()) wire_.read();
      return false;
    }

    for (uint8_t i = 0; i < sampleSize; i++) {
      data[i] = (uint8_t)wire_.read();
    }

    sample.gesture = data[0];
    sample.fingerCount = data[1] & 0x0F;
    sample.event = (data[2] >> 6) & 0x03; // 0=down, 1=up, 2=contact
    sample.rawX = ((uint16_t)(data[2] & 0x0F) << 8) | data[3];
    sample.rawY = ((uint16_t)(data[4] & 0x0F) << 8) | data[5];
    return true;
  }

private:
  enum {
    TOUCH_SDA_PIN = 48,
    TOUCH_SCL_PIN = 47,
    I2C_CLOCK_HZ = 100000,
    TOUCH_ADDRESS = 0x15,
    REG_GESTURE = 0x01,
    SAMPLE_SIZE = 6
  };

  TwoWire& wire_;
  bool available_ = false;
};
