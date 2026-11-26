#pragma once



// Owns the canonical scroll positions for each Weather view and the
// view-specific conversion from pixel positions to renderer viewport inputs.
//
// This model deliberately has no display, input, network or Arduino
// dependencies. Touch candidates, drag ownership, release timing, animation
// and drawing remain in the Weather composition root.
class WeatherScrollState {
public:
  struct ViewportPosition {
    int baseOffset;
    int scrollPixels;
  };

  int weatherMetricOffset = 0;
  int forecastOffset = 0;
  int forecastGraphOffset = 0;
  int forecastGraphScrollPixels = 0;

  void resetAll() {
    weatherMetricOffset = 0;
    resetForecast();
  }

  // List and Graph are separate views of the same hourly data. Switching
  // between them begins at the first hour in either presentation.
  void resetForecast() {
    forecastOffset = 0;
    forecastGraphOffset = 0;
    forecastGraphScrollPixels = 0;
  }

  static int maximumOffset(int itemCount, int visibleCount) {
    const int difference = itemCount - visibleCount;
    return difference > 0 ? difference : 0;
  }

  static int clampViewportPixels(
    int pixels,
    int maximumScrollPixels,
    int overscrollLimitPixels
  ) {
    const int overscroll = overscrollLimitPixels > 0
      ? overscrollLimitPixels
      : 0;
    const int lowerBound = -overscroll;
    const int upperBound = maximumScrollPixels + overscroll;
    return pixels < lowerBound
      ? lowerBound
      : pixels > upperBound
        ? upperBound
        : pixels;
  }


  // Convert the metric panel's elastic pixel position into the exact pair
  // consumed by WeatherMetricsViewportRenderer. C++ integer division for a
  // negative top-edge pull is intentional and matches the previous sketch.
  static ViewportPosition metricsViewportPosition(
    int pixels,
    int itemCount,
    int visibleCount,
    int rowStepPixels,
    int overscrollLimitPixels
  ) {
    ViewportPosition result = {0, 0};
    if (rowStepPixels <= 0) return result;

    const int maxOffset = maximumOffset(itemCount, visibleCount);
    const int maxScrollPixels = maxOffset * rowStepPixels;
    const int boundedPixels = clampViewportPixels(
      pixels, maxScrollPixels, overscrollLimitPixels);

    result.baseOffset = boundedPixels / rowStepPixels;
    result.scrollPixels = boundedPixels % rowStepPixels;
    return result;
  }

  // Convert forecast-list motion into the row and within-row displacement
  // consumed by ForecastListViewportRenderer. At the top and bottom edges the
  // partial-row offset remains visible and is clipped by the viewport renderer.
  static ViewportPosition forecastListViewportPosition(
    int pixels,
    int itemCount,
    int visibleCount,
    int rowStepPixels,
    int overscrollLimitPixels
  ) {
    ViewportPosition result = {0, 0};
    if (rowStepPixels <= 0) return result;

    const int maxOffset = maximumOffset(itemCount, visibleCount);
    const int maxScrollPixels = maxOffset * rowStepPixels;
    const int overscroll = overscrollLimitPixels > 0
      ? overscrollLimitPixels
      : 0;
    const int boundedPixels = ScrollController::clampElasticPosition(
      pixels, maxScrollPixels, overscroll);

    int baseOffset = boundedPixels < 0
      ? 0
      : boundedPixels / rowStepPixels;
    int scrollPixels = boundedPixels < 0
      ? boundedPixels
      : boundedPixels % rowStepPixels;

    if (baseOffset > maxOffset) {
      baseOffset = maxOffset;
      scrollPixels = boundedPixels - maxScrollPixels;
    }

    result.baseOffset = baseOffset;
    result.scrollPixels = scrollPixels;
    return result;
  }

  // Graph panning retains a pixel position for smooth rendering, while this
  // derives the first visible forecast record. Graph hold, quick-swipe,
  // point-selection and horizontal-drag policy remain outside this model.
  static int forecastGraphOffsetForPixels(
    int pixels,
    int itemCount,
    int visibleCount,
    int pointSpacingPixels
  ) {
    if (pointSpacingPixels <= 0) return 0;

    const int maxOffset = maximumOffset(itemCount, visibleCount);
    const int nonNegativePixels = pixels > 0 ? pixels : 0;
    const int offset = nonNegativePixels / pointSpacingPixels;
    return offset > maxOffset ? maxOffset : offset;
  }
};
