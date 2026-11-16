#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/apps/weather/WeatherApp.h"

static char events[64];
static size_t eventCount = 0;
static uint32_t clockMs = 100;
static bool available = true;
static WeatherApp::View view = WeatherApp::View::Weather;
static bool fetchResult = true;

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

static void processInput() { record('I'); }
static void syncTime() { record('S'); }
static void refreshClock() { record('C'); }
static uint32_t millisNow() { return clockMs; }
static bool fetchCurrentWeather() {
  record('W');
  return fetchResult;
}
static void fetchForecast() { record('F'); }
static void prepareWeatherAssets() { record('A'); }
static bool weatherAvailable() { return available; }
static WeatherApp::View activeView() { return view; }
static void drawWeather() { record('D'); }
static void drawForecast() { record('R'); }
static void drawError(const char*) { record('E'); }
static void refreshStarted() { record('X'); }

static WeatherApp::Hooks makeHooks(bool includeAssetHook) {
  WeatherApp::Hooks hooks;
  hooks.processInput = processInput;
  hooks.syncTime = syncTime;
  hooks.refreshClock = refreshClock;
  hooks.millisNow = millisNow;
  hooks.fetchCurrentWeather = fetchCurrentWeather;
  hooks.fetchForecast = fetchForecast;
  hooks.prepareWeatherAssets = includeAssetHook
    ? prepareWeatherAssets
    : nullptr;
  hooks.weatherAvailable = weatherAvailable;
  hooks.activeView = activeView;
  hooks.drawWeather = drawWeather;
  hooks.drawForecast = drawForecast;
  hooks.drawError = drawError;
  hooks.refreshStarted = refreshStarted;
  return hooks;
}

int main() {
  WeatherApp startupApp(50, 1000);
  available = true;
  view = WeatherApp::View::Weather;
  clockMs = 100;
  if (!startupApp.begin(makeHooks(true))) {
    fprintf(stderr, "FAIL: startup begin\n");
    return EXIT_FAILURE;
  }
  if (strcmp(events, "WFAD") != 0) {
    fprintf(stderr, "FAIL: startup order was %s, expected WFAD\n", events);
    return EXIT_FAILURE;
  }

  startupApp.end();
  clearEvents();

  WeatherApp unavailableApp(50, 1000);
  available = false;
  if (!unavailableApp.begin(makeHooks(true))) {
    fprintf(stderr, "FAIL: unavailable startup begin\n");
    return EXIT_FAILURE;
  }
  if (strcmp(events, "WFE") != 0) {
    fprintf(stderr, "FAIL: unavailable startup order was %s, expected WFE\n", events);
    return EXIT_FAILURE;
  }

  unavailableApp.end();
  clearEvents();

  WeatherApp refreshApp(50, 1000);
  available = true;
  view = WeatherApp::View::Weather;
  fetchResult = true;
  clockMs = 100;
  if (!refreshApp.begin(makeHooks(true))) {
    fprintf(stderr, "FAIL: refresh app begin\n");
    return EXIT_FAILURE;
  }

  clearEvents();
  view = WeatherApp::View::Forecast;
  clockMs = 151;
  refreshApp.update();
  if (strcmp(events, "ISXWFAR") != 0) {
    fprintf(
      stderr,
      "FAIL: refresh order was %s, expected ISXWFAR\n",
      events);
    return EXIT_FAILURE;
  }

  refreshApp.end();
  clearEvents();

  // The new preparation hook is optional, preserving existing WeatherApp
  // callers while they migrate to the explicit pre-render step.
  WeatherApp legacyApp(50, 1000);
  available = true;
  view = WeatherApp::View::Weather;
  clockMs = 200;
  if (!legacyApp.begin(makeHooks(false))) {
    fprintf(stderr, "FAIL: legacy begin\n");
    return EXIT_FAILURE;
  }
  if (strcmp(events, "WFD") != 0) {
    fprintf(stderr, "FAIL: optional-hook order was %s, expected WFD\n", events);
    return EXIT_FAILURE;
  }

  printf("WeatherApp asset preparation tests passed.\n");
  return EXIT_SUCCESS;
}
