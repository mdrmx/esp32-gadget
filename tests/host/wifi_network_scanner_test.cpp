#include "src/services/WiFiNetworkScanner.h"

#include <stdio.h>
#include <string.h>

namespace {
int failures = 0;
#define CHECK(condition) do { \
  if (!(condition)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
    ++failures; \
  } \
} while (0)

class FakeBackend : public WiFiScanBackend {
public:
  FakeBackend()
      : startResult(true), completeResult(WiFiNetworkScanner::ScanRunning),
        startCalls(0), completeCalls(0), clearCalls(0), resultCount(0) {
    memset(results, 0, sizeof(results));
  }

  bool startScan() override {
    ++startCalls;
    return startResult;
  }

  int16_t scanComplete() override {
    ++completeCalls;
    return completeResult;
  }

  bool readResult(uint16_t index, WiFiScanResult& output) override {
    if (index >= resultCount) return false;
    output = results[index];
    return true;
  }

  void clearResults() override {
    ++clearCalls;
  }

  void add(const char* ssid, int32_t rssi, bool open, bool supported) {
    WiFiScanResult& result = results[resultCount++];
    memset(&result, 0, sizeof(result));
    snprintf(result.ssid, sizeof(result.ssid), "%s", ssid);
    result.rssiDbm = rssi;
    result.open = open;
    result.supported = supported;
  }

  bool startResult;
  int16_t completeResult;
  int startCalls;
  int completeCalls;
  int clearCalls;
  uint16_t resultCount;
  WiFiScanResult results[40];
};

void remainsNonBlockingUntilDriverCompletes() {
  FakeBackend backend;
  WiFiNetworkScanner scanner(backend);

  CHECK(scanner.state() == WiFiNetworkScanner::State::Idle);
  CHECK(scanner.start());
  CHECK(scanner.state() == WiFiNetworkScanner::State::Scanning);
  CHECK(backend.startCalls == 1);
  CHECK(!scanner.start());

  scanner.update();
  CHECK(scanner.state() == WiFiNetworkScanner::State::Scanning);
  CHECK(backend.completeCalls == 1);
  CHECK(scanner.count() == 0);

  backend.completeResult = 0;
  scanner.update();
  CHECK(scanner.state() == WiFiNetworkScanner::State::Ready);
  CHECK(scanner.count() == 0);
  CHECK(backend.clearCalls == 2);
}

void sortsAndDeduplicatesVisibleNetworks() {
  FakeBackend backend;
  backend.add("weak", -88, false, true);
  backend.add("shared", -74, false, true);
  backend.add("open", -52, true, true);
  backend.add("shared", -39, false, true);
  backend.add("", -20, false, true);

  WiFiNetworkScanner scanner(backend);
  CHECK(scanner.start());
  backend.completeResult = 5;
  scanner.update();

  CHECK(scanner.state() == WiFiNetworkScanner::State::Ready);
  CHECK(scanner.count() == 3);

  WiFiScanResult result = {};
  CHECK(scanner.getAt(0, result));
  CHECK(strcmp(result.ssid, "shared") == 0);
  CHECK(result.rssiDbm == -39);
  CHECK(!result.open);
  CHECK(scanner.getAt(1, result));
  CHECK(strcmp(result.ssid, "open") == 0);
  CHECK(result.rssiDbm == -52);
  CHECK(result.open);
  CHECK(scanner.getAt(2, result));
  CHECK(strcmp(result.ssid, "weak") == 0);
  CHECK(!scanner.getAt(3, result));
}

void capsResultsAtConfiguredMaximum() {
  FakeBackend backend;
  for (int i = 0; i < 30; ++i) {
    char name[16];
    snprintf(name, sizeof(name), "network-%02d", i);
    backend.add(name, -90 + i, false, true);
  }

  WiFiNetworkScanner scanner(backend);
  CHECK(scanner.start());
  backend.completeResult = 30;
  scanner.update();

  CHECK(scanner.count() == WiFiNetworkScanner::MaxResults);
  WiFiScanResult first = {};
  WiFiScanResult last = {};
  CHECK(scanner.getAt(0, first));
  CHECK(scanner.getAt(WiFiNetworkScanner::MaxResults - 1, last));
  CHECK(first.rssiDbm == -61);
  CHECK(last.rssiDbm == -84);
}

void reportsDriverFailureAndAllowsRetry() {
  FakeBackend backend;
  WiFiNetworkScanner scanner(backend);

  backend.startResult = false;
  CHECK(!scanner.start());
  CHECK(scanner.state() == WiFiNetworkScanner::State::Failed);

  backend.startResult = true;
  CHECK(scanner.start());
  backend.completeResult = WiFiNetworkScanner::ScanFailed;
  scanner.update();
  CHECK(scanner.state() == WiFiNetworkScanner::State::Failed);

  backend.completeResult = 0;
  CHECK(scanner.start());
  scanner.update();
  CHECK(scanner.state() == WiFiNetworkScanner::State::Ready);
}

void resetClearsCompletedResults() {
  FakeBackend backend;
  backend.add("saved", -44, false, true);
  WiFiNetworkScanner scanner(backend);
  CHECK(scanner.start());
  backend.completeResult = 1;
  scanner.update();
  CHECK(scanner.count() == 1);

  scanner.reset();
  CHECK(scanner.state() == WiFiNetworkScanner::State::Idle);
  CHECK(scanner.count() == 0);
}
}

int main() {
  remainsNonBlockingUntilDriverCompletes();
  sortsAndDeduplicatesVisibleNetworks();
  capsResultsAtConfiguredMaximum();
  reportsDriverFailureAndAllowsRetry();
  resetClearsCompletedResults();

  if (failures != 0) {
    fprintf(stderr, "%d WiFiNetworkScanner test(s) failed.\n", failures);
    return 1;
  }

  printf("WiFiNetworkScanner tests passed.\n");
  return 0;
}
