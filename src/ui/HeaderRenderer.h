#pragma once

#include <Arduino.h>
#include "../hardware/DisplayManager.h"
#include "TextRenderer.h"

// Draws an application-title header and its live Wi-Fi signal indicator.
// The platform supplies the active app title, clock and connection state; this
// class owns text fitting, indicator presentation and the Wi-Fi hit area.
class HeaderRenderer {
public:
  // Visible Wi-Fi glyph bounds. Keep these tight so redrawing the icon
  // does not overwrite the adjacent clock or app-title text.
  enum {
    WiFiIndicatorLeft = 198,
    WiFiIndicatorTop = 4,
    WiFiIndicatorRight = 240,
    WiFiIndicatorBottom = 32
  };

  // Touch-only target for the Wi-Fi indicator; it remains separate from the
  // app title and clock.
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
    const String& titleText,
    const String& clockText,
    uint16_t panelColour,
    uint16_t titleColour,
    uint16_t accentColour
  ) const {
    Arduino_GFX* target = displayManager_.target();
    if (target == nullptr) return;

    target->fillRect(
      0, 0, displayManager_.width(), HEADER_HEIGHT, panelColour);
    textRenderer_.drawText(
      fitTitleText(titleText), TITLE_X, TEXT_Y,
      TEXT_SIZE, titleColour);
    // Use the supplied ink/divider colour for the clock, Wi-Fi state and
    // the slightly heavier two-pixel header rule.
    drawClockText(clockText, accentColour);

    target->drawFastHLine(
      0, HEADER_HEIGHT, displayManager_.width(), accentColour);
    target->drawFastHLine(
      0, HEADER_HEIGHT + 1, displayManager_.width(), accentColour);
    drawWiFiIndicator(panelColour, titleColour, accentColour);
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
  // state changes. The expanded Wi-Fi touch target is independent of the
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
    TITLE_X = 8,
    TEXT_Y = 10,
    TEXT_SIZE = 2,
    TITLE_MAX_WIDTH = WiFiIndicatorLeft - TITLE_X - 8,
    CLOCK_REGION_WIDTH = 78,
    CLOCK_RIGHT_MARGIN = 10,
    CLOCK_MINIMUM_X_MARGIN = 70
  };

  String fitTitleText(const String& titleText) const {
    String fitted = titleText;
    if (textRenderer_.measureTextWidth(fitted, TEXT_SIZE) <=
        TITLE_MAX_WIDTH) {
      return fitted;
    }

    while (fitted.length() > 0 &&
           textRenderer_.measureTextWidth(fitted, TEXT_SIZE) >
               TITLE_MAX_WIDTH) {
      fitted.remove(fitted.length() - 1, 1);
    }

    if (fitted.length() < titleText.length() && fitted.length() > 1) {
      while (fitted.length() > 1 &&
             textRenderer_.measureTextWidth(fitted + ".", TEXT_SIZE) >
                 TITLE_MAX_WIDTH) {
        fitted.remove(fitted.length() - 1, 1);
      }
      fitted += ".";
    }
    return fitted;
  }

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
