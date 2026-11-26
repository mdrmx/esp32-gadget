#include "../../src/apps/weather/WeatherScrollState.h"

#include <stdio.h>
#include <stdlib.h>

static void check(bool condition, const char* expression, int line) {
  if (!condition) {
    fprintf(stderr, "FAIL: %s (line %d)\n", expression, line);
    exit(EXIT_FAILURE);
  }
}

#define CHECK(expression) check((expression), #expression, __LINE__)

static void checkPosition(
  const WeatherScrollState::ViewportPosition& actual,
  int expectedOffset,
  int expectedPixels
) {
  CHECK(actual.baseOffset == expectedOffset);
  CHECK(actual.scrollPixels == expectedPixels);
}

int main() {
  WeatherScrollState state;

  CHECK(state.weatherMetricOffset == 0);
  CHECK(state.forecastOffset == 0);
  CHECK(state.forecastGraphOffset == 0);
  CHECK(state.forecastGraphScrollPixels == 0);

  state.weatherMetricOffset = 2;
  state.forecastOffset = 3;
  state.forecastGraphOffset = 4;
  state.forecastGraphScrollPixels = 224;
  state.resetForecast();

  CHECK(state.weatherMetricOffset == 2);
  CHECK(state.forecastOffset == 0);
  CHECK(state.forecastGraphOffset == 0);
  CHECK(state.forecastGraphScrollPixels == 0);

  state.forecastOffset = 3;
  state.forecastGraphOffset = 4;
  state.forecastGraphScrollPixels = 224;
  state.resetAll();

  CHECK(state.weatherMetricOffset == 0);
  CHECK(state.forecastOffset == 0);
  CHECK(state.forecastGraphOffset == 0);
  CHECK(state.forecastGraphScrollPixels == 0);

  CHECK(WeatherScrollState::maximumOffset(8, 3) == 5);
  CHECK(WeatherScrollState::maximumOffset(2, 3) == 0);
  CHECK(WeatherScrollState::maximumOffset(0, 3) == 0);

  // Metric panel: row conversion retains C++ truncation for a negative pull.
  checkPosition(
    WeatherScrollState::metricsViewportPosition(0, 8, 3, 59, 20),
    0, 0);
  checkPosition(
    WeatherScrollState::metricsViewportPosition(24, 8, 3, 59, 20),
    0, 24);
  checkPosition(
    WeatherScrollState::metricsViewportPosition(59, 8, 3, 59, 20),
    1, 0);
  checkPosition(
    WeatherScrollState::metricsViewportPosition(-20, 8, 3, 59, 20),
    0, -20);
  checkPosition(
    WeatherScrollState::metricsViewportPosition(500, 8, 3, 59, 20),
    5, 20);
  checkPosition(
    WeatherScrollState::metricsViewportPosition(50, 2, 3, 59, 20),
    0, 20);
  checkPosition(
    WeatherScrollState::metricsViewportPosition(-50, 2, 3, 59, 20),
    0, -20);

  // Forecast list: retain edge displacement and clamp to the final row.
  checkPosition(
    WeatherScrollState::forecastListViewportPosition(0, 12, 3, 43, 20),
    0, 0);
  checkPosition(
    WeatherScrollState::forecastListViewportPosition(22, 12, 3, 43, 20),
    0, 22);
  checkPosition(
    WeatherScrollState::forecastListViewportPosition(43, 12, 3, 43, 20),
    1, 0);
  checkPosition(
    WeatherScrollState::forecastListViewportPosition(-20, 12, 3, 43, 20),
    0, -20);
  checkPosition(
    WeatherScrollState::forecastListViewportPosition(407, 12, 3, 43, 20),
    9, 20);
  checkPosition(
    WeatherScrollState::forecastListViewportPosition(20, 2, 3, 43, 20),
    0, 20);

  // Graph panning retains pixel movement and derives only the first record.
  CHECK(WeatherScrollState::forecastGraphOffsetForPixels(-20, 12, 4, 56) == 0);
  CHECK(WeatherScrollState::forecastGraphOffsetForPixels(0, 12, 4, 56) == 0);
  CHECK(WeatherScrollState::forecastGraphOffsetForPixels(55, 12, 4, 56) == 0);
  CHECK(WeatherScrollState::forecastGraphOffsetForPixels(56, 12, 4, 56) == 1);
  CHECK(WeatherScrollState::forecastGraphOffsetForPixels(280, 12, 4, 56) == 5);
  CHECK(WeatherScrollState::forecastGraphOffsetForPixels(1000, 12, 4, 56) == 8);
  CHECK(WeatherScrollState::forecastGraphOffsetForPixels(100, 12, 4, 0) == 0);

  puts("WeatherScrollState tests passed.");
  return EXIT_SUCCESS;
}
