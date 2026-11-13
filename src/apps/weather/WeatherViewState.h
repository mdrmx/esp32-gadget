#pragma once

// Persistent Weather presentation state. This type deliberately contains no
// display, input, network, or Arduino dependencies so its state rules can be
// exercised in a small host-side C++ test.
class WeatherViewState {
public:
  int currentTab; // Existing index convention: 0 = Weather, 1 = Forecast.
  int weatherMetricOffset;
  int forecastOffset;
  // First forecast hour currently nearest the left edge of the graph.
  int forecastGraphOffset;
  int forecastGraphScrollPixels;
  bool forecastGraphView;

  WeatherViewState()
    : currentTab(0),
      weatherMetricOffset(0),
      forecastOffset(0),
      forecastGraphOffset(0),
      forecastGraphScrollPixels(0),
      forecastGraphView(false) {
  }

  // Main-tab changes reset scroll positions but intentionally retain the
  // chosen List/Graph forecast mode, matching the existing dashboard policy.
  void resetScrollablePositions() {
    weatherMetricOffset = 0;
    forecastOffset = 0;
    forecastGraphOffset = 0;
    forecastGraphScrollPixels = 0;
  }

  // List and Graph are two presentations of the same hourly data. Switching
  // presentation starts at the first hour in either mode.
  void selectForecastMode(bool graphMode) {
    forecastOffset = 0;
    forecastGraphOffset = 0;
    forecastGraphScrollPixels = 0;
    forecastGraphView = graphMode;
  }
};
