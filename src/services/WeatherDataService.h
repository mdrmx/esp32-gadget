#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <time.h>

#include "HttpRequestClient.h"

// Forecast records are deliberately fixed-capacity. The view layer reads this
// model but does not own the downloaded weather data.
struct ForecastItem {
  String timeText;
  String description;  // OpenWeather condition group, e.g. Clouds, Rain, Clear.
  float temp = 0.0f;
  float feelsLike = 0.0f;
  float rainProbability = 0.0f; // 0.0 to 1.0 from OpenWeather pop.
  uint32_t timestamp = 0;
};

struct WeatherCurrentData {
  String city;
  String description;
  String main;
  String icon = "03d";

  float temperature = 0.0f;
  float feelsLike = 0.0f;
  int humidity = 0;
  int pressure = 0;
  float windSpeedKmh = 0.0f;
  int cloudCover = 0;
  int visibilityMeters = 0;
  float rainLastHourMm = 0.0f;

  bool visibilityAvailable = false;
  bool available = false;
};

struct WeatherForecastData {
  static const int MaxRecords = 48;

  ForecastItem items[MaxRecords];
  int count = 0;
  bool available = false;
  String error;

  // Provider-local forecast time is distinct from the device's Europe/London
  // civil clock configured by TimeService.
  long timezoneOffsetSeconds = 0;

  // These metrics are populated from the nearest valid hourly record.
  float currentUvIndex = 0.0f;
  float currentRainProbability = 0.0f;
  bool uvIndexAvailable = false;
  bool rainProbabilityAvailable = false;
};

// Owns current Weather and One Call data acquisition/parsing and the bounded
// models consumed by the Weather UI. It deliberately does not connect Wi-Fi,
// resolve device location, draw loading/error UI, or manage downloaded icons.
//
// The caller supplies the API key and already-selected coordinates. Requests
// remain synchronous and sequential to preserve the existing refresh policy.
class WeatherDataService {
public:
  enum class ErrorCode : uint8_t {
    None,
    RequestInitializationFailed,
    Unauthorized,
    NotFound,
    HttpError,
    JsonParseFailed,
    RateLimited,
    NetworkError,
    UnexpectedResponse,
    NoForecastRecords
  };

  struct FetchResult {
    bool success = false;
    bool partial = false;
    int httpStatus = 0;
    int pagesFetched = 0;
    ErrorCode errorCode = ErrorCode::None;
    String detail;

    // Current-weather response coordinates are only a last-resort suggestion.
    // The caller applies them only when LocationService did not resolve a fix.
    bool hasLocationCoordinates = false;
    float suggestedLatitude = 0.0f;
    float suggestedLongitude = 0.0f;
  };

  WeatherCurrentData& current() {
    return current_;
  }

  WeatherForecastData& forecast() {
    return forecast_;
  }

  FetchResult fetchCurrentWeather(
    float latitude,
    float longitude,
    const char* openWeatherApiKey,
    bool locationCoordinatesResolved
  ) {
    FetchResult result;

    const String url =
      "https://api.openweathermap.org/data/2.5/weather?lat=" +
      String(latitude, 4) + "&lon=" + String(longitude, 4) +
      "&appid=" +
      String(openWeatherApiKey != nullptr ? openWeatherApiKey : "") +
      "&units=metric";

    HttpRequestClient http;
    if (!http.begin(
          url, 15000, 15000,
          HttpRequestClient::TlsPolicy::InsecureCompatibility)) {
      result.errorCode = ErrorCode::RequestInitializationFailed;
      result.detail = "Could not initialise current-weather request";
      return result;
    }

    const int status = http.get();
    if (status != 200) {
      http.end();
      result.httpStatus = status;
      result.errorCode = currentWeatherHttpError(status);
      result.detail = "Current-weather HTTP request failed";
      return result;
    }

    JsonDocument doc;
    const DeserializationError error =
      deserializeJson(doc, http.getStream());
    http.end();

    if (error) {
      result.errorCode = ErrorCode::JsonParseFailed;
      result.detail = error.c_str();
      return result;
    }

    current_.city = doc["name"] | "Unknown";
    current_.description = doc["weather"][0]["description"] | "Unknown";
    current_.main = doc["weather"][0]["main"] | "Unknown";
    current_.icon = doc["weather"][0]["icon"] | "03d";

    if (!locationCoordinatesResolved) {
      result.hasLocationCoordinates =
        !doc["coord"]["lat"].isNull() || !doc["coord"]["lon"].isNull();
      result.suggestedLatitude =
        doc["coord"]["lat"] | latitude;
      result.suggestedLongitude =
        doc["coord"]["lon"] | longitude;
    }

    forecast_.timezoneOffsetSeconds =
      doc["timezone"] | forecast_.timezoneOffsetSeconds;
    current_.temperature = doc["main"]["temp"] | 0.0f;
    current_.feelsLike = doc["main"]["feels_like"] | 0.0f;
    current_.humidity = doc["main"]["humidity"] | 0;
    current_.pressure = doc["main"]["pressure"] | 0;
    current_.windSpeedKmh =
      (doc["wind"]["speed"] | 0.0f) * 3.6f;
    current_.cloudCover = doc["clouds"]["all"] | 0;
    current_.visibilityAvailable = !doc["visibility"].isNull();
    current_.visibilityMeters = doc["visibility"] | 0;
    current_.rainLastHourMm = doc["rain"]["1h"] | 0.0f;
    current_.available = true;

    result.success = true;
    return result;
  }

