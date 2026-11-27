#include <stdio.h>
#include <stdlib.h>

#include "../../src/apps/ApplicationManager.h"
#include "../../src/apps/ApplicationLauncherController.h"
#include "../../src/apps/weather/WeatherApp.h"

static void check(bool condition, const char* expression, int line) {
  if (!condition) {
    fprintf(stderr, "FAIL: %s (line %d)\n", expression, line);
    exit(EXIT_FAILURE);
  }
}

#define CHECK(expression) check((expression), #expression, __LINE__)

class FakeApplication : public Application {
 public:
  FakeApplication(const char* applicationId, bool beginResult = true)
      : applicationId_(applicationId),
        beginResult_(beginResult),
        beginCalls_(0),
        updateCalls_(0),
        endCalls_(0) {}

  const char* id() const override {
    return applicationId_;
  }

  bool begin() override {
    ++beginCalls_;
    return beginResult_;
  }

  void update() override {
    ++updateCalls_;
  }

  void end() override {
    ++endCalls_;
  }

  void setBeginResult(bool result) {
    beginResult_ = result;
  }

  void setId(const char* applicationId) {
    applicationId_ = applicationId;
  }

  int beginCalls() const { return beginCalls_; }
  int updateCalls() const { return updateCalls_; }
  int endCalls() const { return endCalls_; }

 private:
  const char* applicationId_;
  bool beginResult_;
  int beginCalls_;
  int updateCalls_;
  int endCalls_;
};

static bool testRegistrationIsBoundedAndRejectsDuplicateIds() {
  ApplicationManager manager;
  FakeApplication weather("weather");
  FakeApplication maps("maps");
  FakeApplication settings("settings");
  FakeApplication diagnostics("diagnostics");
  FakeApplication duplicateWeather("weather");
  FakeApplication nullId(nullptr);
  FakeApplication emptyId("");
  FakeApplication overflow("overflow");

  CHECK(!manager.registerApplication(nullId));
  CHECK(!manager.registerApplication(emptyId));
  CHECK(manager.registerApplication(weather));
  CHECK(manager.registerApplication(maps));
  CHECK(manager.registerApplication(settings));
  CHECK(manager.registerApplication(diagnostics));
  CHECK(!manager.registerApplication(duplicateWeather));
  CHECK(!manager.registerApplication(overflow));
  CHECK(manager.registeredCount() == ApplicationManager::MaxApplications);
  CHECK(manager.hasApplication("weather"));
  CHECK(manager.hasApplication("maps"));
  CHECK(manager.applicationAt(0) == &weather);
  CHECK(manager.applicationAt(1) == &maps);
  CHECK(manager.applicationAt(ApplicationManager::MaxApplications) == nullptr);
  CHECK(!manager.hasApplication("missing"));
  CHECK(!manager.hasApplication(nullptr));
  CHECK(!manager.hasApplication(""));
  return true;
}

static bool testChangingRegisteredIdToNullIsSafe() {
  ApplicationManager manager;
  FakeApplication unstable("weather");
  FakeApplication maps("maps");

  CHECK(manager.registerApplication(unstable));
  CHECK(manager.registeredCount() == 1);

  // The id() contract should be stable, but the manager must remain safe if
  // an implementation violates it after registration.
  unstable.setId(nullptr);
  CHECK(!manager.hasApplication("weather"));
  CHECK(!manager.start("weather"));
  CHECK(manager.activeApplication() == nullptr);

  // A null id from an existing registration must not break duplicate checks
  // or prevent other valid applications from being registered and found.
  CHECK(manager.registerApplication(maps));
  CHECK(manager.registeredCount() == 2);
  CHECK(manager.hasApplication("maps"));
  CHECK(manager.start("maps"));
  CHECK(manager.activeApplication() == &maps);
  manager.update();
  CHECK(maps.updateCalls() == 1);
  return true;
}

