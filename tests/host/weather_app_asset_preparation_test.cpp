#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/apps/weather/WeatherApp.h"

static char events[128];
static size_t eventCount = 0;
static uint32_t clockMs = 100;
static bool available = true;
static WeatherApp::View view = WeatherApp::View::Weather;
static bool fetchResult = true;
static bool connected = false;
static bool connectionPending = true;

static void record(char event) {
  if (eventCount < sizeof(events) - 1) {
    events[eventCount++] = event;
    events[eventCount] = '\0';
  }
}

static void clearEvents() {
  eventCount = 0;
  events[0] = '\0';
}

static void resetEnvironment() {
  clearEvents();
  clockMs = 100;
  available = true;
  view = WeatherApp::View::Weather;
  fetchResult = true;
  connected = false;
  connectionPending = true;
}

static void processInput() { record('I'); }
static uint32_t millisNow() { return clockMs; }

static bool isConnectionPending() { return connectionPending; }
static bool isConnected() { return connected; }
static void resolveLocation() { record('L'); }

static bool fetchCurrentWeather() {
  record('W');
  return fetchResult;
}

static void fetchForecast() {
  record('F');
}

static void prepareWeatherAssets() { record('A'); }
static bool weatherAvailable() { return available; }
static WeatherApp::View activeView() { return view; }
static void drawLoading(const char*) { record('G'); }
static void drawWeather() { record('D'); }
static void drawForecast() { record('R'); }
static void drawError(const char*) { record('E'); }
static void refreshStarted() { record('X'); }

static WeatherApp::Hooks makeHooks(bool includeAssetHook = true) {
  WeatherApp::Hooks hooks;
  hooks.processInput = processInput;
  hooks.millisNow = millisNow;
  hooks.connectionPending = isConnectionPending;
  hooks.isConnected = isConnected;
  hooks.resolveLocation = resolveLocation;
  hooks.fetchCurrentWeather = fetchCurrentWeather;
  hooks.fetchForecast = fetchForecast;
  hooks.prepareWeatherAssets = includeAssetHook
    ? prepareWeatherAssets
    : nullptr;
  hooks.weatherAvailable = weatherAvailable;
  hooks.activeView = activeView;
  hooks.drawLoading = drawLoading;
  hooks.drawWeather = drawWeather;
  hooks.drawForecast = drawForecast;
  hooks.drawError = drawError;
  hooks.refreshStarted = refreshStarted;
  return hooks;
}

static bool expectEvents(const char* expected, const char* label) {
  if (strcmp(events, expected) == 0) {
    return true;
  }

  fprintf(stderr, "FAIL: %s events were %s, expected %s\n",
          label, events, expected);
  return false;
}

static bool completeSuccessfulStartup(WeatherApp& app) {
  connected = true;
  connectionPending = false;
  app.update(); // Connecting -> ResolvingLocation.
  app.update(); // Resolve location.
  app.update(); // Current weather.
  app.update(); // Forecast.
  app.update(); // Assets + first render.
  return app.isReady();
}

static bool testStartupAdvancesWithoutWaitingForConnection() {
  resetEnvironment();
  WeatherApp app(50);

  if (!app.begin(makeHooks())) {
    fprintf(stderr, "FAIL: async startup begin\n");
    return false;
  }
  if (!expectEvents("G", "startup begin")) return false;
  if (app.state() != WeatherApp::State::Connecting) {
    fprintf(stderr, "FAIL: startup did not wait in Connecting\n");
    return false;
  }

  clearEvents();
  app.update();
  if (!expectEvents("I", "pending connection loop")) return false;
  if (app.state() != WeatherApp::State::Connecting) {
    fprintf(stderr, "FAIL: app left Connecting while connection was pending\n");
    return false;
  }

  clearEvents();
  connected = true;
  connectionPending = false;
  app.update();
  if (!expectEvents("I", "connection completed")) return false;
  if (app.state() != WeatherApp::State::ResolvingLocation) {
    fprintf(stderr, "FAIL: connection completion did not advance to location\n");
    return false;
  }

  clearEvents();
  app.update();
  if (!expectEvents("IL", "location stage")) return false;
  if (app.state() != WeatherApp::State::FetchingCurrentWeather) {
    fprintf(stderr, "FAIL: location stage did not advance to current weather\n");
    return false;
  }

  clearEvents();
  app.update();
  if (!expectEvents("IW", "current weather stage")) return false;
  if (app.state() != WeatherApp::State::FetchingForecast) {
    fprintf(stderr, "FAIL: current weather stage did not advance to forecast\n");
    return false;
  }

  clearEvents();
  app.update();
  if (!expectEvents("IF", "forecast stage")) return false;
  if (app.state() != WeatherApp::State::PreparingStartupAssets) {
    fprintf(stderr, "FAIL: forecast stage did not advance to assets\n");
    return false;
  }

  clearEvents();
  app.update();
  if (!expectEvents("IAD", "startup render")) return false;
  if (!app.isReady()) {
    fprintf(stderr, "FAIL: startup did not become Ready\n");
    return false;
  }
  if (app.lastRefreshMs() != clockMs) {
    fprintf(stderr, "FAIL: refresh cadence was not anchored after startup\n");
    return false;
  }

  app.end();
  if (app.state() != WeatherApp::State::Ready || app.isActive()) {
    fprintf(stderr, "FAIL: end did not suspend while retaining app state\n");
    return false;
  }
  return true;
}

