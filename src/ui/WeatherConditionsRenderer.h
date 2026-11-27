#pragma once

#include <Arduino.h>
#include "TextRenderer.h"

// Lays out the Weather app's current-conditions column. The sketch remains
// responsible for selecting/formatting the location and weather data, drawing
// the provider icon, composing the right-hand metric cards and managing the
// screen/canvas lifecycle.
class WeatherConditionsRenderer {
public:
  struct Colours {
    uint16_t location;
    uint16_t description;
    uint16_t temperature;
    uint16_t feelsLike;
  };

  explicit WeatherConditionsRenderer(TextRenderer& textRenderer)
    : textRenderer_(textRenderer) {}

  void draw(
    const String& location,
    const String& description,
    const String& temperatureText,
    const String& feelsLikeText,
    const Colours& colours
  ) const {
    const String fittedLocation = fitLocationText(location);
    if (fittedLocation.length() > 0) {
      textRenderer_.drawText(
        fittedLocation,
        LOCATION_X,
        LOCATION_Y,
        LOCATION_TEXT_SIZE,
        colours.location);
    }

    // The large temperature and smaller feels-like line sit to the right of
    // the compact provider icon; the description remains centred beneath it.
    textRenderer_.drawText(
      temperatureText,
      TEMPERATURE_X,
      TEMPERATURE_Y,
      TEMPERATURE_TEXT_SIZE,
      colours.temperature);
    textRenderer_.drawText(
      feelsLikeText,
      FEELS_LIKE_X,
      FEELS_LIKE_Y,
      FEELS_LIKE_TEXT_SIZE,
      colours.feelsLike);

    textRenderer_.drawWrappedCenteredText(
      description,
      DESCRIPTION_CENTRE_X,
      DESCRIPTION_MAX_WIDTH,
      DESCRIPTION_TOP,
      DESCRIPTION_TEXT_SIZE,
      colours.description);
  }

private:
  enum {
    LOCATION_X = 8,
    LOCATION_Y = 47,
    LOCATION_MAX_WIDTH = 176,
    LOCATION_TEXT_SIZE = 2,
    TEMPERATURE_X = 96,
    TEMPERATURE_Y = 96,
    TEMPERATURE_TEXT_SIZE = 2,
    FEELS_LIKE_X = 92,
    FEELS_LIKE_Y = 122,
    FEELS_LIKE_TEXT_SIZE = 1,
    DESCRIPTION_CENTRE_X = 54,
    DESCRIPTION_MAX_WIDTH = 112,
    DESCRIPTION_TOP = 169,
    DESCRIPTION_TEXT_SIZE = 1
  };

  String fitLocationText(const String& location) const {
    String fitted = location;
    if (textRenderer_.measureTextWidth(fitted, LOCATION_TEXT_SIZE) <=
        LOCATION_MAX_WIDTH) {
      return fitted;
    }

    while (fitted.length() > 1 &&
           textRenderer_.measureTextWidth(fitted + "...", LOCATION_TEXT_SIZE) >
               LOCATION_MAX_WIDTH) {
      fitted.remove(fitted.length() - 1, 1);
    }
    if (fitted.length() < location.length() && fitted.length() > 1) {
      fitted += "...";
    }
    return fitted;
  }

  TextRenderer& textRenderer_;
};
