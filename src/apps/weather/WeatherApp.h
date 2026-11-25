#ifndef ESP32_GADGET_WEATHER_APP_H
#define ESP32_GADGET_WEATHER_APP_H

#include <stdint.h>

// Coordinates the Weather application's startup, update cadence and recurring
// work. Connectivity is platform-owned and injected as a small status/start
// interface; this class never includes Arduino or Wi-Fi headers.
class WeatherApp {
 public:
  enum class View : uint8_t {
    Weather = 0,
    Forecast = 1
  };

  enum class State : uint8_t {
    Inactive = 0,
    Connecting,
    ResolvingLocation,
    FetchingCurrentWeather,
    FetchingForecast,
    PreparingStartupAssets,
    Ready,
    RefreshConnecting,
    RefreshingCurrentWeather,
    RefreshingForecast,
    PreparingRefreshAssets
  };

  struct Hooks {
    void (*processInput)();
    void (*syncTime)();
    void (*refreshClock)();
    uint32_t (*millisNow)();

    // ConnectivityManager remains owned by the platform composition root.
    // startConnection must only initiate the attempt and must never wait.
    bool (*startConnection)(uint32_t nowMs);
    bool (*connectionPending)();
    bool (*isConnected)();

    // Location and data operations are advanced one at a time by update().
    void (*resolveLocation)();
    bool (*fetchCurrentWeather)();
    void (*fetchForecast)();

    // Optional pre-render hook for assets needed by the selected Weather view.
    void (*prepareWeatherAssets)();
    bool (*weatherAvailable)();
    View (*activeView)();

    void (*drawLoading)(const char*);
    void (*drawWeather)();
    void (*drawForecast)();
    void (*drawError)(const char*);
    void (*refreshStarted)();
    bool (*pauseWorkflow)();

    Hooks()
        : processInput(nullptr),
          syncTime(nullptr),
          refreshClock(nullptr),
          millisNow(nullptr),
          startConnection(nullptr),
          connectionPending(nullptr),
          isConnected(nullptr),
          resolveLocation(nullptr),
          fetchCurrentWeather(nullptr),
          fetchForecast(nullptr),
          prepareWeatherAssets(nullptr),
          weatherAvailable(nullptr),
          activeView(nullptr),
          drawLoading(nullptr),
          drawWeather(nullptr),
          drawForecast(nullptr),
          drawError(nullptr),
          refreshStarted(nullptr),
          pauseWorkflow(nullptr) {}
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
        state_(State::Inactive),
        active_(false),
        currentWeatherSucceeded_(false) {}

  // Binds platform/application operations and starts connection acquisition.
  // No association wait, location lookup or HTTP request runs inside begin().
  bool begin(const Hooks& hooks) {
    if (active_ || !hasRequiredHooks(hooks)) {
      return false;
    }

    hooks_ = hooks;
    active_ = true;
    currentWeatherSucceeded_ = false;

    const uint32_t now = hooks_.millisNow();
    lastRefreshMs_ = now;
    lastClockRefreshMs_ = now;
    state_ = State::Connecting;

    hooks_.drawLoading("Connecting Wi-Fi...");
    hooks_.startConnection(now);
    return true;
  }