  FetchResult fetchHourlyForecast(
    float latitude,
    float longitude,
    const char* openWeatherApiKey
  ) {
    FetchResult result;
    forecast_.count = 0;
    forecast_.error = "";
    forecast_.currentUvIndex = 0.0f;
    forecast_.currentRainProbability = 0.0f;
    forecast_.uvIndexAvailable = false;
    forecast_.rainProbabilityAvailable = false;
    forecast_.available = false;

    String nextUrl =
      "https://api.openweathermap.org/data/4.0/onecall/timeline/1h?lat=" +
      String(latitude, 4) + "&lon=" + String(longitude, 4) +
      "&units=metric&appid=" +
      String(openWeatherApiKey != nullptr ? openWeatherApiKey : "");

    // The endpoint returns up to 20 entries per request. Follow its prepared
    // pagination URL until we have up to 48 records or no next page.
    static const int MaxHourlyPages = 3;
    while (nextUrl.length() > 0 &&
           forecast_.count < WeatherForecastData::MaxRecords &&
           result.pagesFetched < MaxHourlyPages) {
      String pageNext;
      FetchResult pageResult = fetchOneCallForecastPage(nextUrl, pageNext);
      if (!pageResult.success) {
        if (forecast_.count == 0) {
          forecast_.available = false;
          return pageResult;
        }

        // Preserve the earlier records if a later page fails. The caller can
        // still render usable partial data and inspect the error state.
        forecast_.available = true;
        result.success = true;
        result.partial = true;
        result.errorCode = pageResult.errorCode;
        result.httpStatus = pageResult.httpStatus;
        result.detail = pageResult.detail;
        return result;
      }

      nextUrl = pageNext;
      result.pagesFetched++;
    }

    forecast_.available = forecast_.count > 0;
    if (!forecast_.available) {
      forecast_.error = "No hourly records";
      result.errorCode = ErrorCode::NoForecastRecords;
      result.detail = forecast_.error;
      return result;
    }

    result.success = true;
    return result;
  }

  // Deterministic offline fixture used by the existing demo-mode switch.
  // It populates the same service-owned models without making network requests.
  bool loadDemoData(time_t now) {
    current_.city = "Manchester";
    current_.description = "scattered clouds";
    current_.main = "Clouds";
    current_.icon = "03d";
    current_.temperature = 14.6f;
    current_.feelsLike = 13.8f;
    current_.humidity = 68;
    current_.pressure = 1014;
    current_.windSpeedKmh = 16.2f;
    current_.cloudCover = 40;
    current_.visibilityMeters = 10000;
    current_.rainLastHourMm = 0.2f;
    current_.visibilityAvailable = true;
    current_.available = true;

    forecast_.timezoneOffsetSeconds = 0;
    forecast_.currentUvIndex = 1.4f;
    forecast_.currentRainProbability = 0.25f;
    forecast_.uvIndexAvailable = true;
    forecast_.rainProbabilityAvailable = true;
    forecast_.error = "";
    forecast_.count = 24;
    forecast_.available = true;

    const char* descriptions[] = {
      "Clouds", "Clouds", "Rain", "Clouds", "Clear", "Clear",
      "Clouds", "Rain", "Clouds", "Clear", "Clouds", "Clouds"
    };
    const float temperatures[] = {
      14.6f, 14.1f, 13.8f, 13.2f, 12.9f, 12.5f,
      12.1f, 11.8f, 11.6f, 11.9f, 12.4f, 13.0f
    };
    const float rainProbabilities[] = {
      0.05f, 0.10f, 0.35f, 0.55f, 0.18f, 0.05f,
      0.03f, 0.72f, 0.48f, 0.12f, 0.06f, 0.02f
    };

    for (int i = 0; i < forecast_.count; i++) {
      const int sample = i % 12;
      ForecastItem& item = forecast_.items[i];
      item.timestamp = (uint32_t)(now + (time_t)i * 3600);
      item.timeText = formatForecastTime(item.timestamp);
      item.description = descriptions[sample];
      item.temp = temperatures[sample];
      item.feelsLike =
        temperatures[sample] - (sample % 3 == 0 ? 1.0f : 0.0f);
      item.rainProbability = rainProbabilities[sample];
    }

    return true;
  }

private:
  WeatherCurrentData current_;
  WeatherForecastData forecast_;

  static ErrorCode currentWeatherHttpError(int status) {
    if (status == 401) return ErrorCode::Unauthorized;
    if (status == 404) return ErrorCode::NotFound;
    return ErrorCode::HttpError;
  }

