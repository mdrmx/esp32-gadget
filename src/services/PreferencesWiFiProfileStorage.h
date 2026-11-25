#pragma once

#include <Preferences.h>
#include <stddef.h>
#include <stdint.h>

// Small Preferences/NVS adapter for WiFiProfileStore. Each profile-set update
// replaces one bounded blob key; putBytes() is checked before the store exposes
// the updated profile set to callers.
class PreferencesWiFiProfileStorage {
 public:
  bool read(uint8_t* destination, size_t capacity, size_t& outputLength) {
    outputLength = 0;
    if (destination == nullptr || !preferences_.begin(kNamespace(), true)) {
      return false;
    }

    const size_t storedLength = preferences_.getBytesLength(kKey());
    outputLength = storedLength;
    if (storedLength == 0) {
      preferences_.end();
      return true;
    }

    if (storedLength > capacity) {
      preferences_.end();
      return false;
    }

    const size_t bytesRead =
        preferences_.getBytes(kKey(), destination, storedLength);
    preferences_.end();
    return bytesRead == storedLength;
  }

  bool write(const uint8_t* source, size_t length) {
    if (source == nullptr || length == 0 ||
        !preferences_.begin(kNamespace(), false)) {
      return false;
    }

    const size_t bytesWritten =
        preferences_.putBytes(kKey(), source, length);
    preferences_.end();
    return bytesWritten == length;
  }

 private:
  static const char* kNamespace() {
    return "wifi_profiles";
  }

  static const char* kKey() {
    return "profiles";
  }

  Preferences preferences_;
};
