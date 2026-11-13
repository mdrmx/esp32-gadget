#pragma once

#include <Arduino.h>
#include "../hardware/DisplayManager.h"

// Small text-rendering boundary shared by Weather views. The renderer keeps a
// reference to DisplayManager and resolves target() for each operation, so it
// automatically follows the physical display / tab-transition render target.
// It owns no display, canvas, Weather data, colours or layout configuration.
class TextRenderer {
public:
  explicit TextRenderer(DisplayManager& displayManager)
    : displayManager_(displayManager) {}

  void drawText(
    const String& text,
    int x,
    int y,
    uint8_t size,
    uint16_t colour
  ) const {
    Arduino_GFX* target = displayManager_.target();
    target->setTextSize(size);
    target->setTextColor(colour);
    target->setCursor(x, y);
    target->print(text);
  }

  void drawCenteredText(
    const String& text,
    int y,
    uint8_t size,
    uint16_t colour
  ) const {
    Arduino_GFX* target = displayManager_.target();
    target->setTextSize(size);
    target->setTextColor(colour);

    int16_t x1, y1;
    uint16_t width, height;
    target->getTextBounds(
      text.c_str(), 0, y, &x1, &y1, &width, &height);

    int x = (displayManager_.width() - (int)width) / 2;
    if (x < 4) x = 4;
    if (x + (int)width > displayManager_.width() - 4) {
      x = displayManager_.width() - 4 - width;
    }

    target->setCursor(x, y);
    target->print(text);
  }

  int measureTextWidth(const String& text, uint8_t size) const {
    Arduino_GFX* target = displayManager_.target();
    target->setTextSize(size);

    int16_t x1, y1;
    uint16_t width, height;
    target->getTextBounds(
      text.c_str(), 0, 0, &x1, &y1, &width, &height);
    return (int)width;
  }

  int measureTextHeight(const String& text, uint8_t size) const {
    Arduino_GFX* target = displayManager_.target();
    target->setTextSize(size);

    int16_t x1, y1;
    uint16_t width, height;
    target->getTextBounds(
      text.c_str(), 0, 0, &x1, &y1, &width, &height);
    return (int)height;
  }

  // Wrap to at most two lines around centreX. Returns the rendered block
  // height so the Weather layout can place following data consistently.
  int drawWrappedCenteredText(
    const String& text,
    int centreX,
    int maxWidth,
    int topY,
    uint8_t size,
    uint16_t colour
  ) const {
    String lines[2];
    int lineCount = 0;
    String currentLine;
    int wordStart = 0;

    while (wordStart < (int)text.length()) {
      while (wordStart < (int)text.length() && text[wordStart] == ' ') {
        wordStart++;
      }
      if (wordStart >= (int)text.length()) break;

      int wordEnd = text.indexOf(' ', wordStart);
      if (wordEnd < 0) wordEnd = text.length();
      String word = text.substring(wordStart, wordEnd);
      wordStart = wordEnd + 1;

      String candidate = currentLine.length()
        ? currentLine + " " + word
        : word;
      if (measureTextWidth(candidate, size) <= maxWidth) {
        currentLine = candidate;
      } else if (lineCount == 0 && currentLine.length() > 0) {
        lines[lineCount++] = currentLine;
        currentLine = word;
      } else if (lineCount == 1) {
        // There is no room for another line; shorten the second line neatly.
        String shortened = currentLine;
        while (shortened.length() > 0 &&
               measureTextWidth(shortened + "...", size) > maxWidth) {
          int lastSpace = shortened.lastIndexOf(' ');
          if (lastSpace >= 0) {
            shortened = shortened.substring(0, lastSpace);
          } else {
            shortened = shortened.substring(0, shortened.length() - 1);
          }
        }
        currentLine = shortened + "...";
        break;
      } else {
        currentLine = word;
      }
    }

    if (lineCount < 2 && currentLine.length() > 0) {
      lines[lineCount++] = currentLine;
    }
    if (lineCount == 0) return 0;

    Arduino_GFX* target = displayManager_.target();
    target->setTextSize(size);
    target->setTextColor(colour);
    int lineHeight = measureTextHeight(lines[0], size);
    if (lineHeight <= 0) lineHeight = 8 * size;

    for (int i = 0; i < lineCount; i++) {
      int width = measureTextWidth(lines[i], size);
      drawText(lines[i], centreX - width / 2, topY + i * lineHeight,
               size, colour);
    }

    return lineCount * lineHeight;
  }

  // Draw one line centred around centreX and return its measured height.
  int drawCenteredWeatherLine(
    const String& text,
    int centreX,
    int y,
    uint8_t size,
    uint16_t colour
  ) const {
    Arduino_GFX* target = displayManager_.target();
    target->setTextSize(size);

    int16_t x1, y1;
    uint16_t width, height;
    target->getTextBounds(
      text.c_str(), 0, 0, &x1, &y1, &width, &height);
    drawText(text, centreX - (int)width / 2, y, size, colour);
    return height > 0 ? (int)height : 8 * size;
  }

private:
  DisplayManager& displayManager_;
};
