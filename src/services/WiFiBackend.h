#pragma once

#include "ConnectivityBackend.h"
#include <WiFi.h>

// ESP32 Arduino Wi-Fi station backend. The selected profile is configured by
// the composition root; association itself is started asynchronously by the
// driver and observed through ConnectivityManager::update().
class WiFiBackend : public ConnectivityBackend {
public:
  WiFiBackend() : ssid_(nullptr), password_(nullptr) {}

  // The supplied strings are borrowed, not copied. Keep them alive for as long
  // as this backend may start/restart the selected profile. The current sketch
  // uses static secrets.h-backed strings; a future NVS profile store will own
  // and supply its selected profile lifetime.
  bool configure(const char* ssid, const char* password) {
    if (ssid == nullptr || ssid[0] == '\0') {
      ssid_ = nullptr;
      password_ = nullptr;
      return false;
    }

    ssid_ = ssid;
    password_ = password != nullptr ? password : "";
    return true;
  }

  NetworkType networkType() const override {
    return NetworkType::WiFi;
  }

  bool begin() override {
    if (ssid_ == nullptr || ssid_[0] == '\0') {
      return false;
    }

    WiFi.mode(WIFI_STA);
    // Do not interpret begin()'s immediate status as the final connection
    // result; the station associates asynchronously and is polled separately.
    WiFi.begin(ssid_, password_ != nullptr ? password_ : "");
    return true;
  }

  void update() override {
    // ESP32 Wi-Fi runs in the system driver. status() is sampled by isConnected.
  }

  bool isConnected() const override {
    return WiFi.status() == WL_CONNECTED;
  }

  void disconnect() override {
    // Retain the driver's saved credentials; profile persistence is a later
    // Phase 2 slice owned by the settings/profile store.
    WiFi.disconnect();
  }

  int32_t signalStrengthDbm() const override {
    return isConnected() ? (int32_t)WiFi.RSSI() : 0;
  }

private:
  const char* ssid_;
  const char* password_;
};
