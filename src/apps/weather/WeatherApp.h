#ifndef ESP32_GADGET_WEATHER_APP_H
#define ESP32_GADGET_WEATHER_APP_H

#include <stdint.h>

#include "../Application.h"

// Coordinates the Weather application's startup, update cadence and recurring
// work. Connectivity is platform-owned and injected as a small status/start
// interface; this class never includes Arduino or Wi-Fi headers.
class WeatherApp : public Application {
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

  const char* id() const override {
    return "weather";
  }

  // Bind long-lived platform adapters before registering the application.
  // The hooks are retained across end()/begin() so the manager can switch
  // away and back without rebuilding platform-owned services.
  bool configure(const Hooks& hooks) {
    if (active_ || !hasRequiredHooks(hooks)) {
      return false;
    }
    hooks_ = hooks;
    return true;
  }

  // Starts Weather or resumes its previous in-memory state after the
  // platform launcher has temporarily stopped app updates. A live platform
  // connection is reused; begin() never restarts an already-established link.
  // No association wait, location lookup or HTTP request runs here.
  bool begin() override {
    if (active_ || !hasRequiredHooks(hooks_)) {
      return false;
    }

    const bool resuming = state_ != State::Inactive;
    const State previousState = state_;
    const uint32_t now = hooks_.millisNow();
    const bool connected = hooks_.isConnected();

    active_ = true;
    lastClockRefreshMs_ = now;

    if (!resuming) {
      currentWeatherSucceeded_ = false;
      lastRefreshMs_ = now;

      if (connected) {
        state_ = State::ResolvingLocation;
        drawCachedViewOrLoading("Finding your location...");
      } else {
        state_ = State::Connecting;
        hooks_.drawLoading("Connecting Wi-Fi...");
        hooks_.startConnection(now);
      }
      return true;
    }

    // Returning to an already-ready Weather app should reveal its cached view
    // immediately and retain the existing refresh schedule.
    if (previousState == State::Ready && connected) {
      drawActiveView();
      return true;
    }

    // If the app was stopped while its initial or refresh connection was
    // pending, reuse that pending attempt. If the link is already up, advance
    // directly to the first data stage instead of displaying a false banner.
    if (previousState == State::Connecting ||
        previousState == State::RefreshConnecting) {
      if (connected) {
        state_ = previousState == State::Connecting
            ? State::ResolvingLocation
            : State::RefreshingCurrentWeather;
        drawCachedViewOrLoading(
            previousState == State::Connecting
                ? "Finding your location..."
                : "Refreshing weather...");
      } else {
        state_ = previousState;
        hooks_.drawLoading("Connecting Wi-Fi...");
        if (!hooks_.connectionPending()) {
          hooks_.startConnection(now);
        }
      }
      return true;
    }

    // A link can disappear while the launcher is open. Restart only the
    // relevant connection stage, then recover the workflow from its safe
    // beginning; do not tear down or reconfigure the platform Wi-Fi service.
    if (!connected) {
      const bool refreshFlow =
          previousState == State::RefreshingCurrentWeather ||
          previousState == State::RefreshingForecast ||
          previousState == State::PreparingRefreshAssets;
      state_ = refreshFlow ? State::RefreshConnecting : State::Connecting;
      hooks_.drawLoading("Connecting Wi-Fi...");
      if (!hooks_.connectionPending()) {
        hooks_.startConnection(now);
      }
      return true;
    }

    state_ = previousState;
    drawCachedViewOrLoading(
        previousState == State::ResolvingLocation
            ? "Finding your location..."
            : "Updating weather...");
    return true;
  }

  // Compatibility convenience for existing callers and focused app tests.
  bool begin(const Hooks& hooks) {
    return configure(hooks) && begin();
  }

  // Called from the device loop. While associating, input, clock and other
  // loop-level work continue on every pass. Each startup/refresh stage is
  // advanced separately rather than waiting for connectivity here.
  //
  // Location lookup, HTTP requests and asset preparation are still synchronous
  // operations when their individual stages run. Their transport-level
  // asynchronous conversion is intentionally a separate follow-up.
  void update() override {
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

  // Suspends app updates without releasing shared platform resources or
  // discarding Weather's current stage, cached data or refresh schedule.
  // Re-entering begin() resumes this state against the live platform services.
  void end() override {
    active_ = false;
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
  void drawActiveView() {
    if (hooks_.activeView() == View::Forecast) {
      hooks_.drawForecast();
    } else {
      hooks_.drawWeather();
    }
  }

  void drawCachedViewOrLoading(const char* loadingMessage) {
    if (hooks_.weatherAvailable()) {
      drawActiveView();
    } else if (loadingMessage != nullptr) {
      hooks_.drawLoading(loadingMessage);
    }
  }

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
