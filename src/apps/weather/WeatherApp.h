#ifndef ESP32_GADGET_WEATHER_APP_H
#define ESP32_GADGET_WEATHER_APP_H

#include <stdint.h>

// Coordinates the Weather application's lifecycle and recurring work.
//
// The application shell deliberately does not know about Arduino_GFX, Wi-Fi,
// HTTP, LocationService, or Weather's concrete data models. The sketch injects
// its existing operations through a fixed function-pointer table. This keeps
// the first extraction small, deterministic, and free of std::function or
// callback heap allocations while the individual views are moved in later
// slices.
class WeatherApp {
 public:
  enum class View : uint8_t {
    Weather = 0,
    Forecast = 1
  };

  struct Hooks {
    void (*processInput)();
    void (*syncTime)();
    void (*refreshClock)();
    uint32_t (*millisNow)();

    bool (*fetchCurrentWeather)();
    void (*fetchForecast)();
    bool (*weatherAvailable)();
    View (*activeView)();

    void (*drawWeather)();
    void (*drawForecast)();
    void (*drawError)(const char*);
    void (*refreshStarted)();

    Hooks()
        : processInput(nullptr),
          syncTime(nullptr),
          refreshClock(nullptr),
          millisNow(nullptr),
          fetchCurrentWeather(nullptr),
          fetchForecast(nullptr),
          weatherAvailable(nullptr),
          activeView(nullptr),
          drawWeather(nullptr),
          drawForecast(nullptr),
          drawError(nullptr),
          refreshStarted(nullptr) {}
  };

  static const uint32_t DefaultRefreshIntervalMs = 600000UL;
  static const uint32_t DefaultClockRefreshIntervalMs = 1000UL;

  explicit WeatherApp(
      uint32_t refreshIntervalMs = DefaultRefreshIntervalMs,
      uint32_t clockRefreshIntervalMs = DefaultClockRefreshIntervalMs)
      : hooks_(),
        refreshIntervalMs_(refreshIntervalMs),
        clockRefreshIntervalMs_(clockRefreshIntervalMs),
        lastRefreshMs_(0),
        lastClockRefreshMs_(0),
        active_(false) {}

  // Starts the Weather application after platform hardware and location setup.
  // Initial data retrieval intentionally remains current-weather then forecast.
  bool begin(const Hooks& hooks) {
    if (active_ || !hasRequiredHooks(hooks)) {
      return false;
    }

    hooks_ = hooks;
    active_ = true;

    hooks_.fetchCurrentWeather();
    hooks_.fetchForecast();

    if (hooks_.weatherAvailable()) {
      hooks_.drawWeather();
    } else {
      hooks_.drawError("Weather unavailable");
    }

    // Start the refresh interval after startup retrieval/rendering, matching
    // the previous sketch behaviour.
    lastRefreshMs_ = hooks_.millisNow();
    lastClockRefreshMs_ = lastRefreshMs_;
    return true;
  }

  // Called from the device loop. Input and time service work retain their
  // original order; Weather refreshes remain synchronous and sequential.
  void update() {
    if (!active_) {
      return;
    }

    hooks_.processInput();
    hooks_.syncTime();

    uint32_t now = hooks_.millisNow();
    if (now - lastClockRefreshMs_ >= clockRefreshIntervalMs_) {
      lastClockRefreshMs_ = now;
      hooks_.refreshClock();
    }

    now = hooks_.millisNow();
    if (now - lastRefreshMs_ >= refreshIntervalMs_) {
      // Set the timestamp before the blocking requests, as the original loop
      // did, so a slow refresh does not shift the established cadence.
      lastRefreshMs_ = now;

      if (hooks_.refreshStarted != nullptr) {
        hooks_.refreshStarted();
      }

      const bool currentOk = hooks_.fetchCurrentWeather();
      hooks_.fetchForecast();

      if (hooks_.activeView() == View::Forecast) {
        hooks_.drawForecast();
      } else if (currentOk || hooks_.weatherAvailable()) {
        hooks_.drawWeather();
      } else {
        hooks_.drawError("Refresh failed");
      }
    }
  }

  // Leaves the application inactive without releasing shared platform
  // resources. Resource lifetime remains with the platform owners.
  void end() {
    active_ = false;
    hooks_ = Hooks();
  }

  bool isActive() const {
    return active_;
  }

  uint32_t lastRefreshMs() const {
    return lastRefreshMs_;
  }

 private:
  Hooks hooks_;
  uint32_t refreshIntervalMs_;
  uint32_t clockRefreshIntervalMs_;
  uint32_t lastRefreshMs_;
  uint32_t lastClockRefreshMs_;
  bool active_;

  static bool hasRequiredHooks(const Hooks& hooks) {
    return hooks.processInput != nullptr &&
           hooks.syncTime != nullptr &&
           hooks.refreshClock != nullptr &&
           hooks.millisNow != nullptr &&
           hooks.fetchCurrentWeather != nullptr &&
           hooks.fetchForecast != nullptr &&
           hooks.weatherAvailable != nullptr &&
           hooks.activeView != nullptr &&
           hooks.drawWeather != nullptr &&
           hooks.drawForecast != nullptr &&
           hooks.drawError != nullptr;
  }
};

#endif  // ESP32_GADGET_WEATHER_APP_H
