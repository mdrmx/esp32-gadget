#pragma once

#include <Arduino.h>
#include "TextRenderer.h"

// Composes the current Weather conditions text below the provider icon.
// Weather continues to select/format data, draw the icon, render metric cards
// and manage the screen/canvas lifecycle. This class owns only the vertical
// layout and drawing of the description, temperature and feels-like lines.
class WeatherConditionsRenderer {
public:
  struct Colours {
    uint16_t description;
    uint16_t temperature;
    uint16_t feelsLike;
  };

  explicit WeatherConditionsRenderer(TextRenderer& textRenderer)
    : textRenderer_(textRenderer) {}

  void draw(
    const String& description,
    const String& temperatureText,
    const String& feelsLikeText,
    int iconHeight,
    const Colours& colours
  ) const {
    int contentY = CONTENT_TOP;
    if (iconHeight > 0) {
      contentY += iconHeight + ICON_DESCRIPTION_GAP;
    }

    const int descriptionHeight = textRenderer_.drawWrappedCenteredText(
      description,
      CONTENT_CENTRE_X,
      DESCRIPTION_MAX_WIDTH,
      contentY,
      DESCRIPTION_TEXT_SIZE,
      colours.description);
    contentY += descriptionHeight + DESCRIPTION_TEMPERATURE_GAP;

    const int temperatureHeight = textRenderer_.drawCenteredWeatherLine(
      temperatureText,
      CONTENT_CENTRE_X,
      contentY,
      TEMPERATURE_TEXT_SIZE,
      colours.temperature);
    contentY += temperatureHeight + TEMPERATURE_FEELS_GAP;

    textRenderer_.drawCenteredWeatherLine(
      feelsLikeText,
      CONTENT_CENTRE_X,
      contentY,
      FEELS_LIKE_TEXT_SIZE,
      colours.feelsLike);
  }

private:
  enum {
    CONTENT_TOP = 37,
    CONTENT_CENTRE_X = 96,
    DESCRIPTION_MAX_WIDTH = 176,
    DESCRIPTION_TEXT_SIZE = 2,
    TEMPERATURE_TEXT_SIZE = 2,
    FEELS_LIKE_TEXT_SIZE = 1,
    ICON_DESCRIPTION_GAP = -7,
    DESCRIPTION_TEMPERATURE_GAP = 10,
    TEMPERATURE_FEELS_GAP = 8
  };

  TextRenderer& textRenderer_;
};
