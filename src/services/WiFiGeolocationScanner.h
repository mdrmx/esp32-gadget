#pragma once

#include <stdint.h>
#include <string.h>

// A bounded, platform-neutral observation used by Google's Wi-Fi positioning
// request. BSSID, RSSI and channel are copied out of the platform scan buffer
// before the backend releases that buffer.
struct WiFiGeolocationAccessPoint {
  uint8_t bssid[6];
  int32_t signalStrengthDbm;
  int32_t channel;
};

class WiFiGeolocationScanBackend {
public:
  virtual ~WiFiGeolocationScanBackend() {}
  virtual int16_t scanNetworks() = 0;
  virtual bool readNetwork(
    int16_t index,
    WiFiGeolocationAccessPoint& output
  ) = 0;
  virtual void clearResults() = 0;
};

// Owns Google Wi-Fi positioning's access-point eligibility and selection
// policy. The ESP32 adapter owns only the platform scan and result access.
class WiFiGeolocationScanner {
public:
  enum { MaxAccessPoints = 15 };

  struct Result {
    int16_t networksFound;
    uint8_t accessPointCount;
    bool succeeded;
  };

  explicit WiFiGeolocationScanner(WiFiGeolocationScanBackend& backend)
      : backend_(backend) {}

  WiFiGeolocationScanner(const WiFiGeolocationScanner&) = delete;
  WiFiGeolocationScanner& operator=(const WiFiGeolocationScanner&) = delete;

  Result scan(
    WiFiGeolocationAccessPoint* output,
    uint8_t outputCapacity
  ) {
    Result result = {};
    if (output == nullptr || outputCapacity == 0) {
      return result;
    }

    memset(output, 0, sizeof(WiFiGeolocationAccessPoint) * outputCapacity);

    const int16_t scanCount = backend_.scanNetworks();
    result.networksFound = scanCount;
    if (scanCount < 0) {
      backend_.clearResults();
      return result;
    }

    uint8_t candidateCount = 0;
    const uint8_t capacity = outputCapacity < MaxAccessPoints
      ? outputCapacity
      : static_cast<uint8_t>(MaxAccessPoints);

    for (int16_t index = 0; index < scanCount; ++index) {
      WiFiGeolocationAccessPoint candidate = {};
      if (!backend_.readNetwork(index, candidate)) {
        continue;
      }
      if (!isEligible(candidate)) {
        continue;
      }
      if (containsBssid(output, candidateCount, candidate.bssid)) {
        continue;
      }
      insertStrongest(output, candidateCount, capacity, candidate);
    }

    // The caller-provided bounded buffer owns the selected records before the
    // ESP32 driver releases its scan result allocation.
    result.accessPointCount = candidateCount;
    result.succeeded = true;
    backend_.clearResults();
    return result;
  }

private:
  static bool isEligible(const WiFiGeolocationAccessPoint& candidate) {
    const uint8_t firstOctet = candidate.bssid[0];

    // Google's API requires globally administered unicast MAC addresses.
    if ((firstOctet & 0x03) != 0) {
      return false;
    }

    // Exclude the documented reserved 00:00:5E range.
    if (firstOctet == 0x00 &&
        candidate.bssid[1] == 0x00 &&
        candidate.bssid[2] == 0x5E) {
      return false;
    }

    bool allZero = true;
    for (uint8_t octet = 0; octet < 6; ++octet) {
      if (candidate.bssid[octet] != 0) {
        allZero = false;
        break;
      }
    }
    if (allZero) {
      return false;
    }

    return candidate.signalStrengthDbm >= -128 &&
           candidate.signalStrengthDbm <= -10;
  }

  static bool containsBssid(
    const WiFiGeolocationAccessPoint* candidates,
    uint8_t count,
    const uint8_t bssid[6]
  ) {
    for (uint8_t i = 0; i < count; ++i) {
      if (memcmp(candidates[i].bssid, bssid, 6) == 0) {
        return true;
      }
    }
    return false;
  }

  static void insertStrongest(
    WiFiGeolocationAccessPoint* candidates,
    uint8_t& count,
    uint8_t capacity,
    const WiFiGeolocationAccessPoint& candidate
  ) {
    uint8_t insertAt = 0;
    while (insertAt < count &&
           candidates[insertAt].signalStrengthDbm >=
             candidate.signalStrengthDbm) {
      ++insertAt;
    }

    if (count == capacity && insertAt >= capacity) {
      return;
    }

    if (count < capacity) {
      ++count;
    }

    for (uint8_t index = static_cast<uint8_t>(count - 1);
         index > insertAt; --index) {
      candidates[index] = candidates[index - 1];
    }
    candidates[insertAt] = candidate;
  }

  WiFiGeolocationScanBackend& backend_;
};
