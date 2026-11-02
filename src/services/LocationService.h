#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>

// Owns location-provider orchestration and the resulting fix. It uses the
// existing Wi-Fi connection but never starts, stops or reconfigures Wi-Fi.
class LocationService {
public:
  enum class Source : uint8_t {
    ConfiguredFallback,
    GoogleWifi,
    IpGeolocation,
    Gnss
  };

  struct LocationFix {
    float latitude;
    float longitude;
    Source source;
    float accuracyRadiusMeters;
    bool hasAccuracyRadius;
    uint32_t observedAtMs;
    bool valid;
  };

  struct Result {
    LocationFix fix;
    String displayName;
  };

  // Preserve the existing provider priority: Google Wi-Fi positioning, then
  // coarse public-IP geolocation, then the configured coordinate fallback.
  // The caller owns Wi-Fi association and supplies the current connection
  // state. API keys are supplied by the sketch and are never stored here.
  Result resolve(
    bool networkConnected,
    const char* googleGeolocationApiKey,
    const char* openWeatherApiKey,
    float fallbackLatitude,
    float fallbackLongitude
  ) {
    Result result = {};
    result.fix = configuredFallback(fallbackLatitude, fallbackLongitude);

    if (!networkConnected) {
      Serial.println(
        "[LOCATION] Network unavailable; using configured coordinates.");
      return result;
    }

    LocationFix resolvedFix = {};
    String resolvedName;

    if (resolveFromWiFi(
          googleGeolocationApiKey,
          openWeatherApiKey,
          resolvedFix,
          resolvedName)) {
      result.fix = resolvedFix;
      result.displayName = resolvedName;
      return result;
    }

    if (resolveFromIP(
          openWeatherApiKey,
          resolvedFix,
          resolvedName)) {
      result.fix = resolvedFix;
      result.displayName = resolvedName;
      return result;
    }

    Serial.println(
      "[LOCATION] All location providers failed; using configured coordinates.");
    return result;
  }

  static LocationFix configuredFallback(
    float latitude,
    float longitude
  ) {
    LocationFix fix = {};
    fix.latitude = latitude;
    fix.longitude = longitude;
    fix.source = Source::ConfiguredFallback;
    fix.accuracyRadiusMeters = 0.0f;
    fix.hasAccuracyRadius = false;
    fix.observedAtMs = millis();
    fix.valid = isValidCoordinates(latitude, longitude);
    return fix;
  }

  static const char* sourceName(Source source) {
    switch (source) {
      case Source::GoogleWifi: return "Google Wi-Fi";
      case Source::IpGeolocation: return "IP geolocation";
      case Source::Gnss: return "GNSS";
      case Source::ConfiguredFallback:
      default: return "configured fallback";
    }
  }

private:
  struct WifiCandidate {
    uint8_t bssid[6];
    int32_t rssi;
    int32_t channel;
  };

  static bool isValidCoordinates(float latitude, float longitude) {
    return isfinite(latitude) && isfinite(longitude) &&
           latitude >= -90.0f && latitude <= 90.0f &&
           longitude >= -180.0f && longitude <= 180.0f;
  }