static bool testStartUpdateSwitchAndStopLifecycle() {
  ApplicationManager manager;
  FakeApplication weather("weather");
  FakeApplication maps("maps");

  CHECK(manager.registerApplication(weather));
  CHECK(manager.registerApplication(maps));
  CHECK(!manager.start("missing"));
  CHECK(manager.activeApplication() == nullptr);
  manager.update();
  CHECK(weather.updateCalls() == 0);
  CHECK(maps.updateCalls() == 0);

  CHECK(manager.start("weather"));
  CHECK(manager.activeApplication() == &weather);
  CHECK(manager.activeId() != nullptr);
  CHECK(strcmp(manager.activeId(), "weather") == 0);
  CHECK(weather.beginCalls() == 1);

  // Starting the same app is idempotent; starting a different app directly
  // must not silently displace the current application.
  CHECK(manager.start("weather"));
  CHECK(weather.beginCalls() == 1);
  CHECK(!manager.start("maps"));
  CHECK(manager.activeApplication() == &weather);

  manager.update();
  CHECK(weather.updateCalls() == 1);
  CHECK(maps.updateCalls() == 0);

  // Unknown targets are rejected before the active app is stopped.
  CHECK(!manager.switchTo("missing"));
  CHECK(manager.activeApplication() == &weather);
  CHECK(weather.endCalls() == 0);

  CHECK(manager.switchTo("maps"));
  CHECK(weather.endCalls() == 1);
  CHECK(maps.beginCalls() == 1);
  CHECK(manager.activeApplication() == &maps);
  manager.update();
  CHECK(weather.updateCalls() == 1);
  CHECK(maps.updateCalls() == 1);

  CHECK(manager.switchTo("maps"));
  CHECK(maps.beginCalls() == 1);
  CHECK(manager.stopActive());
  CHECK(maps.endCalls() == 1);
  CHECK(manager.activeApplication() == nullptr);
  CHECK(manager.activeId() == nullptr);
  manager.update();
  CHECK(maps.updateCalls() == 1);
  CHECK(!manager.stopActive());
  return true;
}

static bool testFailedSwitchRestoresPreviousApplication() {
  ApplicationManager manager;
  FakeApplication weather("weather");
  FakeApplication maps("maps", false);

  CHECK(manager.registerApplication(weather));
  CHECK(manager.registerApplication(maps));
  CHECK(manager.start("weather"));

  CHECK(!manager.switchTo("maps"));
  CHECK(weather.endCalls() == 1);
  CHECK(maps.beginCalls() == 1);
  CHECK(weather.beginCalls() == 2);
  CHECK(manager.activeApplication() == &weather);
  CHECK(strcmp(manager.activeId(), "weather") == 0);

  manager.update();
  CHECK(weather.updateCalls() == 1);
  CHECK(maps.updateCalls() == 0);

  // If both the requested app and the previous app fail to begin, the manager
  // must not retain a stale active pointer.
  weather.setBeginResult(false);
  CHECK(!manager.switchTo("maps"));
  CHECK(weather.endCalls() == 2);
  CHECK(weather.beginCalls() == 3);
  CHECK(manager.activeApplication() == nullptr);
  CHECK(manager.activeId() == nullptr);
  CHECK(!manager.stopActive());
  CHECK(!manager.start("maps"));
  return true;
}


