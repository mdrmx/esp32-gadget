#pragma once

#include <stdint.h>

// Schedules lightweight refreshes of saved-profile availability while the
// Wi-Fi Profiles screen is visible. The scanner itself remains asynchronous.
class WiFiAvailabilityRefreshPolicy {
public:
  static const uint32_t RefreshIntervalMs = 30000UL;
  static const uint32_t FailedScanRetryIntervalMs = 5000UL;

  WiFiAvailabilityRefreshPolicy()
      : lastAttemptAtMs_(0), hasAttempt_(false) {}

  bool shouldStart(
      uint32_t nowMs,
      bool profilesVisible,
      bool scanInProgress,
      bool lastScanFailed) const {
    if (!profilesVisible || scanInProgress) {
      return false;
    }

    if (!hasAttempt_) {
      return true;
    }

    const uint32_t interval = lastScanFailed
        ? FailedScanRetryIntervalMs
        : RefreshIntervalMs;
    return static_cast<uint32_t>(nowMs - lastAttemptAtMs_) >= interval;
  }

  // Called for both successful and failed start attempts. This prevents a
  // persistent driver error from producing a tight retry loop.
  void noteAttempt(uint32_t nowMs) {
    lastAttemptAtMs_ = nowMs;
    hasAttempt_ = true;
  }

private:
  uint32_t lastAttemptAtMs_;
  bool hasAttempt_;
};
