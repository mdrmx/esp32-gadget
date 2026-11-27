#pragma once

#include <Arduino.h>
#include <algorithm>
#include <cmath>
#include "../hardware/DisplayManager.h"

// Draws one hourly forecast list card (including its compact vector icon) and
// clips edge cards to the list viewport. Forecast data, scroll position,
// viewport placement and screen composition remain owned by Weather.
class ForecastListItemRenderer {
public:
  struct Colours {
    uint16_t panel;
    uint16_t outline;
    uint16_t text;
    uint16_t temperature;
    uint16_t muted;
    uint16_t accent;
  };

  struct Item {
    String time;
    String description;
    float temperature;
    float feelsLike;
    float rainProbability;
  };

  enum {
    CARD_HEIGHT = 44
  };

  ForecastListItemRenderer(
    DisplayManager& displayManager,
    const Colours& colours
  )
    : displayManager_(displayManager), colours_(colours) {}

  void draw(
    Arduino_GFX* target,
    const Item& item,
    int y
  ) const {
    if (target == nullptr) return;

    const int cardWidth = displayManager_.width() - 8;
    target->fillRoundRect(
      4, y, cardWidth, CARD_HEIGHT, 4, colours_.panel);
    target->drawRoundRect(
      4, y, cardWidth, CARD_HEIGHT, 4, colours_.outline);

    target->setTextSize(1);
    target->setTextColor(colours_.text);
    target->setCursor(10, y + 18);
    target->print(item.time);

    drawMiniWeatherIcon(target, item.description, 48, y + 13);

    const String temperatureText =
      String((int)std::round(item.temperature)) + "C";
    target->setTextSize(2);
    target->setTextColor(colours_.temperature);
    target->setCursor(72, y + 14);
    target->print(temperatureText);

    const String feelsText =
      "FL " + String((int)std::round(item.feelsLike)) + "C";
    target->setTextSize(1);
    target->setTextColor(colours_.muted);
    target->setCursor(145, y + 18);
    target->print(feelsText);

    const String rainText =
      "POP " + String(rainPercent(item.rainProbability)) + "%";
    int16_t x1, y1;
    uint16_t width, height;
    target->getTextBounds(
      rainText.c_str(), 0, 0, &x1, &y1, &width, &height);
    target->setTextColor(colours_.text);
    target->setCursor(
      displayManager_.width() - 10 - (int)width, y + 18);
    target->print(rainText);
  }

  // Forecast condition symbols are drawn directly from vector primitives,
  // not from PNGs or padded bitmap canvases. List and Graph use this same
  // tightly bounded drawing path, so there are no transparent pixels to crop.
  void drawConditionIcon(
    Arduino_GFX* target,
    const String& condition,
    int x,
    int y
  ) const {
    if (target == nullptr) return;
    drawMiniWeatherIcon(target, condition, x, y);
  }

  // Graph uses a slightly enlarged vector symbol; List keeps the original
  // compact 16x17 drawing so its established card layout is unchanged.
  void drawGraphConditionIcon(
    Arduino_GFX* target,
    const String& condition,
    int x,
    int y
  ) const {
    if (target == nullptr) return;
    drawMiniWeatherIcon(target, condition, x, y, 120);
  }