  bool resolveLocationNameFromCoordinates(
    const LocationFix& fix,
    const char* openWeatherApiKey,
    String& displayName
  ) {
    Serial.printf(
      "[LOCATION] Reverse geocoding coordinates %.5f, %.5f...\n",
      (double)fix.latitude,
      (double)fix.longitude);

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;

    String url =
      "https://api.openweathermap.org/geo/1.0/reverse?lat=" +
      String(fix.latitude, 5) + "&lon=" + String(fix.longitude, 5) +
      "&limit=1&appid=" +
      String(openWeatherApiKey != nullptr ? openWeatherApiKey : "");

    if (!http.begin(client, url)) {
      Serial.println(
        "[LOCATION] Could not initialise OpenWeather reverse geocoding; "
        "keeping existing location name.");
      return false;
    }

    http.setConnectTimeout(8000);
    http.setTimeout(8000);
    const int status = http.GET();
    if (status != HTTP_CODE_OK) {
      Serial.printf(
        "[LOCATION] Reverse geocoding HTTP status: %d; "
        "keeping existing location name.\n",
        status);
      http.end();
      return false;
    }

    JsonDocument doc;
    const DeserializationError error =
      deserializeJson(doc, http.getStream());
    http.end();

    if (error || !doc.is<JsonArray>() ||
        doc.size() == 0 || doc[0]["name"].isNull()) {
      Serial.printf(
        "[LOCATION] Reverse geocoding response had no place name "
        "(JSON error: %s); keeping existing location name.\n",
        error ? error.c_str() : "none");
      return false;
    }

    String resolvedName = doc[0]["name"] | "";
    resolvedName.trim();
    if (resolvedName.length() == 0) {
      Serial.println(
        "[LOCATION] Reverse geocoding returned an empty place name; "
        "keeping existing location name.");
      return false;
    }

    displayName = resolvedName;
    Serial.printf(
      "[LOCATION] OpenWeather reverse geocoded place: %s\n",
      displayName.c_str());
    return true;
  }