  static String describeForecastHttpError(int status) {
    if (status == 401 || status == 403) return "Check One Call access";
    if (status == 429) return "One Call rate limit";
    if (status < 0) return "One Call network error";
    return "One Call HTTP " + String(status);
  }

  String formatForecastTime(uint32_t timestamp) const {
    const time_t localTimestamp =
      (time_t)timestamp + (time_t)forecast_.timezoneOffsetSeconds;
    struct tm localTime;
    gmtime_r(&localTimestamp, &localTime);

    char clockText[8];
    snprintf(
      clockText, sizeof(clockText), "%02d:%02d",
      localTime.tm_hour, localTime.tm_min);

    if (forecast_.count > 0) {
      const time_t firstTimestamp =
        (time_t)forecast_.items[0].timestamp +
        (time_t)forecast_.timezoneOffsetSeconds;
      struct tm firstTime;
      gmtime_r(&firstTimestamp, &firstTime);

      if (localTime.tm_yday != firstTime.tm_yday ||
          localTime.tm_year != firstTime.tm_year) {
        const char* dayNames[] = {
          "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
        };
        return String(dayNames[localTime.tm_wday]) + " " + String(clockText);
      }
    }

    return String(clockText);
  }

  FetchResult fetchOneCallForecastPage(
    const String& url,
    String& nextUrl
  ) {
    FetchResult result;
    HttpRequestClient http;
    if (!http.begin(
          url, 15000, 15000,
          HttpRequestClient::TlsPolicy::InsecureCompatibility)) {
      forecast_.error = "Hourly API unavailable";
      result.errorCode = ErrorCode::RequestInitializationFailed;
      result.detail = forecast_.error;
      return result;
    }

    const int status = http.get();
    if (status != 200) {
      // The error body is diagnostic only. Read at most 240 bytes so a large
      // captive-portal/CDN response cannot allocate an unbounded String.
      String errorPreview;
      NetworkClient* responseStream = http.getStreamPtr();
      if (responseStream != nullptr) {
        char previewBuffer[241];
        const size_t previewLength = responseStream->readBytes(
          reinterpret_cast<uint8_t*>(previewBuffer),
          sizeof(previewBuffer) - 1);
        if (previewLength > 0) {
          previewBuffer[previewLength] = '\\0';
          errorPreview = previewBuffer;
        }
      }
      http.end();

      forecast_.error = describeForecastHttpError(status);
      result.httpStatus = status;
      result.errorCode =
        status == 429 ? ErrorCode::RateLimited :
        status < 0 ? ErrorCode::NetworkError :
        status == 401 || status == 403 ? ErrorCode::Unauthorized :
        ErrorCode::HttpError;
      result.detail = forecast_.error;
      if (errorPreview.length() > 0) {
        result.detail += ": " + errorPreview;
      }
      return result;
    }

    JsonDocument doc;
    const DeserializationError error =
      deserializeJson(doc, http.getStream());
    http.end();

    if (error) {
      forecast_.error = "Hourly data parse failed";
      result.errorCode = ErrorCode::JsonParseFailed;
      result.detail = error.c_str();
      return result;
    }

    forecast_.timezoneOffsetSeconds =
      doc["timezone_offset"] | forecast_.timezoneOffsetSeconds;
    nextUrl = doc["next"] | "";

    JsonArray list = doc["data"].as<JsonArray>();
    if (list.isNull()) {
      forecast_.error = "Unexpected One Call data";
      result.errorCode = ErrorCode::UnexpectedResponse;
      result.detail = forecast_.error;
      return result;
    }

    for (JsonObject item : list) {
      if (forecast_.count >= WeatherForecastData::MaxRecords) break;

      const uint32_t timestamp = item["dt"] | 0UL;
      if (timestamp == 0) continue;

      bool duplicate = false;
      for (int i = 0; i < forecast_.count; i++) {
        if (forecast_.items[i].timestamp == timestamp) {
          duplicate = true;
          break;
        }
      }
      if (duplicate) continue;

      const char* description = item["weather"][0]["main"] | "Unknown";
      const float temperature = item["temp"] | 0.0f;
      const float feelsLike = item["feels_like"] | temperature;
      float rainProbability = item["pop"] | 0.0f;
      rainProbability = constrain(rainProbability, 0.0f, 1.0f);

      // The first valid hourly record is the nearest available estimate for
      // current UV index and precipitation probability.
      if (forecast_.count == 0) {
        if (!item["uvi"].isNull()) {
          forecast_.currentUvIndex = item["uvi"].as<float>();
          forecast_.uvIndexAvailable = true;
        }
        if (!item["pop"].isNull()) {
          forecast_.currentRainProbability = item["pop"].as<float>();
          forecast_.rainProbabilityAvailable = true;
        }
      }

      ForecastItem& record = forecast_.items[forecast_.count];
      record.timestamp = timestamp;
      record.timeText = formatForecastTime(timestamp);
      record.description = String(description);
      record.temp = temperature;
      record.feelsLike = feelsLike;
      record.rainProbability = rainProbability;
      forecast_.count++;
    }

    result.success = true;
    return result;
  }
};
