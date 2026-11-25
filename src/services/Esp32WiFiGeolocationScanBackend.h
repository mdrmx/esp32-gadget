#pragma once

#include <WiFi.h>
#include <string.h>

#include "WiFiGeolocationScanner.h"

// ESP32 adapter for the synchronous passive scan used by Google positioning.
// It copies BSSID/RSSI/channel records into the neutral scanner's value type;
// selection and eligibility rules remain outside the Arduino Wi-Fi API layer.
class Esp32WiFiGeolocationScanBackend : public WiFiGeolocationScanBackend {
public:
  int16_t scanNetworks() override {
    // Passive scan with a bounded per-channel dwell, matching the existing
    // LocationService behaviour. This remains a synchronous startup stage.
    return WiFi.scanNetworks(false, false, true, 120);
  }

  bool readNetwork(
    int16_t index,
    WiFiGeolocationAccessPoint& output
  ) override {
    uint8_t* bssid = WiFi.BSSID(index);
    if (bssid == nullptr) {
      return false;
    }

    memset(&output, 0, sizeof(output));
    memcpy(output.bssid, bssid, sizeof(output.bssid));
    output.signalStrengthDbm = WiFi.RSSI(index);
    output.channel = WiFi.channel(index);
    return true;
  }

  void clearResults() override {
    WiFi.scanDelete();
  }
};
