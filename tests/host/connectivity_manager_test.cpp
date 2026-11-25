#include "src/services/ConnectivityManager.h"
#include <stdio.h>

namespace {
int failures = 0;

#define CHECK(condition) do { \
  if (!(condition)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
    ++failures; \
  } \
} while (0)

class FakeBackend : public ConnectivityBackend {
public:
  FakeBackend()
      : type(NetworkType::WiFi),
        startResult(true),
        connected(false),
        beginCalls(0),
        updateCalls(0),
        disconnectCalls(0),
        signalDbm(-61) {}

  NetworkType networkType() const override { return type; }

  bool begin() override {
    ++beginCalls;
    return startResult;
  }

  void update() override { ++updateCalls; }
  bool isConnected() const override { return connected; }
  void disconnect() override { ++disconnectCalls; connected = false; }
  int32_t signalStrengthDbm() const override { return signalDbm; }

  NetworkType type;
  bool startResult;
  bool connected;
  int beginCalls;
  int updateCalls;
  int disconnectCalls;
  int32_t signalDbm;
};

void startsWithoutBlockingAndTransitionsWhenConnected() {
  FakeBackend backend;
  ConnectivityManager manager(backend, 20000UL);

  CHECK(manager.connect(100U));
  CHECK(manager.state() == ConnectivityManager::State::Connecting);
  CHECK(manager.isConnecting());
  CHECK(!manager.isConnected());
  CHECK(manager.activeNetworkType() == NetworkType::WiFi);
  CHECK(backend.beginCalls == 1);

  backend.connected = true;
  manager.update(600U);
  CHECK(manager.state() == ConnectivityManager::State::Connected);
  CHECK(manager.isConnected());
  CHECK(manager.hasSignalStrength());
  CHECK(manager.signalStrengthDbm() == -61);
  CHECK(backend.updateCalls == 1);
}

void reportsCellularBackendWithoutWiFiAssumptions() {
  FakeBackend backend;
  backend.type = NetworkType::Cellular;
  ConnectivityManager manager(backend);

  CHECK(manager.connect(10U));
  backend.connected = true;
  manager.update(20U);
  CHECK(manager.activeNetworkType() == NetworkType::Cellular);
  CHECK(manager.hasSignalStrength());
  CHECK(manager.signalStrengthDbm() == -61);
}

void startsWithBackendOwnedProfileConfiguration() {
  FakeBackend backend;
  ConnectivityManager manager(backend);

  CHECK(manager.connect(0U));
  CHECK(manager.state() == ConnectivityManager::State::Connecting);
}

void rejectsDuplicatePendingAttempt() {
  FakeBackend backend;
  ConnectivityManager manager(backend);

  CHECK(manager.connect(0U));
  CHECK(!manager.connect(1U));
  CHECK(backend.beginCalls == 1);
}

void reportsBackendStartFailure() {
  FakeBackend backend;
  backend.startResult = false;
  ConnectivityManager manager(backend);

  CHECK(!manager.connect(100U));
  CHECK(manager.state() == ConnectivityManager::State::Failed);
  CHECK(manager.activeNetworkType() == NetworkType::None);
}

void timesOutAndDisconnectsBackend() {
  FakeBackend backend;
  ConnectivityManager manager(backend, 500U);

  CHECK(manager.connect(1000U));
  manager.update(1499U);
  CHECK(manager.state() == ConnectivityManager::State::Connecting);
  manager.update(1500U);
  CHECK(manager.state() == ConnectivityManager::State::TimedOut);
  CHECK(backend.disconnectCalls == 1);
  CHECK(manager.activeNetworkType() == NetworkType::None);
}

void timeoutIsSafeAcrossMillisRollover() {
  FakeBackend backend;
  ConnectivityManager manager(backend, 100U);
  const uint32_t start = 0xFFFFFFF0UL;

  CHECK(manager.connect(start));
  manager.update(0x00000053UL); // 99 ms elapsed across rollover.
  CHECK(manager.state() == ConnectivityManager::State::Connecting);
  manager.update(0x00000054UL); // 100 ms elapsed across rollover.
  CHECK(manager.state() == ConnectivityManager::State::TimedOut);
}

void unexpectedDisconnectAndBackgroundReconnectAreTracked() {
  FakeBackend backend;
  ConnectivityManager manager(backend);
  CHECK(manager.connect(10U));
  backend.connected = true;
  manager.update(20U);
  CHECK(manager.isConnected());

  backend.connected = false;
  manager.update(30U);
  CHECK(manager.state() == ConnectivityManager::State::Disconnected);
  CHECK(!manager.isConnected());
  CHECK(manager.activeNetworkType() == NetworkType::None);

  // Simulate the Wi-Fi driver restoring the link without a new connect() call.
  backend.connected = true;
  manager.update(40U);
  CHECK(manager.state() == ConnectivityManager::State::Connected);
  CHECK(manager.isConnected());
  CHECK(manager.activeNetworkType() == NetworkType::WiFi);
}

void reconnectWorksAfterLossAndDisconnect() {
  FakeBackend backend;
  ConnectivityManager manager(backend);
  CHECK(manager.connect(0U));
  manager.disconnect();
  CHECK(manager.state() == ConnectivityManager::State::Disconnected);
  CHECK(backend.disconnectCalls == 1);
  CHECK(manager.connect(50U));
  CHECK(backend.beginCalls == 2);
}
}

int main() {
  startsWithoutBlockingAndTransitionsWhenConnected();
  startsWithBackendOwnedProfileConfiguration();
  reportsCellularBackendWithoutWiFiAssumptions();
  rejectsDuplicatePendingAttempt();
  reportsBackendStartFailure();
  timesOutAndDisconnectsBackend();
  timeoutIsSafeAcrossMillisRollover();
  unexpectedDisconnectAndBackgroundReconnectAreTracked();
  reconnectWorksAfterLossAndDisconnect();

  if (failures != 0) {
    fprintf(stderr, "%d connectivity manager test(s) failed.\n", failures);
    return 1;
  }

  printf("ConnectivityManager tests passed.\n");
  return 0;
}