  void drawClipped(
    Arduino_GFX* target,
    const Item& item,
    int y,
    int viewportTop,
    int viewportHeight
  ) const {
    if (target == nullptr || viewportHeight <= 0) return;

    const int cardTop = y - viewportTop;
    const int cardBottom = cardTop + CARD_HEIGHT;
    const int clippedTop = std::max(0, cardTop);
    const int clippedBottom = std::min(viewportHeight, cardBottom);
    if (clippedTop >= clippedBottom) return;

    if (cardTop >= 0 && cardBottom <= viewportHeight) {
      draw(target, item, cardTop);
      return;
    }

    const int cardX = 4;
    const int cardWidth = displayManager_.width() - 8;
    const int visibleHeight = clippedBottom - clippedTop;
    target->fillRect(
      cardX,
      clippedTop,
      cardWidth,
      visibleHeight,
      colours_.panel);
    target->drawFastVLine(
      cardX, clippedTop, visibleHeight, colours_.outline);
    target->drawFastVLine(
      cardX + cardWidth - 1, clippedTop, visibleHeight, colours_.outline);
    if (cardTop >= 0) {
      target->drawFastHLine(
        cardX, cardTop, cardWidth, colours_.outline);
    }
    if (cardBottom <= viewportHeight) {
      target->drawFastHLine(
        cardX, cardBottom - 1, cardWidth, colours_.outline);
    }

    const int fieldY = cardTop + 18;
    if (fieldY >= 0 && fieldY + 8 <= viewportHeight) {
      target->setTextSize(1);
      target->setTextColor(colours_.text);
      target->setCursor(10, fieldY);
      target->print(item.time);

      const String feelsText =
        "FL " + String((int)std::round(item.feelsLike)) + "C";
      target->setTextColor(colours_.muted);
      target->setCursor(145, fieldY);
      target->print(feelsText);

      const String rainText =
        "POP " + String(rainPercent(item.rainProbability)) + "%";
      int16_t x1, y1;
      uint16_t width, height;
      target->getTextBounds(
        rainText.c_str(), 0, 0, &x1, &y1, &width, &height);
      target->setTextColor(colours_.text);
      target->setCursor(
        displayManager_.width() - 10 - (int)width, fieldY);
      target->print(rainText);
    }

    if (cardTop + 13 >= 0 && cardTop + 31 <= viewportHeight) {
      drawMiniWeatherIcon(
        target, item.description, 48, cardTop + 13);

      const String temperatureText =
        String((int)std::round(item.temperature)) + "C";
      target->setTextSize(2);
      target->setTextColor(colours_.temperature);
      target->setCursor(72, cardTop + 14);
      target->print(temperatureText);
    }
  }

private:
  static int rainPercent(float probability) {
    int percent = (int)std::round(probability * 100.0f);
    if (percent < 0) return 0;
    if (percent > 100) return 100;
    return percent;
  }

  static int scaleIconValue(int value, int scalePercent) {
    return (value * scalePercent + 50) / 100;
  }

