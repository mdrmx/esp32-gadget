#pragma once

#include <WiFi.h>
#include <string.h>

#include "WiFiNetworkScanner.h"

// ESP32 Arduino adapter. Scanning is asynchronous and leaves the existing
// station association in place; WiFiNetworkScanner copies the results before
// asking the driver to release its scan buffer.
class Esp32WiFiScanBackend : public WiFiScanBackend {
public:
  bool startScan() override {
    WiFi.mode(WIFI_STA);
    // The scan runs across the 2.4 GHz channels. A 120 ms channel window keeps
    // the discovery period bounded without blocking the application loop.
    const int16_t result = WiFi.scanNetworks(true, false, false, 120);
    return result >= WiFiNetworkScanner::ScanRunning;
  }

  int16_t scanComplete() override {
    return WiFi.scanComplete();
  }

  bool readResult(uint16_t index, WiFiScanResult& output) override {
    const String ssid = WiFi.SSID(index);
    if (ssid.length() == 0 || ssid.length() > 32) {
      return false;
    }

    memset(&output, 0, sizeof(output));
    ssid.toCharArray(output.ssid, sizeof(output.ssid));
    output.rssiDbm = WiFi.RSSI(index);

    const uint8_t auth =
        static_cast<uint8_t>(WiFi.encryptionType(index));
    output.open = auth == WIFI_AUTH_OPEN;
    output.supported =
        auth == WIFI_AUTH_OPEN ||
        auth == WIFI_AUTH_WPA_PSK ||
        auth == WIFI_AUTH_WPA2_PSK ||
        auth == WIFI_AUTH_WPA_WPA2_PSK ||
        auth == WIFI_AUTH_WPA3_PSK ||
        auth == WIFI_AUTH_WPA2_WPA3_PSK;
    return output.ssid[0] != '\0';
  }

  void clearResults() override {
    WiFi.scanDelete();
  }
};
