#pragma once

#include "WiFiNetworkScanner.h"

#include <stdint.h>
#include <string.h>

// Retains the most recently completed scan's SSID set independently from the
// active scanner. Starting or failing a background refresh must not erase the
// availability badges already visible on the Saved Profiles screen.
class WiFiAvailabilitySnapshot {
public:
  static const uint8_t MaxResults = WiFiNetworkScanner::MaxResults;
  static const uint8_t MaxSsidLength = 32;

  WiFiAvailabilitySnapshot() : count_(0), valid_(false) {
    memset(ssids_, 0, sizeof(ssids_));
  }

  // Only a completed scan may replace the snapshot. Scanning/failed states
  // leave the last-known availability untouched.
  bool capture(const WiFiNetworkScanner& scanner) {
    if (scanner.state() != WiFiNetworkScanner::State::Ready) {
      return false;
    }

    char nextSsids[MaxResults][MaxSsidLength + 1] = {};
    uint8_t nextCount = 0;
    for (uint8_t i = 0; i < scanner.count(); ++i) {
      WiFiScanResult result = {};
      if (!scanner.getAt(i, result) || result.ssid[0] == '\0') {
        continue;
      }

      if (memchr(result.ssid, '\0', sizeof(result.ssid)) == nullptr) {
        continue;
      }

      memcpy(nextSsids[nextCount], result.ssid, sizeof(nextSsids[nextCount]));
      ++nextCount;
    }

    memcpy(ssids_, nextSsids, sizeof(ssids_));
    count_ = nextCount;
    valid_ = true;
    return true;
  }

  bool hasSnapshot() const {
    return valid_;
  }

  uint8_t count() const {
    return valid_ ? count_ : 0;
  }

  bool contains(const char* ssid) const {
    if (!valid_ || ssid == nullptr || ssid[0] == '\0') {
      return false;
    }

    for (uint8_t i = 0; i < count_; ++i) {
      if (strcmp(ssids_[i], ssid) == 0) {
        return true;
      }
    }
    return false;
  }

  void reset() {
    memset(ssids_, 0, sizeof(ssids_));
    count_ = 0;
    valid_ = false;
  }

private:
  char ssids_[MaxResults][MaxSsidLength + 1];
  uint8_t count_;
  bool valid_;
};
