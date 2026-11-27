#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <algorithm>
#include <cmath>

#include "ForecastListItemRenderer.h"

// Draws the hourly forecast graph only. Weather retains forecast-data ownership,
// graph scroll state/clamping, gesture policy, animation and canvas lifecycle.
// Forecast records are supplied through a reader so this renderer does not
// depend on WeatherDataService or copy the fixed-capacity model.
class ForecastGraphRenderer {
public:
  struct Colours {
    uint16_t grid;
    uint16_t line;
    uint16_t point;
    uint16_t text;
    uint16_t muted;
    uint16_t rain;
  };

  struct Item {
    const String* timeText = nullptr;
    const String* description = nullptr;
    float temperature = 0.0f;
    float rainProbability = 0.0f;
  };

  typedef bool (*ItemReader)(
    const void* context,
    int index,
    Item& item
  );

  explicit ForecastGraphRenderer(
    ForecastListItemRenderer& listItemRenderer,
    const Colours& colours
  )
    : listItemRenderer_(listItemRenderer), colours_(colours) {}

  // scrollPixels is the Weather-owned, already-clamped pixel position. This
  // method does not update offsets, apply overscroll policy or schedule frames.
  void draw(
    Arduino_GFX* target,
    int displayWidth,
    int originY,
    int itemCount,
    int scrollPixels,
    const void* context,
    ItemReader readItem
  ) const {
    if (target == nullptr || displayWidth <= 0 || itemCount <= 0 ||
        context == nullptr || readItem == nullptr) {
      return;
    }

    const int left = LEFT_INSET;
    const int right = displayWidth - RIGHT_INSET;
    const int plotWidth = right - left;
    if (plotWidth <= 0) return;

    const int visibleScroll = (std::max)(0, scrollPixels);

    // Include one neighbouring hour after the visible window so the vertical
    // scale does not jump as the graph is dragged between hourly positions.
    const int rangeStart = (std::min)(
      (std::max)(0, visibleScroll / POINT_SPACING),
      itemCount - 1);
    const int rangeEnd = (std::min)(
      itemCount,
      rangeStart + VISIBLE_POINTS + 1);

    Item rangeItem;
    if (!readItem(context, rangeStart, rangeItem)) return;
    float minTemperature = rangeItem.temperature;
    float maxTemperature = rangeItem.temperature;

    for (int index = rangeStart + 1; index < rangeEnd; index++) {
      Item item;
      if (!readItem(context, index, item)) continue;
      minTemperature = (std::min)(minTemperature, item.temperature);
      maxTemperature = (std::max)(maxTemperature, item.temperature);
    }
    minTemperature -= 1.0f;
    maxTemperature += 1.0f;

    const int plotTop = BASE_PLOT_TOP + VERTICAL_SHIFT;
    const int plotBottom = BASE_PLOT_BOTTOM + VERTICAL_SHIFT;
    const int plotHeight = plotBottom - plotTop;
    target->drawFastHLine(
      left, originY + plotTop, plotWidth, colours_.grid);
    target->drawFastHLine(
      left, originY + plotBottom, plotWidth, colours_.grid);

    // Draw neighbouring points too, allowing the first/last visible line
    // segment to enter and leave the plot without a discontinuity.
    const int firstIndex = (std::max)(
      0, visibleScroll / POINT_SPACING - 1);
    const int lastIndex = (std::min)(
      itemCount,
      (visibleScroll + plotWidth) / POINT_SPACING + 2);

    int previousX = 0;
    int previousY = 0;
    bool hasPrevious = false;

    for (int index = firstIndex; index < lastIndex; index++) {
      Item item;
      if (!readItem(context, index, item)) continue;

      const int x = left + index * POINT_SPACING - scrollPixels;
      const float ratio =
        (item.temperature - minTemperature) /
        (maxTemperature - minTemperature);
      const int y = plotBottom - (int)std::round(ratio * plotHeight);

      if (hasPrevious && previousX <= right && x >= left) {
        int lineStartX = previousX;
        int lineStartY = previousY;
        int lineEndX = x;
        int lineEndY = y;
        const int dx = x - previousX;
        if (dx > 0) {
          if (lineStartX < left) {
            lineStartY = previousY +
              (y - previousY) * (left - previousX) / dx;
            lineStartX = left;
          }
          if (lineEndX > right) {
            lineEndY = previousY +
              (y - previousY) * (right - previousX) / dx;
            lineEndX = right;
          }
          if (lineStartX <= lineEndX) {
            drawTemperatureLine(
              target,
              lineStartX, originY + lineStartY,
              lineEndX, originY + lineEndY,
              colours_.line);
          }
        }
      }

      if (x >= left && x <= right) {
        target->fillCircle(x, originY + y, 2, colours_.point);
      }

      // Weather condition icons can sit close to the plot edge; clip against
      // the physical logical display bounds, not the inner chart boundary.
      if (item.description != nullptr &&
          x >= ICON_HALF_WIDTH && x <= displayWidth - ICON_HALF_WIDTH) {
        listItemRenderer_.drawGraphConditionIcon(
          target,
          *item.description,
          x - ICON_HALF_WIDTH,
          originY + ICON_TOP + VERTICAL_SHIFT,
          ICON_SCALE_PERCENT);
      }

      const String temperatureText =
        String((int)std::round(item.temperature)) + "C";
      drawCentredLabel(
        target, temperatureText, x,
        originY + y - TEMPERATURE_LABEL_OFFSET,
        displayWidth, colours_.text);

      if (item.timeText != nullptr) {
        String timeLabel = *item.timeText;
        const int separator = timeLabel.lastIndexOf(' ');
        if (separator >= 0) {
          timeLabel = timeLabel.substring(separator + 1);
        }
        drawCentredLabel(
          target, timeLabel, x,
          originY + TIME_LABEL_TOP + VERTICAL_SHIFT,
          displayWidth, colours_.muted);
      }

      const int rainPercent = clampPercent(
        (int)std::round(item.rainProbability * 100.0f));
      const int barHeight = (rainPercent * RAIN_BAR_MAX_HEIGHT) / 100;
      if (x >= RAIN_BAR_HALF_WIDTH &&
          x <= displayWidth - RAIN_BAR_HALF_WIDTH) {
        target->fillRect(
          x - RAIN_BAR_HALF_WIDTH,
          originY + RAIN_BAR_BOTTOM - barHeight,
          RAIN_BAR_WIDTH,
          (std::max)(1, barHeight),
          colours_.rain);
      }

      const String rainLabel = String(rainPercent) + "%";
      drawCentredLabel(
        target, rainLabel, x, originY + RAIN_LABEL_TOP,
        displayWidth, colours_.text);

      previousX = x;
      previousY = y;
      hasPrevious = true;
    }
  }

private:
  static constexpr int LEFT_INSET = 22;
  static constexpr int RIGHT_INSET = 17;
  static constexpr int VISIBLE_POINTS = 7;
  static constexpr int POINT_SPACING = 46;
  static constexpr int VERTICAL_SHIFT = 12;
  static constexpr int BASE_PLOT_TOP = 37;
  static constexpr int BASE_PLOT_BOTTOM = 89;
  static constexpr int ICON_HALF_WIDTH = 10;
  static constexpr int ICON_TOP = 6;
  static constexpr int ICON_SCALE_PERCENT = 120;
  static constexpr int TEMPERATURE_LABEL_OFFSET = 10;
  static constexpr int TIME_LABEL_TOP = 99;
  static constexpr int RAIN_BAR_BOTTOM = 139;
  static constexpr int RAIN_BAR_MAX_HEIGHT = 18;
  static constexpr int RAIN_BAR_WIDTH = 6;
  static constexpr int RAIN_BAR_HALF_WIDTH = 3;
  static constexpr int RAIN_LABEL_TOP = 143;
  static constexpr int TEXT_EDGE_MARGIN = 2;

