#pragma once

#include <Arduino.h>
#include <math.h>
#include "../hardware/DisplayManager.h"

// Draws one hourly forecast list card (including its compact vector icon) and
// clips edge cards to the list viewport. Forecast data, scroll position,
// viewport placement and screen composition remain owned by Weather.
class ForecastListItemRenderer {
public:
  struct Colours {
    uint16_t panel;
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

    target->fillRoundRect(
      4, y, displayManager_.width() - 8, CARD_HEIGHT, 4, colours_.panel);

    target->setTextSize(1);
    target->setTextColor(colours_.text);
    target->setCursor(10, y + 18);
    target->print(item.time);

    drawMiniWeatherIcon(target, item.description, 48, y + 13);

    const String temperatureText =
      String((int)round(item.temperature)) + "C";
    target->setTextSize(2);
    target->setTextColor(colours_.temperature);
    target->setCursor(72, y + 14);
    target->print(temperatureText);

    const String feelsText =
      "FL " + String((int)round(item.feelsLike)) + "C";
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

  void drawConditionIcon(
    Arduino_GFX* target,
    const String& condition,
    int x,
    int y
  ) const {
    if (target == nullptr) return;
    drawMiniWeatherIcon(target, condition, x, y);
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
    const int clippedTop = max(0, cardTop);
    const int clippedBottom = min(viewportHeight, cardBottom);
    if (clippedTop >= clippedBottom) return;

    if (cardTop >= 0 && cardBottom <= viewportHeight) {
      draw(target, item, cardTop);
      return;
    }

    target->fillRect(
      4,
      clippedTop,
      displayManager_.width() - 8,
      clippedBottom - clippedTop,
      colours_.panel);

    const int fieldY = cardTop + 18;
    if (fieldY >= 0 && fieldY + 8 <= viewportHeight) {
      target->setTextSize(1);
      target->setTextColor(colours_.text);
      target->setCursor(10, fieldY);
      target->print(item.time);

      const String feelsText =
        "FL " + String((int)round(item.feelsLike)) + "C";
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
        String((int)round(item.temperature)) + "C";
      target->setTextSize(2);
      target->setTextColor(colours_.temperature);
      target->setCursor(72, cardTop + 14);
      target->print(temperatureText);
    }
  }

private:
  static int rainPercent(float probability) {
    int percent = (int)round(probability * 100.0f);
    if (percent < 0) return 0;
    if (percent > 100) return 100;
    return percent;
  }

  void drawMiniWeatherIcon(
    Arduino_GFX* target,
    const String& condition,
    int x,
    int y
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
      target->fillCircle(x + 8, y + 7, 4, colours_.temperature);
      target->drawFastHLine(x + 1, y + 7, 3, colours_.temperature);
      target->drawFastHLine(x + 12, y + 7, 3, colours_.temperature);
      target->drawFastVLine(x + 8, y, 3, colours_.temperature);
      target->drawFastVLine(x + 8, y + 11, 3, colours_.temperature);
      target->drawLine(x + 3, y + 2, x + 5, y + 4, colours_.temperature);
      target->drawLine(x + 11, y + 10, x + 13, y + 12, colours_.temperature);
      target->drawLine(x + 11, y + 4, x + 13, y + 2, colours_.temperature);
      target->drawLine(x + 3, y + 12, x + 5, y + 10, colours_.temperature);
      return;
    }

    if (mist) {
      target->drawFastHLine(x + 1, y + 4, 14, colours_.muted);
      target->drawFastHLine(x, y + 8, 15, colours_.text);
      target->drawFastHLine(x + 2, y + 12, 12, colours_.muted);
      return;
    }

    target->fillCircle(x + 5, y + 8, 3, colours_.text);
    target->fillCircle(x + 9, y + 5, 4, colours_.text);
    target->fillCircle(x + 13, y + 8, 3, colours_.text);
    target->fillRect(x + 4, y + 7, 10, 5, colours_.text);

    if (thunder) {
      target->drawLine(x + 9, y + 10, x + 6, y + 14, colours_.temperature);
      target->drawLine(x + 6, y + 14, x + 9, y + 14, colours_.temperature);
      target->drawLine(x + 9, y + 14, x + 7, y + 17, colours_.temperature);
    } else if (snow) {
      target->drawFastVLine(x + 6, y + 13, 4, colours_.accent);
      target->drawFastHLine(x + 4, y + 14, 4, colours_.accent);
      target->drawFastHLine(x + 9, y + 15, 4, colours_.accent);
    } else if (rain) {
      target->drawLine(x + 5, y + 13, x + 3, y + 16, colours_.accent);
      target->drawLine(x + 10, y + 13, x + 8, y + 16, colours_.accent);
      target->drawLine(x + 14, y + 13, x + 12, y + 16, colours_.accent);
    }
  }

  DisplayManager& displayManager_;
  Colours colours_;
};
