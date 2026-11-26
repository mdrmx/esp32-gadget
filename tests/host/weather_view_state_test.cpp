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
  CHECK(state.scroll.weatherMetricOffset == 0);
  CHECK(state.scroll.forecastOffset == 0);
  CHECK(state.scroll.forecastGraphOffset == 0);
  CHECK(state.scroll.forecastGraphScrollPixels == 0);
  CHECK(!state.forecastGraphView);

  // Main-tab reset clears all canonical scroll positions, but preserves the
  // active tab and selected forecast presentation.
  state.currentTab = 1;
  state.scroll.weatherMetricOffset = 2;
  state.scroll.forecastOffset = 5;
  state.scroll.forecastGraphOffset = 4;
  state.scroll.forecastGraphScrollPixels = 224;
  state.forecastGraphView = true;

  state.resetScrollablePositions();

  CHECK(state.currentTab == 1);
  CHECK(state.scroll.weatherMetricOffset == 0);
  CHECK(state.scroll.forecastOffset == 0);
  CHECK(state.scroll.forecastGraphOffset == 0);
  CHECK(state.scroll.forecastGraphScrollPixels == 0);
  CHECK(state.forecastGraphView);

  // Selecting Graph resets both forecast scroll representations.
  state.scroll.forecastOffset = 3;
  state.scroll.forecastGraphOffset = 2;
  state.scroll.forecastGraphScrollPixels = 112;
  state.selectForecastMode(true);

  CHECK(state.forecastGraphView);
  CHECK(state.scroll.forecastOffset == 0);
  CHECK(state.scroll.forecastGraphOffset == 0);
  CHECK(state.scroll.forecastGraphScrollPixels == 0);

  // Selecting List performs the same reset and changes only the mode.
  state.scroll.forecastOffset = 1;
  state.scroll.forecastGraphOffset = 1;
  state.scroll.forecastGraphScrollPixels = 56;
  state.selectForecastMode(false);

  CHECK(!state.forecastGraphView);
  CHECK(state.scroll.forecastOffset == 0);
  CHECK(state.scroll.forecastGraphOffset == 0);
  CHECK(state.scroll.forecastGraphScrollPixels == 0);
  CHECK(state.currentTab == 1);

  puts("WeatherViewState tests passed.");
  return EXIT_SUCCESS;
}