  // Resolve the device from nearby stationary Wi-Fi access points using
  // Google's Geolocation API. Scanning is passive; no scanned network is
  // joined. Wi-Fi lifecycle remains the caller's responsibility.
  bool resolveFromWiFi(
    const char* googleGeolocationApiKey,
    const char* openWeatherApiKey,
    LocationFix& fix,
    String& displayName
  ) {
    if (googleGeolocationApiKey == nullptr ||
        googleGeolocationApiKey[0] == '\0') {
      Serial.println(
        "[LOCATION] GOOGLE_GEOLOCATION_API_KEY is missing from secrets.h; "
        "trying IP geolocation instead.");
      return false;
    }

    if (WiFi.status() != WL_CONNECTED) {
      Serial.printf(
        "[LOCATION] Google Wi-Fi positioning skipped: Wi-Fi status is %d.\n",
        (int)WiFi.status());
      return false;
    }

    Serial.println("[LOCATION] Scanning nearby Wi-Fi access points...");
    // Keep the per-channel dwell short because this runs during startup.
    const int16_t scanCount = WiFi.scanNetworks(false, false, true, 120);
    if (scanCount < 0) {
      Serial.printf(
        "[LOCATION] Wi-Fi scan failed with code %d; trying IP geolocation.\n",
        scanCount);
      WiFi.scanDelete();
      return false;
    }

    const int MAX_WIFI_GEOLOCATION_APS = 15;
    WifiCandidate candidates[MAX_WIFI_GEOLOCATION_APS];
    int candidateCount = 0;

    // Google only locates universally-administered unicast MAC addresses.
    // Keep the strongest eligible, unique BSSIDs to maximise useful evidence.
    for (int i = 0; i < scanCount; i++) {
      uint8_t* bssid = WiFi.BSSID(i);
      if (bssid == nullptr) continue;

      // Bit 0 set means multicast; bit 1 set means locally administered.
      if ((bssid[0] & 0x03) != 0) continue;

      // Google's documented reserved range is excluded.
      if (bssid[0] == 0x00 && bssid[1] == 0x00 && bssid[2] == 0x5E) {
        continue;
      }

      bool allZero = true;
      for (int octet = 0; octet < 6; octet++) {
        if (bssid[octet] != 0) {
          allZero = false;
          break;
        }
      }
      if (allZero) continue;

      const int32_t rssi = WiFi.RSSI(i);
      if (rssi < -128 || rssi > -10) continue;

      bool duplicate = false;
      for (int j = 0; j < candidateCount; j++) {
        bool same = true;
        for (int octet = 0; octet < 6; octet++) {
          if (candidates[j].bssid[octet] != bssid[octet]) {
            same = false;
            break;
          }
        }
        if (same) {
          duplicate = true;
          break;
        }
      }
      if (duplicate) continue;

      WifiCandidate candidate;
      for (int octet = 0; octet < 6; octet++) {
        candidate.bssid[octet] = bssid[octet];
      }
      candidate.rssi = rssi;
      candidate.channel = WiFi.channel(i);

      // Retain the strongest 15 usable access points in descending RSSI order.
      int insertAt = 0;
      while (insertAt < candidateCount &&
             candidates[insertAt].rssi >= candidate.rssi) {
        insertAt++;
      }
      if (insertAt >= MAX_WIFI_GEOLOCATION_APS) continue;

      int newCount = candidateCount;
      if (newCount < MAX_WIFI_GEOLOCATION_APS) newCount++;
      for (int j = newCount - 1; j > insertAt; j--) {
        candidates[j] = candidates[j - 1];
      }
      candidates[insertAt] = candidate;
      candidateCount = newCount;
    }

    WiFi.scanDelete();
    Serial.printf(
      "[LOCATION] Wi-Fi scan found %d networks; %d usable BSSIDs.\n",
      scanCount, candidateCount);

    // Google requires at least two distinct stationary access points.
    if (candidateCount < 2) {
      Serial.println(
        "[LOCATION] Fewer than two usable BSSIDs; trying IP geolocation.");
      return false;
    }

    JsonDocument requestDoc;
    requestDoc["considerIp"] = false;
    JsonArray wifiPoints = requestDoc["wifiAccessPoints"].to<JsonArray>();

    // ArduinoJson may retain references to these writable buffers until
    // serializeJson() runs, so their lifetime must extend beyond the loop.
    char macAddresses[MAX_WIFI_GEOLOCATION_APS][18] = {};

    for (int i = 0; i < candidateCount; i++) {
      snprintf(
        macAddresses[i], sizeof(macAddresses[i]),
        "%02x:%02x:%02x:%02x:%02x:%02x",
        candidates[i].bssid[0],
        candidates[i].bssid[1],
        candidates[i].bssid[2],
        candidates[i].bssid[3],
        candidates[i].bssid[4],
        candidates[i].bssid[5]);

      JsonObject point = wifiPoints.add<JsonObject>();
      point["macAddress"] = macAddresses[i];
      point["signalStrength"] = candidates[i].rssi;
      point["channel"] = candidates[i].channel;
      point["age"] = 0;
    }

    String requestBody;
    serializeJson(requestDoc, requestBody);

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    String url =
      "https://www.googleapis.com/geolocation/v1/geolocate?key=" +
      String(googleGeolocationApiKey);

    if (!http.begin(client, url)) {
      Serial.println(
        "[LOCATION] Could not initialise Google Geolocation request; "
        "trying IP geolocation.");
      return false;
    }

    http.addHeader("Content-Type", "application/json");
    http.setConnectTimeout(10000);
    http.setTimeout(12000);
    const int status = http.POST(requestBody);
    String responseBody = http.getString();
    http.end();

    if (status != HTTP_CODE_OK) {
      if (responseBody.length() > 220) {
        responseBody = responseBody.substring(0, 220);
      }
      Serial.printf(
        "[LOCATION] Google Geolocation HTTP status %d; trying IP geolocation.\n",
        status);
      if (responseBody.length() > 0) {
        Serial.printf(
          "[LOCATION] Google response: %s\n",
          responseBody.c_str());
      }
      return false;
    }

    JsonDocument responseDoc;
    const DeserializationError error =
      deserializeJson(responseDoc, responseBody);
    if (error ||
        responseDoc["location"]["lat"].isNull() ||
        responseDoc["location"]["lng"].isNull()) {
      Serial.printf(
        "[LOCATION] Google response had no valid coordinates "
        "(JSON error: %s); trying IP geolocation.\n",
        error ? error.c_str() : "missing location");
      return false;
    }

    const double resolvedLat =
      responseDoc["location"]["lat"].as<double>();
    const double resolvedLon =
      responseDoc["location"]["lng"].as<double>();
    const double accuracyMeters = responseDoc["accuracy"] | -1.0;

    if (!isfinite(resolvedLat) || !isfinite(resolvedLon) ||
        resolvedLat < -90.0 || resolvedLat > 90.0 ||
        resolvedLon < -180.0 || resolvedLon > 180.0 ||
        (accuracyMeters >= 0.0 && !isfinite(accuracyMeters))) {
      Serial.println(
        "[LOCATION] Google returned out-of-range coordinates; "
        "trying IP geolocation.");
      return false;
    }

    fix = {};
    fix.latitude = (float)resolvedLat;
    fix.longitude = (float)resolvedLon;
    fix.source = Source::GoogleWifi;
    fix.hasAccuracyRadius =
      isfinite(accuracyMeters) && accuracyMeters >= 0.0;
    fix.accuracyRadiusMeters =
      fix.hasAccuracyRadius ? (float)accuracyMeters : 0.0f;
    fix.observedAtMs = millis();
    fix.valid = true;
    displayName = "";

    if (fix.hasAccuracyRadius) {
      Serial.printf(
        "[LOCATION] Google Wi-Fi positioning succeeded: "
        "latitude=%.6f, longitude=%.6f, accuracy radius about %.0f m.\n",
        resolvedLat, resolvedLon, accuracyMeters);
    } else {
      Serial.printf(
        "[LOCATION] Google Wi-Fi positioning succeeded: "
        "latitude=%.6f, longitude=%.6f.\n",
        resolvedLat, resolvedLon);
    }

    // Resolve a human-readable place name without replacing Google's fix.
    resolveLocationNameFromCoordinates(
      fix, openWeatherApiKey, displayName);
    return true;
  }

