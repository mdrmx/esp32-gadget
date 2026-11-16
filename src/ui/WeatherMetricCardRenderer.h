#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <algorithm>

// Draws one Weather metric card and clips its contents to the supplied panel
// viewport. Weather retains metric selection/formatting, panel layout, scroll
// state, canvas lifecycle and screen composition.
class WeatherMetricCardRenderer {
public:
  struct Colours {
    uint16_t panel;
    uint16_t label;
    uint16_t value;
  };

  enum {
    CARD_HEIGHT = 54
  };

  explicit WeatherMetricCardRenderer(const Colours& colours)
    : colours_(colours) {}

  void draw(
    Arduino_GFX* target,
    const String& label,
    const String& value,
    int x,
    int y,
    int panelWidth,
    int viewportTop,
    int viewportHeight
  ) const {
    if (target == nullptr || viewportHeight <= 0) return;

    const int localY = y - viewportTop;
    const int cardBottom = localY + CARD_HEIGHT;
    const int clippedTop = std::max(0, localY);
    const int clippedBottom = std::min(viewportHeight, cardBottom);
    if (clippedTop >= clippedBottom) return;

    const uint8_t valueSize = value.length() <= 6 ? 2 : 1;
    int16_t x1, y1;
    uint16_t width, height;

    if (localY >= 0 && cardBottom <= viewportHeight) {
      target->fillRoundRect(
        x + 2,
        viewportTop + localY,
        panelWidth - 4,
        CARD_HEIGHT,
        6,
        colours_.panel);

      target->setTextSize(1);
      target->setTextColor(colours_.label);
      target->setCursor(x + 9, viewportTop + localY + 7);
      target->print(label);

      target->setTextSize(valueSize);
      target->setTextColor(colours_.value);
      target->getTextBounds(
        value.c_str(), 0, 0, &x1, &y1, &width, &height);
      const int valueX = x + (panelWidth - (int)width) / 2;
      const int valueY = viewportTop + localY +
        (valueSize == 2 ? 23 : 28);
      target->setCursor(valueX, valueY);
      target->print(value);
      return;
    }

    // Partial edge cards use a clipped rectangular slice. Only draw text
    // fields whose full glyph bounds fit inside the viewport.
    target->fillRect(
      x + 2,
      viewportTop + clippedTop,
      panelWidth - 4,
      clippedBottom - clippedTop,
      colours_.panel);

    const int labelLocalY = localY + 7;
    if (labelLocalY >= 0 && labelLocalY + 8 <= viewportHeight) {
      target->setTextSize(1);
      target->setTextColor(colours_.label);
      target->setCursor(x + 9, viewportTop + labelLocalY);
      target->print(label);
    }

    target->setTextSize(valueSize);
    target->getTextBounds(
      value.c_str(), 0, 0, &x1, &y1, &width, &height);
    const int valueLocalY = localY + (valueSize == 2 ? 23 : 28);
    if (valueLocalY >= 0 &&
        valueLocalY + (int)height <= viewportHeight) {
      const int valueX = x + (panelWidth - (int)width) / 2;
      target->setTextColor(colours_.value);
      target->setCursor(valueX, viewportTop + valueLocalY);
      target->print(value);
    }
  }

private:
  Colours colours_;
};
