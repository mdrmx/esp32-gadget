#include <stdio.h>
#include <stdlib.h>

#include "../../src/apps/ApplicationManager.h"
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
  CHECK(!manager.hasApplication("missing"));
  CHECK(!manager.hasApplication(nullptr));
  CHECK(!manager.hasApplication(""));
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

static uint32_t fakeMillis = 100;
static int weatherInputCalls = 0;
static int weatherSyncCalls = 0;
static int weatherConnectionStarts = 0;
static int weatherEndToEndFetchCalls = 0;

static void weatherProcessInput() { ++weatherInputCalls; }
static void weatherSyncTime() { ++weatherSyncCalls; }
static void weatherRefreshClock() {}
static uint32_t weatherMillisNow() { return fakeMillis; }
static bool weatherStartConnection(uint32_t) {
  ++weatherConnectionStarts;
  return true;
}
static bool weatherConnectionPending() { return false; }
static bool weatherIsConnected() { return true; }
static void weatherResolveLocation() { ++weatherEndToEndFetchCalls; }
static bool weatherFetchCurrent() { ++weatherEndToEndFetchCalls; return true; }
static bool weatherFetchForecast() { ++weatherEndToEndFetchCalls; return true; }
static void weatherPrepareAssets() {}
static bool weatherAvailable() { return true; }
static WeatherApp::View weatherActiveView() { return WeatherApp::View::Weather; }
static void weatherDrawLoading(const char*) {}
static void weatherDrawWeather() {}
static void weatherDrawForecast() {}
static void weatherDrawError(const char*) {}

static WeatherApp::Hooks makeWeatherHooks() {
  WeatherApp::Hooks hooks;
  hooks.processInput = weatherProcessInput;
  hooks.syncTime = weatherSyncTime;
  hooks.refreshClock = weatherRefreshClock;
  hooks.millisNow = weatherMillisNow;
  hooks.startConnection = weatherStartConnection;
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

static bool testWeatherAppUsesSharedLifecycleManager() {
  ApplicationManager manager;
  WeatherApp weather;
  WeatherApp::Hooks hooks = makeWeatherHooks();

  CHECK(strcmp(weather.id(), "weather") == 0);
  CHECK(weather.configure(hooks));
  CHECK(manager.registerApplication(weather));
  CHECK(manager.start("weather"));
  CHECK(weather.isActive());
  CHECK(weather.state() == WeatherApp::State::Connecting);
  CHECK(weatherConnectionStarts == 1);

  manager.update();
  CHECK(weatherInputCalls == 1);
  CHECK(weatherSyncCalls == 1);
  CHECK(weather.state() == WeatherApp::State::ResolvingLocation);

  CHECK(manager.stopActive());
  CHECK(!weather.isActive());
  const int inputCallsAtStop = weatherInputCalls;
  const int syncCallsAtStop = weatherSyncCalls;
  manager.update();
  CHECK(weatherInputCalls == inputCallsAtStop);
  CHECK(weatherSyncCalls == syncCallsAtStop);

  // Weather keeps its injected platform hooks and can be restarted by the
  // manager without reconstructing shared services or the application object.
  CHECK(manager.start("weather"));
  CHECK(weather.isActive());
  CHECK(weather.state() == WeatherApp::State::Connecting);
  CHECK(weatherConnectionStarts == 2);
  manager.stopActive();
  return true;
}

int main() {
  if (!testRegistrationIsBoundedAndRejectsDuplicateIds()) return EXIT_FAILURE;
  if (!testStartUpdateSwitchAndStopLifecycle()) return EXIT_FAILURE;
  if (!testFailedSwitchRestoresPreviousApplication()) return EXIT_FAILURE;
  if (!testWeatherAppUsesSharedLifecycleManager()) return EXIT_FAILURE;

  printf("ApplicationManager tests passed.\n");
  return EXIT_SUCCESS;
}
