#pragma once

#include <Arduino.h>
#include "../hardware/DisplayManager.h"
#include "TextRenderer.h"

// Draws the fixed Weather header without owning application data, clock state,
// theme colours or display resources. Text and colours are supplied by the app;
// DisplayManager supplies the active physical or transition render target.
class HeaderRenderer {
public:
  HeaderRenderer(
    DisplayManager& displayManager,
    TextRenderer& textRenderer
  )
    : displayManager_(displayManager),
      textRenderer_(textRenderer) {}

  void draw(
    const String& locationText,
    const String& clockText,
    uint16_t panelColour,
    uint16_t locationColour,
    uint16_t accentColour
  ) const {
    Arduino_GFX* target = displayManager_.target();
    if (target == nullptr) return;

    target->fillRect(
      0, 0, displayManager_.width(), HEADER_HEIGHT, panelColour);
    textRenderer_.drawText(
      locationText, LOCATION_X, TEXT_Y, TEXT_SIZE, locationColour);
    drawClockText(clockText, accentColour);
  
    target->drawFastHLine(
      0, HEADER_HEIGHT, displayManager_.width(), accentColour);

  }

  // Redraw only the clock's reserved region when the displayed time changes.
  void drawClockOnly(
    const String& clockText,
    uint16_t panelColour,
    uint16_t accentColour
  ) const {
    Arduino_GFX* target = displayManager_.target();
    if (target == nullptr) return;

    target->fillRect(
      displayManager_.width() - CLOCK_REGION_WIDTH,
      0,
      CLOCK_REGION_WIDTH,
      HEADER_HEIGHT,
      panelColour);
    drawClockText(clockText, accentColour);
  }

private:
  enum {
    HEADER_HEIGHT = 36,
    LOCATION_X = 10,
    TEXT_Y = 10,
    TEXT_SIZE = 2,
    CLOCK_REGION_WIDTH = 78,
    CLOCK_RIGHT_MARGIN = 10,
    CLOCK_MINIMUM_X_MARGIN = 70
  };

  void drawClockText(
    const String& clockText,
    uint16_t accentColour
  ) const {
    const int clockWidth = textRenderer_.measureTextWidth(
      clockText, TEXT_SIZE);
    int x = displayManager_.width() - CLOCK_RIGHT_MARGIN - clockWidth;
    if (x < displayManager_.width() - CLOCK_MINIMUM_X_MARGIN) {
      x = displayManager_.width() - CLOCK_MINIMUM_X_MARGIN;
    }

    textRenderer_.drawText(clockText, x, TEXT_Y, TEXT_SIZE, accentColour);
  }

  DisplayManager& displayManager_;
  TextRenderer& textRenderer_;
};
