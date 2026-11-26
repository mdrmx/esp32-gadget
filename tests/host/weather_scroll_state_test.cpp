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

static WeatherScrollState::ViewportPosition expectedMetricsPosition(
  int pixels, int itemCount, int visibleCount, int rowStep, int overscroll
) {
  WeatherScrollState::ViewportPosition result = {0, 0};
  if (rowStep <= 0) return result;
  const int maxOffset = itemCount - visibleCount > 0
    ? itemCount - visibleCount
    : 0;
  const int maxScroll = maxOffset * rowStep;
  const int lower = -overscroll;
  const int upper = maxScroll + overscroll;
  if (pixels < lower) pixels = lower;
  if (pixels > upper) pixels = upper;

  int baseOffset = pixels < 0 ? 0 : pixels / rowStep;
  int scrollPixels = pixels < 0 ? pixels : pixels % rowStep;
  if (baseOffset > maxOffset) {
    baseOffset = maxOffset;
    scrollPixels = pixels - maxScroll;
  }
  result.baseOffset = baseOffset;
  result.scrollPixels = scrollPixels;
  return result;
}

static WeatherScrollState::ViewportPosition legacyForecastListPosition(
  int pixels, int itemCount, int visibleCount, int rowStep, int overscroll
) {
  WeatherScrollState::ViewportPosition result = {0, 0};
  if (rowStep <= 0) return result;
  const int maxOffset = itemCount - visibleCount > 0
    ? itemCount - visibleCount
    : 0;
  const int maxScroll = maxOffset * rowStep;
  const int lower = -overscroll;
  const int upper = maxScroll + overscroll;
  if (pixels < lower) pixels = lower;
  if (pixels > upper) pixels = upper;
  int baseOffset = pixels < 0 ? 0 : pixels / rowStep;
  int scrollPixels = pixels < 0 ? pixels : pixels % rowStep;
  if (baseOffset > maxOffset) {
    baseOffset = maxOffset;
    scrollPixels = pixels - maxScroll;
  }
  result.baseOffset = baseOffset;
  result.scrollPixels = scrollPixels;
  return result;
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

  CHECK(WeatherScrollState::maximumScrollPixels(8, 3, 59) == 295);
  CHECK(WeatherScrollState::maximumScrollPixels(4, 3, 43) == 43);
  CHECK(WeatherScrollState::maximumScrollPixels(3, 3, 43) == 0);
  CHECK(WeatherScrollState::maximumScrollPixels(2, 3, 43) == 0);
  CHECK(WeatherScrollState::maximumScrollPixels(8, 3, 0) == 0);
  CHECK(WeatherScrollState::maximumScrollPixels(8, 3, -1) == 0);

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

  // Exhaustive parity sweep against the pre-extraction sketch arithmetic.
  // This confirms the refactor preserves row mapping, C++ negative division
  // semantics and elastic top/bottom displacement across representative sizes.
  for (int itemCount = 0; itemCount <= 12; itemCount++) {
    for (int visibleCount = 1; visibleCount <= 6; visibleCount++) {
      for (int rowStep = 1; rowStep <= 15; rowStep++) {
        for (int overscroll = 0; overscroll <= 5; overscroll++) {
          const int maxOffset = WeatherScrollState::maximumOffset(
            itemCount, visibleCount);
          const int maxScroll = maxOffset * rowStep;
          CHECK(WeatherScrollState::maximumScrollPixels(
            itemCount, visibleCount, rowStep) == maxScroll);
          for (int pixels = -overscroll - 5;
               pixels <= maxScroll + overscroll + 5;
               pixels++) {
            checkPosition(
              WeatherScrollState::metricsViewportPosition(
                pixels, itemCount, visibleCount, rowStep, overscroll),
              expectedMetricsPosition(
                pixels, itemCount, visibleCount, rowStep, overscroll).baseOffset,
              expectedMetricsPosition(
                pixels, itemCount, visibleCount, rowStep, overscroll).scrollPixels);
            const WeatherScrollState::ViewportPosition metricsPosition =
              WeatherScrollState::metricsViewportPosition(
                pixels, itemCount, visibleCount, rowStep, overscroll);
            CHECK(metricsPosition.baseOffset >= 0);
            CHECK(metricsPosition.baseOffset <= maxOffset);
            checkPosition(
              WeatherScrollState::forecastListViewportPosition(
                pixels, itemCount, visibleCount, rowStep, overscroll),
              legacyForecastListPosition(
                pixels, itemCount, visibleCount, rowStep, overscroll).baseOffset,
              legacyForecastListPosition(
                pixels, itemCount, visibleCount, rowStep, overscroll).scrollPixels);
          }
        }
      }
    }
  }

  // Overscroll greater than a row step must never produce an invalid
  // renderer index, including when the content has no scrollable range.
  checkPosition(
    WeatherScrollState::metricsViewportPosition(-100, 8, 3, 10, 25),
    0, -25);
  checkPosition(
    WeatherScrollState::metricsViewportPosition(100, 8, 3, 10, 25),
    5, 25);
  checkPosition(
    WeatherScrollState::metricsViewportPosition(-100, 2, 3, 10, 30),
    0, -30);
  checkPosition(
    WeatherScrollState::metricsViewportPosition(100, 2, 3, 10, 30),
    0, 30);

  puts("WeatherScrollState tests passed.");
  return EXIT_SUCCESS;
}