  // Called from the device loop. While associating, input, clock and other
  // loop-level work continue on every pass. Each startup/refresh stage is
  // advanced separately rather than waiting for connectivity here.
  //
  // Location lookup, HTTP requests and asset preparation are still synchronous
  // operations when their individual stages run. Their transport-level
  // asynchronous conversion is intentionally a separate follow-up.
  void update() {
    if (!active_) {
      return;
    }

    hooks_.processInput();
    hooks_.syncTime();

    const uint32_t now = hooks_.millisNow();
    if (now - lastClockRefreshMs_ >= clockRefreshIntervalMs_) {
      lastClockRefreshMs_ = now;
      hooks_.refreshClock();
    }

    // Modal platform settings (such as Wi-Fi management) keep input and the
    // clock alive while pausing Weather's staged fetch/render workflow.
    if (hooks_.pauseWorkflow != nullptr && hooks_.pauseWorkflow()) {
      return;
    }

    switch (state_) {
      case State::Connecting:
        if (hooks_.isConnected() || !hooks_.connectionPending()) {
          state_ = State::ResolvingLocation;
        }
        return;

      case State::ResolvingLocation:
        hooks_.resolveLocation();
        state_ = State::FetchingCurrentWeather;
        return;

      case State::FetchingCurrentWeather:
        currentWeatherSucceeded_ = hooks_.fetchCurrentWeather();
        state_ = State::FetchingForecast;
        return;

      case State::FetchingForecast:
        hooks_.fetchForecast();
        state_ = State::PreparingStartupAssets;
        return;

      case State::PreparingStartupAssets:
        prepareWeatherAssetsIfAvailable(View::Weather);
        if (hooks_.weatherAvailable()) {
          hooks_.drawWeather();
        } else {
          hooks_.drawError("Weather unavailable");
        }
        // Preserve the old cadence: the next periodic refresh is measured
        // from completed startup retrieval/rendering, not from boot time.
        lastRefreshMs_ = hooks_.millisNow();
        state_ = State::Ready;
        return;

      case State::Ready:
        if (now - lastRefreshMs_ >= refreshIntervalMs_) {
          // As before, set cadence before work so long requests do not shift it.
          lastRefreshMs_ = now;
          if (hooks_.refreshStarted != nullptr) {
            hooks_.refreshStarted();
          }
          state_ = State::RefreshConnecting;
          hooks_.startConnection(now);
        }
        return;

      case State::RefreshConnecting:
        if (hooks_.isConnected() || !hooks_.connectionPending()) {
          state_ = State::RefreshingCurrentWeather;
        }
        return;

      case State::RefreshingCurrentWeather:
        currentWeatherSucceeded_ = hooks_.fetchCurrentWeather();
        state_ = State::RefreshingForecast;
        return;

      case State::RefreshingForecast:
        hooks_.fetchForecast();
        state_ = State::PreparingRefreshAssets;
        return;

      case State::PreparingRefreshAssets: {
        const View selectedView = hooks_.activeView();
        prepareWeatherAssetsIfAvailable(selectedView);

        if (selectedView == View::Forecast) {
          hooks_.drawForecast();
        } else if (currentWeatherSucceeded_ || hooks_.weatherAvailable()) {
          hooks_.drawWeather();
        } else {
          hooks_.drawError("Refresh failed");
        }

        state_ = State::Ready;
        return;
      }

      case State::Inactive:
      default:
        return;
    }
  }

  // Leaves the application inactive without releasing shared platform
  // resources. Resource lifetime remains with the platform owners.
  void end() {
    active_ = false;
    currentWeatherSucceeded_ = false;
    state_ = State::Inactive;
    hooks_ = Hooks();
  }

  bool isActive() const {
    return active_;
  }

  bool isReady() const {
    return active_ && state_ == State::Ready;
  }

  // Request a user-initiated refresh without waiting for association. The
  // normal update state machine handles the connection and data stages.
  bool requestRefresh() {
    if (!active_ || state_ != State::Ready) {
      return false;
    }

    const uint32_t now = hooks_.millisNow();
    lastRefreshMs_ = now;
    if (hooks_.refreshStarted != nullptr) {
      hooks_.refreshStarted();
    }
    state_ = State::RefreshConnecting;
    hooks_.startConnection(now);
    return true;
  }

  State state() const {
    return state_;
  }

  uint32_t lastRefreshMs() const {
    return lastRefreshMs_;
  }

 private:
  void prepareWeatherAssetsIfAvailable(View selectedView) {
    if (selectedView == View::Weather &&
        hooks_.prepareWeatherAssets != nullptr &&
        hooks_.weatherAvailable()) {
      hooks_.prepareWeatherAssets();
    }
  }

  Hooks hooks_;
  uint32_t refreshIntervalMs_;
  uint32_t clockRefreshIntervalMs_;
  uint32_t lastRefreshMs_;
  uint32_t lastClockRefreshMs_;
  State state_;
  bool active_;
  bool currentWeatherSucceeded_;

  static bool hasRequiredHooks(const Hooks& hooks) {
    return hooks.processInput != nullptr &&
           hooks.syncTime != nullptr &&
           hooks.refreshClock != nullptr &&
           hooks.millisNow != nullptr &&
           hooks.startConnection != nullptr &&
           hooks.connectionPending != nullptr &&
           hooks.isConnected != nullptr &&
           hooks.resolveLocation != nullptr &&
           hooks.fetchCurrentWeather != nullptr &&
           hooks.fetchForecast != nullptr &&
           hooks.weatherAvailable != nullptr &&
           hooks.activeView != nullptr &&
           hooks.drawLoading != nullptr &&
           hooks.drawWeather != nullptr &&
           hooks.drawForecast != nullptr &&
           hooks.drawError != nullptr;
  }
};

#endif  // ESP32_GADGET_WEATHER_APP_H