static bool testUnavailableNetworkFallsBackWithoutWaiting() {
  resetEnvironment();
  connectionPending = false;
  connected = false;
  available = false;

  WeatherApp app(50);
  if (!app.begin(makeHooks())) {
    fprintf(stderr, "FAIL: offline startup begin\n");
    return false;
  }

  clearEvents();
  app.update();
  if (app.state() != WeatherApp::State::ResolvingLocation) {
    fprintf(stderr, "FAIL: failed connection did not progress to fallback location\n");
    return false;
  }

  clearEvents();
  app.update();
  if (!expectEvents("IL", "offline location fallback")) return false;

  clearEvents();
  app.update();
  if (!expectEvents("IW", "offline current weather")) return false;

  clearEvents();
  app.update();
  if (!expectEvents("IF", "offline forecast")) return false;

  clearEvents();
  app.update();
  if (!expectEvents("IE", "offline startup error")) return false;
  if (!app.isReady()) {
    fprintf(stderr, "FAIL: offline startup did not settle into Ready\n");
    return false;
  }
  return true;
}

static bool testRefreshWaitsForConnectionAndPreservesViewOrdering() {
  resetEnvironment();
  WeatherApp app(50);

  if (!app.begin(makeHooks())) {
    fprintf(stderr, "FAIL: refresh app begin\n");
    return false;
  }
  if (!completeSuccessfulStartup(app)) {
    fprintf(stderr, "FAIL: refresh app startup did not complete\n");
    return false;
  }

  clearEvents();
  view = WeatherApp::View::Forecast;
  connected = false;
  connectionPending = true;
  clockMs = 151;
  app.update();
  if (!expectEvents("IX", "refresh connection wait")) return false;
  if (app.state() != WeatherApp::State::RefreshConnecting) {
    fprintf(stderr, "FAIL: refresh did not enter its connection wait stage\n");
    return false;
  }

  clearEvents();
  app.update();
  if (!expectEvents("I", "refresh pending connection")) return false;
  if (app.state() != WeatherApp::State::RefreshConnecting) {
    fprintf(stderr, "FAIL: refresh advanced before network connection completed\n");
    return false;
  }

  clearEvents();
  connected = true;
  connectionPending = false;
  app.update();
  if (!expectEvents("I", "refresh connection completed")) return false;

  clearEvents();
  app.update();
  if (!expectEvents("IW", "refresh current weather")) return false;

  clearEvents();
  app.update();
  if (!expectEvents("IF", "refresh forecast")) return false;

  clearEvents();
  app.update();
  if (!expectEvents("IR", "forecast render without weather asset fetch")) return false;
  if (!app.isReady()) {
    fprintf(stderr, "FAIL: refresh did not return to Ready\n");
    return false;
  }
  return true;
}

static bool testWeatherRefreshPreparesAssetsAndRetainsStaleWeather() {
  resetEnvironment();
  WeatherApp app(50);
  if (!app.begin(makeHooks())) {
    fprintf(stderr, "FAIL: Weather refresh app begin\n");
    return false;
  }
  if (!completeSuccessfulStartup(app)) {
    fprintf(stderr, "FAIL: Weather refresh startup did not complete\n");
    return false;
  }

  clearEvents();
  view = WeatherApp::View::Weather;
  clockMs = 151;
  app.update(); // Start the periodic refresh.
  connected = true;
  connectionPending = false;
  app.update(); // Advance past connection state.
  clearEvents();

  fetchResult = false;
  available = true; // Last known weather remains usable.
  app.update(); // Fetch current weather.
  if (!expectEvents("IW", "Weather refresh current fetch")) return false;

  clearEvents();
  app.update(); // Fetch forecast.
  if (!expectEvents("IF", "Weather refresh forecast fetch")) return false;

  clearEvents();
  app.update(); // Prepare icon and redraw stale-but-available Weather.
  if (!expectEvents("IAD", "Weather refresh asset/render")) return false;
  return app.isReady();
}

int main() {
  if (!testStartupAdvancesWithoutWaitingForConnection()) return EXIT_FAILURE;
  if (!testUnavailableNetworkFallsBackWithoutWaiting()) return EXIT_FAILURE;
  if (!testRefreshWaitsForConnectionAndPreservesViewOrdering()) return EXIT_FAILURE;
  if (!testWeatherRefreshPreparesAssetsAndRetainsStaleWeather()) return EXIT_FAILURE;

  printf("WeatherApp state and asset preparation tests passed.\n");
  return EXIT_SUCCESS;
}