static bool testLauncherControllerSelectsAndRestoresApplications() {
  ApplicationManager manager;
  FakeApplication weather("weather");
  FakeApplication maps("maps");

  CHECK(manager.registerApplication(weather));
  CHECK(manager.registerApplication(maps));
  CHECK(manager.start("weather"));

  ApplicationLauncherController launcher(manager);
  CHECK(!launcher.isOpen());
  CHECK(launcher.open());
  CHECK(launcher.isOpen());
  CHECK(strcmp(launcher.returnApplicationId(), "weather") == 0);
  CHECK(manager.activeApplication() == nullptr);
  CHECK(weather.endCalls() == 1);
  CHECK(!launcher.open());
  CHECK(!launcher.select(2));

  CHECK(launcher.select(1));
  CHECK(!launcher.isOpen());
  CHECK(manager.activeApplication() == &maps);
  CHECK(maps.beginCalls() == 1);

  // A failed app start leaves the launcher open and BACK restores the app
  // that was active before opening the launcher.
  weather.setBeginResult(false);
  CHECK(launcher.open());
  CHECK(strcmp(launcher.returnApplicationId(), "maps") == 0);
  CHECK(!launcher.select(0));
  CHECK(launcher.isOpen());
  CHECK(manager.activeApplication() == nullptr);
  CHECK(launcher.back());
  CHECK(!launcher.isOpen());
  CHECK(manager.activeApplication() == &maps);
  CHECK(maps.beginCalls() == 2);

  // Opening with no active app is safe; BACK simply closes without inventing
  // an application to start.
  CHECK(manager.stopActive());
  CHECK(launcher.open());
  CHECK(launcher.returnApplicationId() == nullptr);
  CHECK(launcher.back());
  CHECK(manager.activeApplication() == nullptr);
  return true;
}

static uint32_t fakeMillis = 100;
static int weatherInputCalls = 0;
static int weatherEndToEndFetchCalls = 0;
static bool weatherConnected = true;
static bool weatherDataAvailable = true;
static int weatherDrawLoadingCalls = 0;
static int weatherDrawWeatherCalls = 0;
static int weatherDrawForecastCalls = 0;
static const char* weatherLastLoadingMessage = nullptr;

static void weatherProcessInput() { ++weatherInputCalls; }
static uint32_t weatherMillisNow() { return fakeMillis; }
static bool weatherConnectionPending() { return false; }
static bool weatherIsConnected() { return weatherConnected; }
static void weatherResolveLocation() { ++weatherEndToEndFetchCalls; }
static bool weatherFetchCurrent() { ++weatherEndToEndFetchCalls; return true; }
static void weatherFetchForecast() { ++weatherEndToEndFetchCalls; }
static void weatherPrepareAssets() {}
static bool weatherAvailable() { return weatherDataAvailable; }
static WeatherApp::View weatherActiveView() { return WeatherApp::View::Weather; }
static void weatherDrawLoading(const char* message) {
  ++weatherDrawLoadingCalls;
  weatherLastLoadingMessage = message;
}
static void weatherDrawWeather() { ++weatherDrawWeatherCalls; }
static void weatherDrawForecast() { ++weatherDrawForecastCalls; }
static void weatherDrawError(const char*) {}

static WeatherApp::Hooks makeWeatherHooks() {
  WeatherApp::Hooks hooks;
  hooks.processInput = weatherProcessInput;
  hooks.millisNow = weatherMillisNow;
  hooks.connectionPending = weatherConnectionPending;
  hooks.isConnected = weatherIsConnected;
  hooks.resolveLocation = weatherResolveLocation;
  hooks.fetchCurrentWeather = weatherFetchCurrent;
  hooks.fetchForecast = weatherFetchForecast;
  hooks.prepareWeatherAssets = weatherPrepareAssets;
  hooks.weatherAvailable = weatherAvailable;
  hooks.activeView = weatherActiveView;
  hooks.drawLoading = weatherDrawLoading;
  hooks.drawWeather = weatherDrawWeather;
  hooks.drawForecast = weatherDrawForecast;
  hooks.drawError = weatherDrawError;
  return hooks;
}

