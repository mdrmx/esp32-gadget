#include "src/services/WiFiAvailabilitySnapshot.h"

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
        resultCount(0) {
    memset(results, 0, sizeof(results));
  }

  bool startScan() override {
    return startResult;
  }

  int16_t scanComplete() override {
    return completeResult;
  }

  bool readResult(uint16_t index, WiFiScanResult& output) override {
    if (index >= resultCount) return false;
    output = results[index];
    return true;
  }

  void clearResults() override {}

  void add(const char* ssid) {
    WiFiScanResult& result = results[resultCount++];
    memset(&result, 0, sizeof(result));
    snprintf(result.ssid, sizeof(result.ssid), "%s", ssid);
  }

  void clearRecords() {
    memset(results, 0, sizeof(results));
    resultCount = 0;
  }

  bool startResult;
  int16_t completeResult;
  uint16_t resultCount;
  WiFiScanResult results[8];
};

void startsWithoutACompletedSnapshot() {
  FakeBackend backend;
  WiFiNetworkScanner scanner(backend);
  WiFiAvailabilitySnapshot snapshot;

  CHECK(!snapshot.hasSnapshot());
  CHECK(!snapshot.contains("home"));
  CHECK(scanner.start());
  CHECK(!snapshot.capture(scanner));
  CHECK(!snapshot.hasSnapshot());
}

void keepsLastKnownAvailabilityWhileRefreshing() {
  FakeBackend backend;
  WiFiNetworkScanner scanner(backend);
  WiFiAvailabilitySnapshot snapshot;

  backend.add("home");
  backend.add("phone-hotspot");
  CHECK(scanner.start());
  backend.completeResult = 2;
  scanner.update();
  CHECK(snapshot.capture(scanner));
  CHECK(snapshot.hasSnapshot());
  CHECK(snapshot.count() == 2);
  CHECK(snapshot.contains("home"));
  CHECK(snapshot.contains("phone-hotspot"));

  // A refresh clears the live scanner buffer, but not the visible snapshot.
  CHECK(scanner.start());
  CHECK(scanner.state() == WiFiNetworkScanner::State::Scanning);
  CHECK(!snapshot.capture(scanner));
  CHECK(snapshot.contains("home"));
  CHECK(snapshot.contains("phone-hotspot"));

  // A failed scan also retains the last successful result set.
  backend.completeResult = WiFiNetworkScanner::ScanFailed;
  scanner.update();
  CHECK(scanner.state() == WiFiNetworkScanner::State::Failed);
  CHECK(!snapshot.capture(scanner));
  CHECK(snapshot.contains("home"));
  CHECK(snapshot.contains("phone-hotspot"));
}

void replacesSnapshotOnlyAfterSuccessfulCompletion() {
  FakeBackend backend;
  WiFiNetworkScanner scanner(backend);
  WiFiAvailabilitySnapshot snapshot;

  backend.add("home");
  backend.add("phone-hotspot");
  CHECK(scanner.start());
  backend.completeResult = 2;
  scanner.update();
  CHECK(snapshot.capture(scanner));

  CHECK(scanner.start());
  backend.clearRecords();
  backend.add("home");
  backend.completeResult = 1;
  scanner.update();
  CHECK(snapshot.capture(scanner));

  CHECK(snapshot.count() == 1);
  CHECK(snapshot.contains("home"));
  CHECK(!snapshot.contains("phone-hotspot"));
}

void emptySuccessfulScanIsAValidSnapshot() {
  FakeBackend backend;
  WiFiNetworkScanner scanner(backend);
  WiFiAvailabilitySnapshot snapshot;

  CHECK(scanner.start());
  backend.completeResult = 0;
  scanner.update();
  CHECK(snapshot.capture(scanner));
  CHECK(snapshot.hasSnapshot());
  CHECK(snapshot.count() == 0);
  CHECK(!snapshot.contains("home"));
}

void resetReturnsToUnknownUntilTheNextCompletedScan() {
  FakeBackend backend;
  WiFiNetworkScanner scanner(backend);
  WiFiAvailabilitySnapshot snapshot;

  backend.add("home");
  CHECK(scanner.start());
  backend.completeResult = 1;
  scanner.update();
  CHECK(snapshot.capture(scanner));
  CHECK(snapshot.contains("home"));

  snapshot.reset();
  CHECK(!snapshot.hasSnapshot());
  CHECK(!snapshot.contains("home"));
}
}

int main() {
  startsWithoutACompletedSnapshot();
  keepsLastKnownAvailabilityWhileRefreshing();
  replacesSnapshotOnlyAfterSuccessfulCompletion();
  emptySuccessfulScanIsAValidSnapshot();
  resetReturnsToUnknownUntilTheNextCompletedScan();

  if (failures != 0) {
    fprintf(stderr, "%d WiFiAvailabilitySnapshot test(s) failed.\n",
            failures);
    return 1;
  }

  printf("WiFiAvailabilitySnapshot tests passed.\n");
  return 0;
}
