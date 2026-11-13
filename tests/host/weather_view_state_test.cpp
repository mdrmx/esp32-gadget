#include "src/apps/weather/WeatherViewState.h"

#include <assert.h>
#include <stdio.h>

int main() {
  WeatherViewState state;

  // A fresh app starts on Weather, at the first metric/hour, in List mode.
  assert(state.currentTab == 0);
  assert(state.weatherMetricOffset == 0);
  assert(state.forecastOffset == 0);
  assert(state.forecastGraphOffset == 0);
  assert(state.forecastGraphScrollPixels == 0);
  assert(!state.forecastGraphView);

  // Main-tab reset clears all canonical scroll positions, but preserves the
  // active tab and selected forecast presentation.
  state.currentTab = 1;
  state.weatherMetricOffset = 2;
  state.forecastOffset = 5;
  state.forecastGraphOffset = 4;
  state.forecastGraphScrollPixels = 224;
  state.forecastGraphView = true;

  state.resetScrollablePositions();

  assert(state.currentTab == 1);
  assert(state.weatherMetricOffset == 0);
  assert(state.forecastOffset == 0);
  assert(state.forecastGraphOffset == 0);
  assert(state.forecastGraphScrollPixels == 0);
  assert(state.forecastGraphView);

  // Selecting Graph resets both forecast scroll representations.
  state.forecastOffset = 3;
  state.forecastGraphOffset = 2;
  state.forecastGraphScrollPixels = 112;
  state.selectForecastMode(true);

  assert(state.forecastGraphView);
  assert(state.forecastOffset == 0);
  assert(state.forecastGraphOffset == 0);
  assert(state.forecastGraphScrollPixels == 0);

  // Selecting List performs the same reset and changes only the mode.
  state.forecastOffset = 1;
  state.forecastGraphOffset = 1;
  state.forecastGraphScrollPixels = 56;
  state.selectForecastMode(false);

  assert(!state.forecastGraphView);
  assert(state.forecastOffset == 0);
  assert(state.forecastGraphOffset == 0);
  assert(state.forecastGraphScrollPixels == 0);
  assert(state.currentTab == 1);

  puts("WeatherViewState tests passed.");
  return 0;
}