  static int clampPercent(int percent) {
    if (percent < 0) return 0;
    if (percent > 100) return 100;
    return percent;
  }

  // A second parallel pass one pixel below the primary path gives the
  // temperature curve a clear two-pixel stroke without changing its geometry.
  static void drawTemperatureLine(
    Arduino_GFX* target,
    int x1,
    int y1,
    int x2,
    int y2,
    uint16_t colour
  ) {
    target->drawLine(x1, y1, x2, y2, colour);
    target->drawLine(x1, y1 + 1, x2, y2 + 1, colour);
  }

  static void drawCentredLabel(
    Arduino_GFX* target,
    const String& text,
    int centreX,
    int y,
    int displayWidth,
    uint16_t colour
  ) {
    int16_t x1, y1;
    uint16_t width, height;
    target->setTextSize(1);
    target->getTextBounds(
      text.c_str(), 0, 0, &x1, &y1, &width, &height);
    if (centreX - (int)width / 2 >= TEXT_EDGE_MARGIN &&
        centreX + ((int)width + 1) / 2 <= displayWidth - TEXT_EDGE_MARGIN) {
      target->setTextColor(colour);
      target->setCursor(centreX - (int)width / 2, y);
      target->print(text);
    }
  }

  ForecastListItemRenderer& listItemRenderer_;
  Colours colours_;
};