static bool testWeatherAppResumesWithoutReconnectingWhenOnline() {
  ApplicationManager manager;
  WeatherApp weather;
  WeatherApp::Hooks hooks = makeWeatherHooks();

  fakeMillis = 100;
  weatherInputCalls = 0;
  weatherEndToEndFetchCalls = 0;
  weatherConnected = true;
  weatherDrawLoadingCalls = 0;
  weatherDrawWeatherCalls = 0;
  weatherDrawForecastCalls = 0;
  weatherLastLoadingMessage = nullptr;

  CHECK(strcmp(weather.id(), "weather") == 0);
  CHECK(weather.configure(hooks));
  CHECK(manager.registerApplication(weather));
  CHECK(manager.start("weather"));
  CHECK(weather.isActive());
  CHECK(weather.state() == WeatherApp::State::ResolvingLocation);
  CHECK(weatherDrawLoadingCalls == 0);
  CHECK(weatherDrawWeatherCalls == 1);

  // Advance one startup stage, then open the launcher. The suspended stage and
  // platform connection are retained while the app is inactive.
  manager.update();
  CHECK(weatherInputCalls == 1);
  CHECK(weather.state() == WeatherApp::State::FetchingCurrentWeather);

  const uint32_t lastRefreshBeforeStop = weather.lastRefreshMs();
  CHECK(manager.stopActive());
  CHECK(!weather.isActive());
  CHECK(weather.state() == WeatherApp::State::FetchingCurrentWeather);
  const int inputCallsAtStop = weatherInputCalls;
  manager.update();
  CHECK(weatherInputCalls == inputCallsAtStop);

  // Returning from the launcher resumes Weather's existing state and redraws
  // its cached view. It must not start Wi-Fi association or show a connecting
  // banner when the shared platform link is already established.
  CHECK(manager.start("weather"));
  CHECK(weather.isActive());
  CHECK(weather.state() == WeatherApp::State::FetchingCurrentWeather);
  CHECK(weatherDrawLoadingCalls == 0);
  CHECK(weatherDrawWeatherCalls == 2);
  CHECK(weather.lastRefreshMs() == lastRefreshBeforeStop);

  manager.update();
  CHECK(weather.state() == WeatherApp::State::FetchingForecast);
  CHECK(manager.stopActive());
  return true;
}

static bool testReadyWeatherRestoresCacheWhenOffline() {
  ApplicationManager manager;
  WeatherApp weather;
  WeatherApp::Hooks hooks = makeWeatherHooks();

  fakeMillis = 1000;
  weatherConnected = true;
  weatherDataAvailable = false;
  weatherDrawLoadingCalls = 0;
  weatherDrawWeatherCalls = 0;
  weatherDrawForecastCalls = 0;
  weatherLastLoadingMessage = nullptr;

  CHECK(weather.configure(hooks));
  CHECK(manager.registerApplication(weather));
  CHECK(manager.start("weather"));
  for (int i = 0; i < 3; ++i) manager.update();
  CHECK(weather.state() == WeatherApp::State::PreparingStartupAssets);
  weatherDataAvailable = true; // The initial fetch has populated the cache.
  manager.update();
  CHECK(weather.state() == WeatherApp::State::Ready);
  CHECK(weatherDrawWeatherCalls == 1);
  const uint32_t lastRefresh = weather.lastRefreshMs();
  const int loadingCallsBeforeResume = weatherDrawLoadingCalls;

  CHECK(manager.stopActive());
  weatherConnected = false;
  CHECK(manager.start("weather"));
  CHECK(weather.state() == WeatherApp::State::Ready);
  CHECK(weatherDrawWeatherCalls == 2);
  // The initial cold start legitimately showed a loading message. Resuming
  // with cached data must not add another loading message while offline.
  CHECK(weatherDrawLoadingCalls == loadingCallsBeforeResume);
  CHECK(weather.lastRefreshMs() == lastRefresh);
  CHECK(manager.stopActive());
  return true;
}

static bool testRefreshConnectingResumeKeepsCachedWeatherVisible() {
  ApplicationManager manager;
  WeatherApp weather;
  WeatherApp::Hooks hooks = makeWeatherHooks();

  fakeMillis = 2000;
  weatherConnected = true;
  weatherDrawLoadingCalls = 0;
  weatherDrawWeatherCalls = 0;
  weatherDrawForecastCalls = 0;

  CHECK(weather.configure(hooks));
  CHECK(manager.registerApplication(weather));
  CHECK(manager.start("weather"));
  for (int i = 0; i < 4; ++i) manager.update();
  CHECK(weather.isReady());
  CHECK(weather.requestRefresh());
  CHECK(weather.state() == WeatherApp::State::RefreshConnecting);

  CHECK(manager.stopActive());
  weatherConnected = false;
  CHECK(manager.start("weather"));
  CHECK(weather.state() == WeatherApp::State::RefreshConnecting);
  CHECK(weatherDrawWeatherCalls == 2);
  CHECK(weatherDrawLoadingCalls == 0);
  CHECK(manager.stopActive());
  return true;
}

