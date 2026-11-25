#include "src/services/WiFiBackend.h"
#include <stdio.h>
#include <string.h>

WiFiClass WiFi;

namespace {
int failures = 0;
#define CHECK(condition) do { \
  if (!(condition)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
    ++failures; \
  } \
} while (0)

void configuresAndStartsStationWithSelectedProfile() {
  WiFiBackend backend;
  CHECK(backend.networkType() == NetworkType::WiFi);
  CHECK(!backend.configure(nullptr, "password"));
  CHECK(!backend.configure("", "password"));
  CHECK(!backend.begin());
  CHECK(WiFi.beginCalls == 0);

  CHECK(backend.configure("saved-network", "saved-password"));
  CHECK(backend.begin());
  CHECK(WiFi.modeCalls == 1);
  CHECK(WiFi.lastMode == WIFI_STA);
  CHECK(WiFi.beginCalls == 1);
  CHECK(strcmp(WiFi.lastSsid, "saved-network") == 0);
  CHECK(strcmp(WiFi.lastPassword, "saved-password") == 0);
}

void supportsOpenNetworks() {
  WiFiBackend backend;
  CHECK(backend.configure("open-network", nullptr));
  CHECK(backend.begin());
  CHECK(strcmp(WiFi.lastPassword, "") == 0);
}

void exposesConnectionAndSignalStatus() {
  WiFiBackend backend;
  CHECK(!backend.isConnected());
  CHECK(backend.signalStrengthDbm() == 0);

  WiFi.currentStatus = WL_CONNECTED;
  WiFi.currentRssi = -47;
  CHECK(backend.isConnected());
  CHECK(backend.signalStrengthDbm() == -47);

  backend.update();
  CHECK(backend.isConnected());
}

void disconnectDoesNotEraseProfileByContract() {
  WiFiBackend backend;
  WiFi.currentStatus = WL_CONNECTED;
  backend.disconnect();
  CHECK(WiFi.disconnectCalls == 1);
  CHECK(!backend.isConnected());
}
}

int main() {
  configuresAndStartsStationWithSelectedProfile();
  supportsOpenNetworks();
  exposesConnectionAndSignalStatus();
  disconnectDoesNotEraseProfileByContract();

  if (failures != 0) {
    fprintf(stderr, "%d Wi-Fi backend test(s) failed.\n", failures);
    return 1;
  }

  printf("WiFiBackend tests passed.\n");
  return 0;
}
