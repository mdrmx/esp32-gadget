#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

// Owns the single current-weather icon asset. Downloading is deliberately
// supplied by the caller; this class is responsible only for cache identity,
// validation, byte-buffer ownership and replacement/freeing.
class WeatherAssetCache {
 public:
  static constexpr size_t IconCodeLength = 3;
  static constexpr size_t MinPngBytes = 9;
  static constexpr size_t MaxPngBytes = 20000;

  WeatherAssetCache()
      : data_(nullptr), length_(0), code_() {}

  ~WeatherAssetCache() {
    clear();
  }

  WeatherAssetCache(const WeatherAssetCache&) = delete;
  WeatherAssetCache& operator=(const WeatherAssetCache&) = delete;
  WeatherAssetCache(WeatherAssetCache&&) = delete;
  WeatherAssetCache& operator=(WeatherAssetCache&&) = delete;

  bool contains(const char* iconCode) const {
    return isValidIconCode(iconCode) &&
           data_ != nullptr &&
           length_ >= MinPngBytes &&
           sameCode(iconCode);
  }

  // Returns the cached mutable buffer for PNGdec. The pointer remains owned by
  // this object and is valid until clear() or a successful replacement.
  uint8_t* get(const char* iconCode, size_t* length) const {
    if (length != nullptr) {
      *length = 0;
    }

    if (!contains(iconCode)) {
      return nullptr;
    }

    if (length != nullptr) {
      *length = length_;
    }
    return data_;
  }

  // Takes ownership of ownedData only when validation succeeds. On failure,
  // ownership remains with the caller. A successful replacement frees the
  // previous asset after the new asset has been accepted.
  bool store(
      const char* iconCode,
      uint8_t* ownedData,
      size_t length
  ) {
    if (!isValidIconCode(iconCode) ||
        !isValidPng(ownedData, length)) {
      return false;
    }

    uint8_t* previousData = data_;
    data_ = ownedData;
    length_ = length;
    code_[0] = iconCode[0];
    code_[1] = iconCode[1];
    code_[2] = iconCode[2];
    code_[3] = '\0';

    if (previousData != nullptr && previousData != ownedData) {
      free(previousData);
    }
    return true;
  }

  void clear() {
    uint8_t* previousData = data_;
    data_ = nullptr;
    length_ = 0;
    code_[0] = '\0';
    code_[1] = '\0';
    code_[2] = '\0';
    code_[3] = '\0';

    if (previousData != nullptr) {
      free(previousData);
    }
  }

 private:
  static bool isValidIconCode(const char* iconCode) {
    return iconCode != nullptr &&
           iconCode[0] != '\0' &&
           iconCode[1] != '\0' &&
           iconCode[2] != '\0' &&
           iconCode[3] == '\0';
  }

  static bool isValidPng(const uint8_t* data, size_t length) {
    return data != nullptr &&
           length >= MinPngBytes &&
           length <= MaxPngBytes &&
           data[0] == 0x89 &&
           data[1] == 0x50 &&
           data[2] == 0x4E &&
           data[3] == 0x47;
  }

  bool sameCode(const char* iconCode) const {
    return code_[0] == iconCode[0] &&
           code_[1] == iconCode[1] &&
           code_[2] == iconCode[2] &&
           iconCode[3] == '\0';
  }

  uint8_t* data_;
  size_t length_;
  char code_[IconCodeLength + 1];
};
