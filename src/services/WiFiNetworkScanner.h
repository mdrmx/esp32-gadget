#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Bounded network discovery contract shared by the ESP32 adapter and host
// tests. The backend starts and polls the platform scan; this class owns the
// result lifetime, ordering and duplicate-SSID policy.
struct WiFiScanResult {
  char ssid[33];
  int32_t rssiDbm;
  bool open;
  bool supported;
};

class WiFiScanBackend {
public:
  virtual ~WiFiScanBackend() {}
  virtual bool startScan() = 0;
  // -1 means running, -2 means failed, >= 0 is the number of AP records.
  virtual int16_t scanComplete() = 0;
  virtual bool readResult(uint16_t index, WiFiScanResult& output) = 0;
  virtual void clearResults() = 0;
};

class WiFiNetworkScanner {
public:
  static const uint8_t MaxResults = 24;
  static const int16_t ScanRunning = -1;
  static const int16_t ScanFailed = -2;

  enum class State : uint8_t {
    Idle = 0,
    Scanning,
    Ready,
    Failed
  };

  explicit WiFiNetworkScanner(WiFiScanBackend& backend)
      : backend_(backend), state_(State::Idle), count_(0) {
    memset(results_, 0, sizeof(results_));
  }

  WiFiNetworkScanner(const WiFiNetworkScanner&) = delete;
  WiFiNetworkScanner& operator=(const WiFiNetworkScanner&) = delete;

  bool start() {
    if (state_ == State::Scanning) {
      return false;
    }

    backend_.clearResults();
    clearResults();
    state_ = State::Idle;

    if (!backend_.startScan()) {
      state_ = State::Failed;
      return false;
    }

    state_ = State::Scanning;
    return true;
  }

  // Poll from loop(); this never waits for a scan to finish.
  void update() {
    if (state_ != State::Scanning) {
      return;
    }

    const int16_t resultCount = backend_.scanComplete();
    if (resultCount == ScanRunning) {
      return;
    }

    if (resultCount < 0) {
      backend_.clearResults();
      state_ = State::Failed;
      return;
    }

    for (uint16_t i = 0; i < static_cast<uint16_t>(resultCount); ++i) {
      WiFiScanResult candidate = {};
      if (!backend_.readResult(i, candidate)) {
        continue;
      }

      if (memchr(candidate.ssid, '\0', sizeof(candidate.ssid)) == nullptr ||
          candidate.ssid[0] == '\0') {
        continue;
      }

      insert(candidate);
    }

    // Copying is complete. Release the ESP32 driver's scan result allocation.
    backend_.clearResults();
    state_ = State::Ready;
  }

  State state() const {
    return state_;
  }

  uint8_t count() const {
    return state_ == State::Ready ? count_ : 0;
  }

  bool getAt(uint8_t index, WiFiScanResult& output) const {
    if (state_ != State::Ready || index >= count_) {
      return false;
    }
    output = results_[index];
    return true;
  }

  void reset() {
    if (state_ == State::Scanning) {
      backend_.clearResults();
    }
    clearResults();
    state_ = State::Idle;
  }

private:
  static bool sameSsid(
      const WiFiScanResult& left,
      const WiFiScanResult& right) {
    return strcmp(left.ssid, right.ssid) == 0;
  }

  void clearResults() {
    memset(results_, 0, sizeof(results_));
    count_ = 0;
  }

  void insert(const WiFiScanResult& candidate) {
    // Multiple BSSIDs can advertise the same SSID. Keep the strongest visible
    // instance so the compact UI does not show duplicate network names.
    for (uint8_t i = 0; i < count_; ++i) {
      if (!sameSsid(results_[i], candidate)) {
        continue;
      }

      if (candidate.rssiDbm <= results_[i].rssiDbm) {
        return;
      }

      for (uint8_t j = i; j + 1 < count_; ++j) {
        results_[j] = results_[j + 1];
      }
      --count_;
      break;
    }

    uint8_t insertAt = 0;
    while (insertAt < count_ &&
           results_[insertAt].rssiDbm >= candidate.rssiDbm) {
      ++insertAt;
    }

    if (count_ == MaxResults && insertAt >= MaxResults) {
      return;
    }

    if (count_ < MaxResults) {
      ++count_;
    }

    for (uint8_t j = static_cast<uint8_t>(count_ - 1);
         j > insertAt; --j) {
      results_[j] = results_[j - 1];
    }
    results_[insertAt] = candidate;
  }

  WiFiScanBackend& backend_;
  State state_;
  WiFiScanResult results_[MaxResults];
  uint8_t count_;
};
