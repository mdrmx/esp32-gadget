#ifndef ESP32_GADGET_WEATHER_APP_H
#define ESP32_GADGET_WEATHER_APP_H

#include <stdint.h>

#include "../Application.h"

// Coordinates the Weather application's startup, update cadence and recurring
// work. Connectivity is platform-owned; Weather observes shared link state but
// does not initiate Wi-Fi association. This class has no Arduino/Wi-Fi headers.
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
    uint32_t (*millisNow)();

    // ConnectivityManager remains owned and advanced by the platform.
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
          millisNow(nullptr),
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

  explicit WeatherApp(
      uint32_t refreshIntervalMs = DefaultRefreshIntervalMs)
      : hooks_(),
        refreshIntervalMs_(refreshIntervalMs),
        lastRefreshMs_(0),
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

    if (!resuming) {
      currentWeatherSucceeded_ = false;
      lastRefreshMs_ = now;

      if (connected) {
        state_ = State::ResolvingLocation;
        drawCachedViewOrLoading("Finding your location...");
      } else {
        // The platform launcher starts the shared boot-time association.
        // Weather waits for that service and never initiates Wi-Fi itself.
        state_ = State::Connecting;
        hooks_.drawLoading("Waiting for Wi-Fi...");
      }
      return true;
    }

    // A ready app always restores its cached view, even if the shared link
    // dropped while it was suspended in the launcher. The platform connectivity
    // manager owns any background recovery; Weather keeps its refresh cadence.
    if (previousState == State::Ready) {
      drawCachedViewOrError("Weather unavailable");
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
        drawCachedViewOrLoading("Connecting Wi-Fi...");
      }
      return true;
    }

    // A link can disappear while the launcher is open. Preserve the relevant
    // connection-wait stage and let the platform connectivity manager recover
    // in the background; Weather never starts or reconfigures Wi-Fi.
    if (!connected) {
      const bool refreshFlow =
          previousState == State::RefreshingCurrentWeather ||
          previousState == State::RefreshingForecast ||
          previousState == State::PreparingRefreshAssets;
      state_ = refreshFlow ? State::RefreshConnecting : State::Connecting;
      drawCachedViewOrLoading("Connecting Wi-Fi...");
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

  // Called from the device loop while Weather is active. Platform connectivity,
  // time synchronisation, shared-header refresh and touch sampling are advanced
  // independently by loop(), regardless of the active application. Each
  // Weather startup/refresh stage advances separately without waiting here.
  //
  // Location lookup, HTTP requests and asset preparation are still synchronous
  // operations when their individual stages run. Their transport-level
  // asynchronous conversion is intentionally a separate follow-up.
  void update() override {
    if (!active_) {
      return;
    }

    hooks_.processInput();

    const uint32_t now = hooks_.millisNow();

    // Modal platform settings pause Weather's staged fetch/render workflow.
    // The platform loop continues to own shared time and header-clock updates.
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

  void drawCachedViewOrError(const char* errorMessage) {
    if (hooks_.weatherAvailable()) {
      drawActiveView();
    } else if (errorMessage != nullptr) {
      hooks_.drawError(errorMessage);
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
  uint32_t lastRefreshMs_;
  State state_;
  bool active_;
  bool currentWeatherSucceeded_;

  static bool hasRequiredHooks(const Hooks& hooks) {
    return hooks.processInput != nullptr &&
           hooks.millisNow != nullptr &&
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
