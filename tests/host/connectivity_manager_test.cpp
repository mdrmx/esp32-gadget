#include "src/services/ConnectivityManager.h"
#include <stdio.h>

namespace {
int failures = 0;

#define CHECK(condition) do { \
  if (!(condition)) { \
    fprintf(stderr, "FAIL %s:%d: %s\\n", __FILE__, __LINE__, #condition); \
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
  ConnectivityManager manager(backend, 20000UL, 0);

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
  CHECK(manager.retryAttempts() == 0);
  CHECK(backend.updateCalls == 1);
}

void reportsCellularBackendWithoutWiFiAssumptions() {
  FakeBackend backend;
  backend.type = NetworkType::Cellular;
  ConnectivityManager manager(backend, 20000UL, 0);

  CHECK(manager.connect(10U));
  backend.connected = true;
  manager.update(20U);
  CHECK(manager.activeNetworkType() == NetworkType::Cellular);
  CHECK(manager.hasSignalStrength());
  CHECK(manager.signalStrengthDbm() == -61);
}

void startsWithBackendOwnedProfileConfiguration() {
  FakeBackend backend;
  ConnectivityManager manager(backend, 20000UL, 0);

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

void reportsBackendStartFailureWhenRetriesAreDisabled() {
  FakeBackend backend;
  backend.startResult = false;
  ConnectivityManager manager(backend, 20000UL, 0);

  CHECK(!manager.connect(100U));
  CHECK(manager.state() == ConnectivityManager::State::Failed);
  CHECK(!manager.isConnecting());
  CHECK(manager.activeNetworkType() == NetworkType::None);
  CHECK(backend.beginCalls == 1);
}

void retriesBackendStartFailureWithExponentialBackoff() {
  FakeBackend backend;
  backend.startResult = false;
  ConnectivityManager manager(backend, 100U, 2, 10U, 40U);

  CHECK(manager.connect(100U));
  CHECK(manager.state() == ConnectivityManager::State::RetryWaiting);
  CHECK(manager.isConnecting());
  CHECK(manager.isRetryPending());
  CHECK(manager.retryAttempts() == 1);
  CHECK(manager.nextRetryDelayMs() == 10U);
  CHECK(backend.beginCalls == 1);

  manager.update(109U);
  CHECK(backend.beginCalls == 1);
  CHECK(manager.state() == ConnectivityManager::State::RetryWaiting);

  manager.update(110U);
  CHECK(backend.beginCalls == 2);
  CHECK(manager.state() == ConnectivityManager::State::RetryWaiting);
  CHECK(manager.retryAttempts() == 2);
  CHECK(manager.nextRetryDelayMs() == 20U);

  manager.update(129U);
  CHECK(backend.beginCalls == 2);
  manager.update(130U);
  CHECK(backend.beginCalls == 3);
  CHECK(manager.state() == ConnectivityManager::State::Failed);
  CHECK(!manager.isConnecting());
  CHECK(manager.retryAttempts() == 2);

  manager.update(1000U);
  CHECK(backend.beginCalls == 3);
}

void retriesTimeoutsAndStopsAfterBudget() {
  FakeBackend backend;
  ConnectivityManager manager(backend, 100U, 1, 10U, 20U);

  CHECK(manager.connect(0U));
  manager.update(99U);
  CHECK(manager.state() == ConnectivityManager::State::Connecting);

  manager.update(100U);
  CHECK(manager.state() == ConnectivityManager::State::RetryWaiting);
  CHECK(manager.retryAttempts() == 1);
  CHECK(manager.nextRetryDelayMs() == 10U);
  CHECK(backend.disconnectCalls == 1);

  manager.update(109U);
  CHECK(backend.beginCalls == 1);
  manager.update(110U);
  CHECK(backend.beginCalls == 2);
  CHECK(manager.state() == ConnectivityManager::State::Connecting);

  manager.update(210U);
  CHECK(manager.state() == ConnectivityManager::State::TimedOut);
  CHECK(backend.disconnectCalls == 2);
  CHECK(!manager.isConnecting());

  manager.update(1000U);
  CHECK(backend.beginCalls == 2);
}

void retryDeadlineIsSafeAcrossMillisRolloverAndBackoffIsCapped() {
  FakeBackend backend;
  backend.startResult = false;
  ConnectivityManager manager(backend, 100U, 3, 10U, 15U);
  const uint32_t start = 0xFFFFFFF0UL;

  CHECK(manager.connect(start));
  CHECK(manager.nextRetryDelayMs() == 10U);
  manager.update(0xFFFFFFF9UL);
  CHECK(backend.beginCalls == 1);
  manager.update(0xFFFFFFFAUL);
  CHECK(backend.beginCalls == 2);
  CHECK(manager.nextRetryDelayMs() == 15U);

  manager.update(0x00000008UL);
  CHECK(backend.beginCalls == 2);
  manager.update(0x00000009UL);
  CHECK(backend.beginCalls == 3);
  CHECK(manager.nextRetryDelayMs() == 15U);
}

void linkLossStartsRecoveryAndDriverReconnectCancelsRetry() {
  FakeBackend backend;
  ConnectivityManager manager(backend, 20000UL, 2, 100U, 400U);

  CHECK(manager.connect(10U));
  backend.connected = true;
  manager.update(20U);
  CHECK(manager.isConnected());

  backend.connected = false;
  manager.update(30U);
  CHECK(manager.state() == ConnectivityManager::State::RetryWaiting);
  CHECK(manager.retryAttempts() == 1);
  CHECK(manager.nextRetryDelayMs() == 100U);

  // The driver may restore the link before the manager's retry deadline.
  backend.connected = true;
  manager.update(50U);
  CHECK(manager.state() == ConnectivityManager::State::Connected);
  CHECK(manager.retryAttempts() == 0);
  CHECK(manager.nextRetryDelayMs() == 0U);
  CHECK(backend.beginCalls == 1);
}

void linkLossStartsManagerOwnedRetryWhenDriverDoesNotRecover() {
  FakeBackend backend;
  ConnectivityManager manager(backend, 20000UL, 1, 100U, 200U);

  CHECK(manager.connect(0U));
  backend.connected = true;
  manager.update(1U);
  backend.connected = false;
  manager.update(2U);
  CHECK(manager.state() == ConnectivityManager::State::RetryWaiting);

  manager.update(101U);
  CHECK(backend.beginCalls == 1);
  manager.update(102U);
  CHECK(backend.beginCalls == 2);
  CHECK(manager.state() == ConnectivityManager::State::Connecting);
}

void manualDisconnectCancelsPendingRetry() {
  FakeBackend backend;
  backend.startResult = false;
  ConnectivityManager manager(backend, 100U, 2, 10U, 20U);

  CHECK(manager.connect(0U));
  CHECK(manager.isRetryPending());
  manager.disconnect();
  CHECK(manager.state() == ConnectivityManager::State::Disconnected);
  CHECK(!manager.isConnecting());
  CHECK(manager.retryAttempts() == 0);

  manager.update(1000U);
  CHECK(backend.beginCalls == 1);
}

void explicitConnectCanStartANewRecoverySequenceAfterFailure() {
  FakeBackend backend;
  backend.startResult = false;
  ConnectivityManager manager(backend, 100U, 0);

  CHECK(!manager.connect(0U));
  CHECK(manager.state() == ConnectivityManager::State::Failed);

  backend.startResult = true;
  CHECK(manager.connect(200U));
  CHECK(manager.state() == ConnectivityManager::State::Connecting);
  CHECK(backend.beginCalls == 2);
}

void reconnectWorksAfterDisconnect() {
  FakeBackend backend;
  ConnectivityManager manager(backend, 20000UL, 0);
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
  reportsCellularBackendWithoutWiFiAssumptions();
  startsWithBackendOwnedProfileConfiguration();
  rejectsDuplicatePendingAttempt();
  reportsBackendStartFailureWhenRetriesAreDisabled();
  retriesBackendStartFailureWithExponentialBackoff();
  retriesTimeoutsAndStopsAfterBudget();
  retryDeadlineIsSafeAcrossMillisRolloverAndBackoffIsCapped();
  linkLossStartsRecoveryAndDriverReconnectCancelsRetry();
  linkLossStartsManagerOwnedRetryWhenDriverDoesNotRecover();
  manualDisconnectCancelsPendingRetry();
  explicitConnectCanStartANewRecoverySequenceAfterFailure();
  reconnectWorksAfterDisconnect();

  if (failures != 0) {
    fprintf(stderr, "%d connectivity manager test(s) failed.\\n", failures);
    return 1;
  }

  printf("ConnectivityManager tests passed.\\n");
  return 0;
}