  void drawMiniWeatherIcon(
    Arduino_GFX* target,
    const String& condition,
    int x,
    int y,
    int scalePercent = 100
  ) const {
    const bool clear = condition.indexOf("Clear") >= 0;
    const bool rain = condition.indexOf("Rain") >= 0 ||
                      condition.indexOf("Drizzle") >= 0;
    const bool snow = condition.indexOf("Snow") >= 0;
    const bool thunder = condition.indexOf("Thunder") >= 0;
    const bool mist = condition.indexOf("Mist") >= 0 ||
                      condition.indexOf("Fog") >= 0 ||
                      condition.indexOf("Haze") >= 0;

    if (clear) {
      target->fillCircle(
        x + scaleIconValue(8, scalePercent),
        y + scaleIconValue(7, scalePercent),
        scaleIconValue(4, scalePercent),
        colours_.temperature);
      target->drawFastHLine(
        x + scaleIconValue(1, scalePercent),
        y + scaleIconValue(7, scalePercent),
        scaleIconValue(3, scalePercent), colours_.temperature);
      target->drawFastHLine(
        x + scaleIconValue(12, scalePercent),
        y + scaleIconValue(7, scalePercent),
        scaleIconValue(3, scalePercent), colours_.temperature);
      target->drawFastVLine(
        x + scaleIconValue(8, scalePercent), y,
        scaleIconValue(3, scalePercent), colours_.temperature);
      target->drawFastVLine(
        x + scaleIconValue(8, scalePercent),
        y + scaleIconValue(11, scalePercent),
        scaleIconValue(3, scalePercent), colours_.temperature);
      target->drawLine(
        x + scaleIconValue(3, scalePercent),
        y + scaleIconValue(2, scalePercent),
        x + scaleIconValue(5, scalePercent),
        y + scaleIconValue(4, scalePercent), colours_.temperature);
      target->drawLine(
        x + scaleIconValue(11, scalePercent),
        y + scaleIconValue(10, scalePercent),
        x + scaleIconValue(13, scalePercent),
        y + scaleIconValue(12, scalePercent), colours_.temperature);
      target->drawLine(
        x + scaleIconValue(11, scalePercent),
        y + scaleIconValue(4, scalePercent),
        x + scaleIconValue(13, scalePercent),
        y + scaleIconValue(2, scalePercent), colours_.temperature);
      target->drawLine(
        x + scaleIconValue(3, scalePercent),
        y + scaleIconValue(12, scalePercent),
        x + scaleIconValue(5, scalePercent),
        y + scaleIconValue(10, scalePercent), colours_.temperature);
      return;
    }

    if (mist) {
      target->drawFastHLine(
        x + scaleIconValue(1, scalePercent),
        y + scaleIconValue(4, scalePercent),
        scaleIconValue(14, scalePercent), colours_.muted);
      target->drawFastHLine(
        x, y + scaleIconValue(8, scalePercent),
        scaleIconValue(15, scalePercent), colours_.text);
      target->drawFastHLine(
        x + scaleIconValue(2, scalePercent),
        y + scaleIconValue(12, scalePercent),
        scaleIconValue(12, scalePercent), colours_.muted);
      return;
    }

    target->fillCircle(
      x + scaleIconValue(5, scalePercent),
      y + scaleIconValue(8, scalePercent),
      scaleIconValue(3, scalePercent), colours_.text);
    target->fillCircle(
      x + scaleIconValue(9, scalePercent),
      y + scaleIconValue(5, scalePercent),
      scaleIconValue(4, scalePercent), colours_.text);
    target->fillCircle(
      x + scaleIconValue(13, scalePercent),
      y + scaleIconValue(8, scalePercent),
      scaleIconValue(3, scalePercent), colours_.text);
    target->fillRect(
      x + scaleIconValue(4, scalePercent),
      y + scaleIconValue(7, scalePercent),
      scaleIconValue(10, scalePercent),
      scaleIconValue(5, scalePercent), colours_.text);

    if (thunder) {
      target->drawLine(
        x + scaleIconValue(9, scalePercent),
        y + scaleIconValue(10, scalePercent),
        x + scaleIconValue(6, scalePercent),
        y + scaleIconValue(14, scalePercent), colours_.temperature);
      target->drawLine(
        x + scaleIconValue(6, scalePercent),
        y + scaleIconValue(14, scalePercent),
        x + scaleIconValue(9, scalePercent),
        y + scaleIconValue(14, scalePercent), colours_.temperature);
      target->drawLine(
        x + scaleIconValue(9, scalePercent),
        y + scaleIconValue(14, scalePercent),
        x + scaleIconValue(7, scalePercent),
        y + scaleIconValue(17, scalePercent), colours_.temperature);
    } else if (snow) {
      target->drawFastVLine(
        x + scaleIconValue(6, scalePercent),
        y + scaleIconValue(13, scalePercent),
        scaleIconValue(4, scalePercent), colours_.accent);
      target->drawFastHLine(
        x + scaleIconValue(4, scalePercent),
        y + scaleIconValue(14, scalePercent),
        scaleIconValue(4, scalePercent), colours_.accent);
      target->drawFastHLine(
        x + scaleIconValue(9, scalePercent),
        y + scaleIconValue(15, scalePercent),
        scaleIconValue(4, scalePercent), colours_.accent);
    } else if (rain) {
      target->drawLine(
        x + scaleIconValue(5, scalePercent),
        y + scaleIconValue(13, scalePercent),
        x + scaleIconValue(3, scalePercent),
        y + scaleIconValue(16, scalePercent), colours_.accent);
      target->drawLine(
        x + scaleIconValue(10, scalePercent),
        y + scaleIconValue(13, scalePercent),
        x + scaleIconValue(8, scalePercent),
        y + scaleIconValue(16, scalePercent), colours_.accent);
      target->drawLine(
        x + scaleIconValue(14, scalePercent),
        y + scaleIconValue(13, scalePercent),
        x + scaleIconValue(12, scalePercent),
        y + scaleIconValue(16, scalePercent), colours_.accent);
    }
  }

  DisplayManager& displayManager_;
  Colours colours_;
};
