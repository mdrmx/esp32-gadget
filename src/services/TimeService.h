#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

// Owns device civil-time configuration and synchronisation.
// Forecast/provider timezone offsets remain with Weather data and are not
// used to configure the device clock.
class TimeService {
public:
  enum class SyncResult : uint8_t {
    NotDue,
    NetworkUnavailable,
    RequestInitializationFailed,
    HttpError,
    InvalidJson,
    InvalidDateTime,
    InvalidEpoch,
    ClockSetFailed,
    Synced
  };

  // Configure Europe/London civil time, including UK daylight-saving rules.
  // This does not claim that the system clock is already valid.
  void begin() const {
    setenv("TZ", timeZoneInfo(), 1);
    tzset();
  }

  bool getCurrentUtcEpoch(time_t& epoch) const {
    const time_t systemEpoch = time(nullptr);
    if (systemEpoch < minimumValidEpoch()) {
      return false;
    }

    epoch = systemEpoch;
    return true;
  }

  // Return a display-ready Europe/London time. Header drawing and redraw
  // decisions remain owned by Weather.
  String headerClockText() const {
    time_t epoch = 0;
    if (!getCurrentUtcEpoch(epoch)) {
      return "--:--";
    }

    struct tm localTime = {};
    if (localtime_r(&epoch, &localTime) == nullptr) {
      return "--:--";
    }

    char clockText[6];
    snprintf(
      clockText,
      sizeof(clockText),
      "%02d:%02d",
      localTime.tm_hour,
      localTime.tm_min
    );
    return String(clockText);
  }

  // The caller owns network lifecycle and passes the current connectivity
  // state and scheduler time. This service never starts or stops Wi-Fi.
  SyncResult syncIfDue(bool networkConnected, unsigned long nowMs) {
    if (!networkConnected) {
      return SyncResult::NetworkUnavailable;
    }

    bool syncDue = false;
    if (timeApiValid_) {
      syncDue = nowMs - lastTimeApiSyncMs_ >= syncIntervalMs();
    } else {
      syncDue = lastTimeApiAttemptMs_ == 0 ||
                nowMs - lastTimeApiAttemptMs_ >= retryIntervalMs();
    }

    // A failed periodic sync must not retry more frequently than five minutes.
    if (!syncDue ||
        (lastTimeApiAttemptMs_ != 0 &&
         nowMs - lastTimeApiAttemptMs_ < retryIntervalMs())) {
      return SyncResult::NotDue;
    }

    lastTimeApiAttemptMs_ = nowMs;
    return fetchAndApplyTime();
  }

  // Used by the deterministic offline demo fixture. The supplied month is
  // one-based, matching ordinary calendar dates.
  bool setDemoTime(
    int year,
    int month,
    int day,
    int hour,
    int minute,
    int second
  ) const {
    if (month < 1 || month > 12 ||
        day < 1 || day > 31 ||
        hour < 0 || hour > 23 ||
        minute < 0 || minute > 59 ||
        second < 0 || second > 60) {
      return false;
    }

    struct tm demoClock = {};
    demoClock.tm_year = year - 1900;
    demoClock.tm_mon = month - 1;
    demoClock.tm_mday = day;
    demoClock.tm_hour = hour;
    demoClock.tm_min = minute;
    demoClock.tm_sec = second;
    demoClock.tm_isdst = -1;

    const time_t demoEpoch = mktime(&demoClock);
    if (demoEpoch < minimumValidEpoch()) {
      return false;
    }

    struct timeval demoSystemTime = {};
    demoSystemTime.tv_sec = demoEpoch;
    demoSystemTime.tv_usec = 0;
    return settimeofday(&demoSystemTime, nullptr) == 0;
  }

private:
  static const char* timeZoneInfo() {
    return "GMT0BST,M3.5.0/1,M10.5.0/2";
  }

  static const char* timeApiUrl() {
    return "https://timeapi.io/api/Time/current/zone?timeZone=Europe%2FLondon";
  }

  static time_t minimumValidEpoch() {
    return static_cast<time_t>(1704067200); // 2024-01-01
  }

  static unsigned long retryIntervalMs() {
    return 5UL * 60UL * 1000UL;
  }

  static unsigned long syncIntervalMs() {
    return 6UL * 60UL * 60UL * 1000UL;
  }

  SyncResult fetchAndApplyTime() {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    if (!http.begin(client, timeApiUrl())) {
      return SyncResult::RequestInitializationFailed;
    }

    http.setConnectTimeout(5000);
    http.setTimeout(5000);
    const int status = http.GET();

    if (status != HTTP_CODE_OK) {
      http.getString();
      http.end();
      return SyncResult::HttpError;
    }

    // Read the complete response first; TimeAPI responses may be chunked.
    const String responseBody = http.getString();
    http.end();

    JsonDocument doc;
    const DeserializationError error = deserializeJson(doc, responseBody);
    if (error) {
      return SyncResult::InvalidJson;
    }

    const int year = doc["year"] | 0;
    const int month = doc["month"] | 0;
    const int day = doc["day"] | 0;
    const int hour = doc["hour"] | -1;
    const int minute = doc["minute"] | -1;
    const int second = doc["seconds"] | (doc["second"] | 0);

    if (year < 2024 || year > 2099 ||
        month < 1 || month > 12 ||
        day < 1 || day > 31 ||
        hour < 0 || hour > 23 ||
        minute < 0 || minute > 59 ||
        second < 0 || second > 60) {
      return SyncResult::InvalidDateTime;
    }

    // TimeAPI returns Europe/London wall-clock fields. mktime() applies the
    // timezone/DST rules configured by begin() to obtain the UTC epoch.
    struct tm localTime = {};
    localTime.tm_year = year - 1900;
    localTime.tm_mon = month - 1;
    localTime.tm_mday = day;
    localTime.tm_hour = hour;
    localTime.tm_min = minute;
    localTime.tm_sec = second;
    localTime.tm_isdst = -1;

    const time_t epoch = mktime(&localTime);
    if (epoch < minimumValidEpoch()) {
      return SyncResult::InvalidEpoch;
    }

    struct timeval newSystemTime = {};
    newSystemTime.tv_sec = epoch;
    newSystemTime.tv_usec = 0;

    if (settimeofday(&newSystemTime, nullptr) != 0) {
      return SyncResult::ClockSetFailed;
    }

    timeApiValid_ = true;
    lastTimeApiSyncMs_ = millis();
    return SyncResult::Synced;
  }

  bool timeApiValid_ = false;
  unsigned long lastTimeApiAttemptMs_ = 0;
  unsigned long lastTimeApiSyncMs_ = 0;
};
