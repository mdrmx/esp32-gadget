#pragma once

#include <Arduino.h>
#include "../hardware/DisplayManager.h"
#include "HeaderRenderer.h"
#include "TextRenderer.h"

// Owns the loading/error screen composition only. Weather supplies resolved
// header strings, status text and colours; the renderer owns no app or service
// state and follows DisplayManager's active physical/transition target.
class StatusScreenRenderer {
public:
  struct Colours {
    uint16_t background;
    uint16_t panel;
    uint16_t locationText;
    uint16_t accent;
    uint16_t bodyText;
    uint16_t errorText;
    uint16_t mutedText;
  };

  StatusScreenRenderer(
    DisplayManager& displayManager,
    HeaderRenderer& headerRenderer,
    TextRenderer& textRenderer
  )
    : displayManager_(displayManager),
      headerRenderer_(headerRenderer),
      textRenderer_(textRenderer) {}

  void drawLoading(
    const String& locationText,
    const String& clockText,
    const String& message,
    const Colours& colours
  ) const {
    if (!beginScreen(locationText, clockText, colours)) return;

    textRenderer_.drawCenteredText(
      "OpenWeatherMap", 83, 2, colours.accent);
    textRenderer_.drawCenteredText(
      message, 119, 1, colours.bodyText);
  }

  void drawError(
    const String& locationText,
    const String& clockText,
    const String& message,
    const Colours& colours
  ) const {
    if (!beginScreen(locationText, clockText, colours)) return;

    textRenderer_.drawCenteredText(
      "WEATHER ERROR", 82, 2, colours.errorText);
    textRenderer_.drawCenteredText(
      message, 117, 1, colours.bodyText);
    textRenderer_.drawCenteredText(
      "Will retry automatically", 143, 1, colours.mutedText);
  }

private:
  bool beginScreen(
    const String& locationText,
    const String& clockText,
    const Colours& colours
  ) const {
    Arduino_GFX* target = displayManager_.target();
    if (target == nullptr) return false;

    target->fillScreen(colours.background);
    headerRenderer_.draw(
      locationText,
      clockText,
      colours.panel,
      colours.locationText,
      colours.accent);
    return true;
  }

  DisplayManager& displayManager_;
  HeaderRenderer& headerRenderer_;
  TextRenderer& textRenderer_;
};
