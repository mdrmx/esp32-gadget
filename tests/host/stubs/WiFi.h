#pragma once

#include <stdint.h>
#include <stdio.h>

static const int WIFI_STA = 1;
static const int WL_CONNECTED = 3;
static const int WL_DISCONNECTED = 6;

typedef int wl_status_t;

class WiFiClass {
public:
  WiFiClass()
      : currentStatus(WL_DISCONNECTED),
        currentRssi(-55),
        modeCalls(0),
        beginCalls(0),
        disconnectCalls(0),
        lastMode(0),
        lastPasswordWasNull(false),
        lastSsid(),
        lastPassword() {}

  bool mode(int value) {
    ++modeCalls;
    lastMode = value;
    return true;
  }

  wl_status_t begin(const char* ssid, const char* password) {
    ++beginCalls;
    lastPasswordWasNull = password == nullptr;
    snprintf(lastSsid, sizeof(lastSsid), "%s", ssid != nullptr ? ssid : "");
    snprintf(lastPassword, sizeof(lastPassword), "%s", password != nullptr ? password : "");
    return currentStatus;
  }

  wl_status_t status() const { return currentStatus; }
  int32_t RSSI() const { return currentRssi; }

  bool disconnect() {
    ++disconnectCalls;
    currentStatus = WL_DISCONNECTED;
    return true;
  }

  wl_status_t currentStatus;
  int32_t currentRssi;
  int modeCalls;
  int beginCalls;
  int disconnectCalls;
  int lastMode;
  bool lastPasswordWasNull;
  char lastSsid[40];
  char lastPassword[80];
};

extern WiFiClass WiFi;