static bool testLostLinkDuringRefreshRestoresCachedWeather() {
  ApplicationManager manager;
  WeatherApp weather;
  WeatherApp::Hooks hooks = makeWeatherHooks();

  fakeMillis = 3000;
  weatherConnected = true;
  weatherDrawLoadingCalls = 0;
  weatherDrawWeatherCalls = 0;
  weatherDrawForecastCalls = 0;

  CHECK(weather.configure(hooks));
  CHECK(manager.registerApplication(weather));
  CHECK(manager.start("weather"));
  for (int i = 0; i < 4; ++i) manager.update();
  CHECK(weather.isReady());
  CHECK(weather.requestRefresh());
  manager.update();
  CHECK(weather.state() == WeatherApp::State::RefreshingCurrentWeather);

  CHECK(manager.stopActive());
  weatherConnected = false;
  CHECK(manager.start("weather"));
  CHECK(weather.state() == WeatherApp::State::RefreshConnecting);
  CHECK(weatherDrawWeatherCalls == 2);
  CHECK(weatherDrawLoadingCalls == 0);
  CHECK(manager.stopActive());
  return true;
}

static bool testWeatherAppWaitsForPlatformConnectionWhenOffline() {
  ApplicationManager manager;
  WeatherApp weather;
  WeatherApp::Hooks hooks = makeWeatherHooks();

  fakeMillis = 500;
  weatherInputCalls = 0;
  weatherEndToEndFetchCalls = 0;
  weatherConnected = false;
  weatherDrawLoadingCalls = 0;
  weatherDrawWeatherCalls = 0;
  weatherDrawForecastCalls = 0;
  weatherLastLoadingMessage = nullptr;

  CHECK(weather.configure(hooks));
  CHECK(manager.registerApplication(weather));
  CHECK(manager.start("weather"));
  CHECK(weather.isActive());
  CHECK(weather.state() == WeatherApp::State::Connecting);
  CHECK(weatherDrawLoadingCalls == 1);
  CHECK(weatherLastLoadingMessage != nullptr);
  CHECK(strcmp(weatherLastLoadingMessage, "Waiting for Wi-Fi...") == 0);

  // The connection manager brings the link up while the app is active; the
  // next update advances directly into the location stage.
  weatherConnected = true;
  manager.update();
  CHECK(weather.state() == WeatherApp::State::ResolvingLocation);
  manager.stopActive();
  return true;
}

int main() {
  if (!testRegistrationIsBoundedAndRejectsDuplicateIds()) return EXIT_FAILURE;
  if (!testChangingRegisteredIdToNullIsSafe()) return EXIT_FAILURE;
  if (!testStartUpdateSwitchAndStopLifecycle()) return EXIT_FAILURE;
  if (!testFailedSwitchRestoresPreviousApplication()) return EXIT_FAILURE;
  if (!testLauncherControllerSelectsAndRestoresApplications()) return EXIT_FAILURE;
  if (!testWeatherAppResumesWithoutReconnectingWhenOnline()) return EXIT_FAILURE;
  if (!testReadyWeatherRestoresCacheWhenOffline()) return EXIT_FAILURE;
  if (!testRefreshConnectingResumeKeepsCachedWeatherVisible()) return EXIT_FAILURE;
  if (!testLostLinkDuringRefreshRestoresCachedWeather()) return EXIT_FAILURE;
  if (!testWeatherAppWaitsForPlatformConnectionWhenOffline()) return EXIT_FAILURE;

  printf("ApplicationManager tests passed.\n");
  return EXIT_SUCCESS;
}
