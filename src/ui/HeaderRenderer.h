#pragma once

#include <Arduino.h>
#include "../hardware/DisplayManager.h"
#include "TextRenderer.h"

// Draws the fixed Weather header and its live Wi-Fi signal indicator. The
// sketch supplies connection state and a smoothed RSSI level; this class owns
// only the indicator's presentation and keeps its hit area stable.
class HeaderRenderer {
public:
  // Visible Wi-Fi glyph bounds. Keep these tight so redrawing the icon
  // does not overwrite the adjacent clock or location text.
  enum {
    WiFiIndicatorLeft = 198,
    WiFiIndicatorTop = 4,
    WiFiIndicatorRight = 240,
    WiFiIndicatorBottom = 32
  };

  // Touch-only target: wider and taller than the glyph, but stops immediately
  // before the clock's leftmost layout position (x = display width - 70).
  enum {
    WiFiHitAreaLeft = 186,
    WiFiHitAreaTop = 0,
    WiFiHitAreaRight = 250,
    WiFiHitAreaBottom = 36
  };

  HeaderRenderer(
    DisplayManager& displayManager,
    TextRenderer& textRenderer
  )
    : displayManager_(displayManager),
      textRenderer_(textRenderer),
      wifiConnected_(false),
      wifiStrengthLevel_(0) {}

  // Returns true only when the visible indicator state changes.
  bool setWiFiStatus(bool connected, uint8_t strengthLevel) {
    if (strengthLevel > 4) strengthLevel = 4;
    if (!connected) strengthLevel = 0;

    const bool changed =
      wifiConnected_ != connected ||
      wifiStrengthLevel_ != strengthLevel;
    wifiConnected_ = connected;
    wifiStrengthLevel_ = strengthLevel;
    return changed;
  }

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
    drawWiFiIndicator(panelColour, locationColour, accentColour);
  }

  // Redraw only the clock's reserved region and the Wi-Fi indicator.
  void drawClockOnly(
    const String& clockText,
    uint16_t panelColour,
    uint16_t iconColour,
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
    drawWiFiIndicator(panelColour, iconColour, accentColour);
  }

  // Redraw when the averaged RSSI crosses a level boundary or the connection
  // state changes. The expanded 64x36 Wi-Fi touch target is independent of the
  // smaller visible glyph bounds above.
  void drawWiFiIndicator(
    uint16_t panelColour,
    uint16_t iconColour,
    uint16_t accentColour
  ) const {
    Arduino_GFX* target = displayManager_.target();
    if (target == nullptr) return;

    target->fillRect(
      WiFiIndicatorLeft,
      WiFiIndicatorTop,
      WiFiIndicatorRight - WiFiIndicatorLeft,
      WiFiIndicatorBottom - WiFiIndicatorTop,
      panelColour);

    const int centreX = WiFiIndicatorLeft + 21;
    const int topY = WiFiIndicatorTop + 1;

    if (!wifiConnected_) {
      drawOuterArc(target, centreX, topY, iconColour);
      drawMiddleArc(target, centreX, topY, iconColour);
      drawInnerArc(target, centreX, topY, iconColour);
      target->fillCircle(centreX, topY + 18, 2, iconColour);
      target->drawLine(
        centreX - 13, topY + 21,
        centreX + 13, topY - 2,
        accentColour);
      return;
    }

    // Four levels are represented by the dot and up to three nested arcs.
    if (wifiStrengthLevel_ >= 4) {
      drawOuterArc(target, centreX, topY, accentColour);
    }
    if (wifiStrengthLevel_ >= 3) {
      drawMiddleArc(target, centreX, topY, accentColour);
    }
    if (wifiStrengthLevel_ >= 2) {
      drawInnerArc(target, centreX, topY, accentColour);
    }
    target->fillCircle(centreX, topY + 18, 2, accentColour);
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

  static void drawOuterArc(
    Arduino_GFX* target, int cx, int y, uint16_t colour) {
    target->drawLine(cx - 13, y + 8, cx - 9, y + 4, colour);
    target->drawLine(cx - 9, y + 4, cx - 4, y + 1, colour);
    target->drawLine(cx - 4, y + 1, cx, y, colour);
    target->drawLine(cx, y, cx + 4, y + 1, colour);
    target->drawLine(cx + 4, y + 1, cx + 9, y + 4, colour);
    target->drawLine(cx + 9, y + 4, cx + 13, y + 8, colour);
  }

  static void drawMiddleArc(
    Arduino_GFX* target, int cx, int y, uint16_t colour) {
    target->drawLine(cx - 9, y + 11, cx - 5, y + 8, colour);
    target->drawLine(cx - 5, y + 8, cx, y + 6, colour);
    target->drawLine(cx, y + 6, cx + 5, y + 8, colour);
    target->drawLine(cx + 5, y + 8, cx + 9, y + 11, colour);
  }

  static void drawInnerArc(
    Arduino_GFX* target, int cx, int y, uint16_t colour) {
    target->drawLine(cx - 4, y + 14, cx, y + 11, colour);
    target->drawLine(cx, y + 11, cx + 4, y + 14, colour);
  }

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
  bool wifiConnected_;
  uint8_t wifiStrengthLevel_;
};