  // IP geolocation is a coarse fallback and can identify an ISP/VPN exit point
  // rather than the device itself. It never reports an accuracy radius.
  bool resolveFromIP(
    const char* openWeatherApiKey,
    LocationFix& fix,
    String& displayName
  ) {
    Serial.println("[LOCATION] Starting IP geolocation request...");
    if (WiFi.status() != WL_CONNECTED) {
      Serial.printf(
        "[LOCATION] IP lookup skipped: Wi-Fi status is %d.\n",
        (int)WiFi.status());
      return false;
    }

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    if (!http.begin(client, "https://ipwho.is/")) {
      Serial.println(
        "[LOCATION] Could not initialise ipwho.is; using configured fallback.");
      return false;
    }

    http.setConnectTimeout(8000);
    http.setTimeout(8000);
    const int status = http.GET();
    if (status != HTTP_CODE_OK) {
      Serial.printf(
        "[LOCATION] IP geolocation HTTP status: %d; "
        "using configured fallback.\n",
        status);
      http.end();
      return false;
    }

    JsonDocument doc;
    const DeserializationError error =
      deserializeJson(doc, http.getStream());
    http.end();

    if (error || !(doc["success"] | false) ||
        doc["latitude"].isNull() || doc["longitude"].isNull()) {
      Serial.printf(
        "[LOCATION] ipwho.is response invalid (JSON error: %s, success: %d); "
        "using configured fallback.\n",
        error ? error.c_str() : "none",
        doc["success"] | false);
      return false;
    }

    const float resolvedLat = doc["latitude"].as<float>();
    const float resolvedLon = doc["longitude"].as<float>();
    if (!isValidCoordinates(resolvedLat, resolvedLon)) {
      Serial.printf(
        "[LOCATION] ipwho.is coordinates out of range: %.5f, %.5f; "
        "using configured fallback.\n",
        (double)resolvedLat, (double)resolvedLon);
      return false;
    }

    fix = {};
    fix.latitude = resolvedLat;
    fix.longitude = resolvedLon;
    fix.source = Source::IpGeolocation;
    fix.accuracyRadiusMeters = 0.0f;
    fix.hasAccuracyRadius = false;
    fix.observedAtMs = millis();
    fix.valid = true;
    displayName = doc["city"] | "";
    displayName.trim();

    Serial.printf(
      "[LOCATION] IP lookup succeeded: city='%s', latitude=%.5f, longitude=%.5f\n",
      displayName.length() > 0 ? displayName.c_str() : "city unavailable",
      (double)fix.latitude,
      (double)fix.longitude);

    // Prefer OpenWeather's place name while retaining the IP coordinates.
    resolveLocationNameFromCoordinates(
      fix, openWeatherApiKey, displayName);
    return true;
  }
};
