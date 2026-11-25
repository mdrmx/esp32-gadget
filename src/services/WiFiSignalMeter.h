#pragma once

#include <stdint.h>

// Maintains a short rolling average of RSSI samples and maps the averaged
// dBm value to four display levels. Sampling cadence is owned by the caller.
class WiFiSignalMeter {
public:
  static const uint8_t WindowSize = 5;

  WiFiSignalMeter() {
    reset();
  }

  bool addSample(int32_t rssiDbm) {
    // ESP32 station RSSI is negative dBm; reject disconnected/invalid values.
    if (rssiDbm >= 0 || rssiDbm <= -127) return false;

    if (sampleCount_ < WindowSize) {
      samples_[nextIndex_] = static_cast<int16_t>(rssiDbm);
      sum_ += rssiDbm;
      ++sampleCount_;
    } else {
      sum_ -= samples_[nextIndex_];
      samples_[nextIndex_] = static_cast<int16_t>(rssiDbm);
      sum_ += rssiDbm;
    }

    nextIndex_ = static_cast<uint8_t>((nextIndex_ + 1) % WindowSize);
    return true;
  }

  void reset() {
    for (uint8_t i = 0; i < WindowSize; ++i) samples_[i] = 0;
    nextIndex_ = 0;
    sampleCount_ = 0;
    sum_ = 0;
  }

  bool hasSamples() const {
    return sampleCount_ > 0;
  }

  uint8_t sampleCount() const {
    return sampleCount_;
  }

  int16_t averageRssiDbm() const {
    if (sampleCount_ == 0) return -127;
    return static_cast<int16_t>(sum_ / sampleCount_);
  }

  uint8_t level() const {
    if (!hasSamples()) return 0;
    const int16_t average = averageRssiDbm();
    if (average >= -55) return 4;
    if (average >= -67) return 3;
    if (average >= -75) return 2;
    if (average >= -85) return 1;
    return 0;
  }

private:
  int16_t samples_[WindowSize];
  uint8_t nextIndex_;
  uint8_t sampleCount_;
  int32_t sum_;
};
