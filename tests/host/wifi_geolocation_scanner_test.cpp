#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "src/services/WiFiGeolocationScanner.h"

namespace {

struct MockRecord {
  WiFiGeolocationAccessPoint accessPoint;
  bool readable;
};

class MockBackend : public WiFiGeolocationScanBackend {
public:
  MockBackend() : scanResult(0), recordCount(0), clearCalls(0), readCalls(0) {
    memset(records, 0, sizeof(records));
  }

  int16_t scanNetworks() override {
    return scanResult;
  }

  bool readNetwork(
    int16_t index,
    WiFiGeolocationAccessPoint& output
  ) override {
    ++readCalls;
    if (index < 0 || index >= recordCount || !records[index].readable) {
      return false;
    }
    output = records[index].accessPoint;
    return true;
  }

  void clearResults() override {
    ++clearCalls;
  }

  void add(
    const uint8_t bssid[6],
    int32_t rssi,
    int32_t channel,
    bool readable = true
  ) {
    assert(recordCount < static_cast<int16_t>(sizeof(records) / sizeof(records[0])));
    MockRecord& record = records[recordCount++];
    memcpy(record.accessPoint.bssid, bssid, 6);
    record.accessPoint.signalStrengthDbm = rssi;
    record.accessPoint.channel = channel;
    record.readable = readable;
  }

  int16_t scanResult;
  int16_t recordCount;
  int clearCalls;
  int readCalls;
  MockRecord records[40];
};

void expectMac(
  const WiFiGeolocationAccessPoint& accessPoint,
  const uint8_t expected[6]
) {
  assert(memcmp(accessPoint.bssid, expected, 6) == 0);
}

void testSuccessfulScanFiltersAndSorts() {
  MockBackend backend;
  const uint8_t weak[] = {0x10, 0x20, 0x30, 0, 0, 1};
  const uint8_t strongest[] = {0x10, 0x20, 0x30, 0, 0, 2};
  const uint8_t middle[] = {0x10, 0x20, 0x30, 0, 0, 3};
  const uint8_t multicast[] = {0x11, 0x20, 0x30, 0, 0, 4};
  const uint8_t local[] = {0x12, 0x20, 0x30, 0, 0, 5};
  const uint8_t reserved[] = {0x00, 0x00, 0x5E, 0, 0, 6};
  const uint8_t zero[] = {0, 0, 0, 0, 0, 0};
  const uint8_t tooWeak[] = {0x10, 0x20, 0x30, 0, 0, 7};
  const uint8_t tooStrong[] = {0x10, 0x20, 0x30, 0, 0, 8};
  const uint8_t duplicate[] = {0x10, 0x20, 0x30, 0, 0, 1};

  backend.add(weak, -88, 1);
  backend.add(strongest, -42, 6);
  backend.add(middle, -65, 11);
  backend.add(multicast, -30, 1);
  backend.add(local, -35, 1);
  backend.add(reserved, -45, 1);
  backend.add(zero, -50, 1);
  backend.add(tooWeak, -129, 1);
  backend.add(tooStrong, -9, 1);
  backend.add(duplicate, -20, 1); // First matching BSSID remains authoritative.
  backend.add(weak, -80, 1, false); // Unreadable result is ignored.
  backend.scanResult = backend.recordCount;

  WiFiGeolocationScanner scanner(backend);
  WiFiGeolocationAccessPoint output[WiFiGeolocationScanner::MaxAccessPoints] = {};
  const WiFiGeolocationScanner::Result result =
    scanner.scan(output, WiFiGeolocationScanner::MaxAccessPoints);

  assert(result.succeeded);
  assert(result.networksFound == backend.recordCount);
  assert(result.accessPointCount == 3);
  expectMac(output[0], strongest);
  expectMac(output[1], middle);
  expectMac(output[2], weak);
  assert(output[0].signalStrengthDbm == -42);
  assert(output[1].signalStrengthDbm == -65);
  assert(output[2].signalStrengthDbm == -88);
  assert(output[0].channel == 6);
  assert(backend.clearCalls == 1);
}

void testKeepsStrongestFifteen() {
  MockBackend backend;
  uint8_t mac[6] = {0x10, 0x20, 0x30, 0, 0, 0};
  for (uint8_t i = 0; i < 20; ++i) {
    mac[5] = static_cast<uint8_t>(i + 1);
    backend.add(mac, -100 + static_cast<int32_t>(i) * 3, 1);
  }
  backend.scanResult = backend.recordCount;

  WiFiGeolocationScanner scanner(backend);
  WiFiGeolocationAccessPoint output[WiFiGeolocationScanner::MaxAccessPoints] = {};
  const WiFiGeolocationScanner::Result result =
    scanner.scan(output, WiFiGeolocationScanner::MaxAccessPoints);

  assert(result.succeeded);
  assert(result.accessPointCount == WiFiGeolocationScanner::MaxAccessPoints);
  assert(output[0].signalStrengthDbm == -43);
  assert(output[14].signalStrengthDbm == -85);
}

void testScanFailureClearsPlatformResults() {
  MockBackend backend;
  backend.scanResult = -2;
  WiFiGeolocationScanner scanner(backend);
  WiFiGeolocationAccessPoint output[WiFiGeolocationScanner::MaxAccessPoints] = {};
  output[0].signalStrengthDbm = -40;

  const WiFiGeolocationScanner::Result result =
    scanner.scan(output, WiFiGeolocationScanner::MaxAccessPoints);

  assert(!result.succeeded);
  assert(result.networksFound == -2);
  assert(result.accessPointCount == 0);
  assert(backend.clearCalls == 1);
  assert(backend.readCalls == 0);
  assert(output[0].signalStrengthDbm == 0);
}

void testSuccessfulEmptyScan() {
  MockBackend backend;
  backend.scanResult = 0;
  WiFiGeolocationScanner scanner(backend);
  WiFiGeolocationAccessPoint output[WiFiGeolocationScanner::MaxAccessPoints] = {};

  const WiFiGeolocationScanner::Result result =
    scanner.scan(output, WiFiGeolocationScanner::MaxAccessPoints);

  assert(result.succeeded);
  assert(result.networksFound == 0);
  assert(result.accessPointCount == 0);
  assert(backend.clearCalls == 1);
}

void testOutputCapacityAndInvalidBuffer() {
  MockBackend backend;
  const uint8_t first[] = {0x10, 0x20, 0x30, 0, 0, 1};
  const uint8_t second[] = {0x10, 0x20, 0x30, 0, 0, 2};
  const uint8_t third[] = {0x10, 0x20, 0x30, 0, 0, 3};
  backend.add(first, -75, 1);
  backend.add(second, -50, 6);
  backend.add(third, -65, 11);
  backend.scanResult = backend.recordCount;

  WiFiGeolocationScanner scanner(backend);
  WiFiGeolocationAccessPoint output[2] = {};
  const WiFiGeolocationScanner::Result result = scanner.scan(output, 2);
  assert(result.succeeded);
  assert(result.accessPointCount == 2);
  expectMac(output[0], second);
  expectMac(output[1], third);
  assert(backend.clearCalls == 1);

  const WiFiGeolocationScanner::Result invalid = scanner.scan(nullptr, 2);
  assert(!invalid.succeeded);
  assert(backend.clearCalls == 1);
}

} // namespace

int main() {
  testSuccessfulScanFiltersAndSorts();
  testKeepsStrongestFifteen();
  testScanFailureClearsPlatformResults();
  testSuccessfulEmptyScan();
  testOutputCapacityAndInvalidBuffer();
  puts("WiFiGeolocationScanner tests passed.");
  return 0;
}
