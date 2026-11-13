#include "../../src/apps/weather/WeatherViewState.h"

#include <stdio.h>
#include <stdlib.h>

static void check(bool condition, const char* expression, int line) {
  if (!condition) {
    fprintf(stderr, "FAIL: %s (line %d)\n", expression, line);
    exit(EXIT_FAILURE);
  }
}

#define CHECK(expression) check((expression), #expression, __LINE__)

int main() {
  WeatherViewState state;

  // A fresh app starts on Weather, at the first metric/hour, in List mode.
  CHECK(state.currentTab == 0);
  CHECK(state.weatherMetricOffset == 0);
  CHECK(state.forecastOffset == 0);
  CHECK(state.forecastGraphOffset == 0);
  CHECK(state.forecastGraphScrollPixels == 0);
  CHECK(!state.forecastGraphView);

  // Main-tab reset clears all canonical scroll positions, but preserves the
  // active tab and selected forecast presentation.
  state.currentTab = 1;
  state.weatherMetricOffset = 2;
  state.forecastOffset = 5;
  state.forecastGraphOffset = 4;
  state.forecastGraphScrollPixels = 224;
  state.forecastGraphView = true;

  state.resetScrollablePositions();

  CHECK(state.currentTab == 1);
  CHECK(state.weatherMetricOffset == 0);
  CHECK(state.forecastOffset == 0);
  CHECK(state.forecastGraphOffset == 0);
  CHECK(state.forecastGraphScrollPixels == 0);
  CHECK(state.forecastGraphView);

  // Selecting Graph resets both forecast scroll representations.
  state.forecastOffset = 3;
  state.forecastGraphOffset = 2;
  state.forecastGraphScrollPixels = 112;
  state.selectForecastMode(true);

  CHECK(state.forecastGraphView);
  CHECK(state.forecastOffset == 0);
  CHECK(state.forecastGraphOffset == 0);
  CHECK(state.forecastGraphScrollPixels == 0);

  // Selecting List performs the same reset and changes only the mode.
  state.forecastOffset = 1;
  state.forecastGraphOffset = 1;
  state.forecastGraphScrollPixels = 56;
  state.selectForecastMode(false);

  CHECK(!state.forecastGraphView);
  CHECK(state.forecastOffset == 0);
  CHECK(state.forecastGraphOffset == 0);
  CHECK(state.forecastGraphScrollPixels == 0);
  CHECK(state.currentTab == 1);

  puts("WeatherViewState tests passed.");
  return EXIT_SUCCESS;
}
