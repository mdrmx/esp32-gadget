#pragma once

#include "WeatherScrollState.h"

// Persistent Weather view selection. Scroll positions and their
// renderer-facing viewport calculations are owned by WeatherScrollState.
class WeatherViewState {
public:
  int currentTab = 0; // Existing index convention: 0 = Weather, 1 = Forecast.
  WeatherScrollState scroll;
  bool forecastGraphView = false;

  WeatherViewState() = default;

  // Main-tab changes reset scroll positions but intentionally retain the
  // chosen List/Graph forecast mode, matching the existing dashboard policy.
  void resetScrollablePositions() {
    scroll.resetAll();
  }

  // List and Graph are two presentations of the same hourly data. Switching
  // presentation starts at the first hour in either mode.
  void selectForecastMode(bool graphMode) {
    scroll.resetForecast();
    forecastGraphView = graphMode;
  }
};
