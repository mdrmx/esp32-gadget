#include <stdio.h>
#include <stdlib.h>

#include "../../src/ui/ForecastListItemRenderer.h"

static void check(bool condition, const char* expression, int line) {
  if (!condition) {
    fprintf(stderr, "FAIL: %s (line %d)\n", expression, line);
    exit(EXIT_FAILURE);
  }
}

#define CHECK(expression) check((expression), #expression, __LINE__)

int main() {
  const uint16_t PANEL = 0x2128;
  const uint16_t WHITE = 0xFFFF;
  const uint16_t YELLOW = 0xFEA0;
  const uint16_t MUTED = 0xBDF7;
  const uint16_t ACCENT = 0x5DDF;
  const ForecastListItemRenderer::Colours colours = {
    PANEL, WHITE, YELLOW, MUTED, ACCENT
  };

  DisplayManager displayManager;
  CHECK(displayManager.begin());
  ForecastListItemRenderer renderer(displayManager, colours);
  Arduino_GFX* target = displayManager.target();
  CHECK(target != nullptr);

  ForecastListItemRenderer::Item clearItem = {
    "15:00", "Clear", 18.6f, 17.4f, 0.256f
  };
  renderer.draw(target, clearItem, 72);

  CHECK(target->fillRoundRectCount == 1);
  CHECK(target->lastFillRoundRectX == 4);
  CHECK(target->lastFillRoundRectY == 72);
  CHECK(target->lastFillRoundRectWidth == 312);
  CHECK(target->lastFillRoundRectHeight == 44);
  CHECK(target->lastFillRoundRectRadius == 4);
  CHECK(target->lastFillRoundRectColour == PANEL);
  CHECK(target->printCount == 4);
  CHECK(target->printedText[0] == String("15:00"));
  CHECK(target->printedX[0] == 10);
  CHECK(target->printedY[0] == 90);
  CHECK(target->printedText[1] == String("19C"));
  CHECK(target->printedX[1] == 72);
  CHECK(target->printedY[1] == 86);
  CHECK(target->printedSize[1] == 2);
  CHECK(target->printedColour[1] == YELLOW);
  CHECK(target->printedText[2] == String("FL 17C"));
  CHECK(target->printedX[2] == 145);
  CHECK(target->printedText[3] == String("POP 26%"));
  CHECK(target->printedX[3] == 250);
  CHECK(target->fillCircleCount == 1);
  CHECK(target->lastFillCircleColour == YELLOW);
  CHECK(target->drawFastVLineCount == 2);

  ForecastListItemRenderer::Item rainItem = {
    "16:00", "Rain", 12.2f, 11.8f, 1.25f
  };
  renderer.draw(target, rainItem, 72);
  CHECK(target->printCount == 8);
  CHECK(target->printedText[7] == String("POP 100%"));
  CHECK(target->drawLineCount >= 3);
  CHECK(target->lastLineColour == ACCENT);

  ForecastListItemRenderer::Item clippedItem = {
    "17:00", "Clouds", 13.0f, 12.0f, -0.2f
  };
  const int fillsBeforeClip = target->fillRectCount;
  const int printsBeforeClip = target->printCount;
  renderer.drawClipped(target, clippedItem, 50, 64, 152);
  CHECK(target->fillRectCount == fillsBeforeClip + 1);
  CHECK(target->lastFillRectX == 4);
  CHECK(target->lastFillRectY == 0);
  CHECK(target->lastFillRectWidth == 312);
  CHECK(target->lastFillRectHeight == 30);
  CHECK(target->printCount == printsBeforeClip + 3);
  CHECK(target->printedText[printsBeforeClip] == String("17:00"));
  CHECK(target->printedText[printsBeforeClip + 1] == String("FL 12C"));
  CHECK(target->printedText[printsBeforeClip + 2] == String("POP 0%"));

  // A card outside the viewport produces no drawing calls.
  const int fillsBeforeOutside = target->fillRectCount;
  const int printsBeforeOutside = target->printCount;
  renderer.drawClipped(target, clippedItem, 20, 64, 152);
  CHECK(target->fillRectCount == fillsBeforeOutside);
  CHECK(target->printCount == printsBeforeOutside);

  // Null targets are safe and do not mutate the last recorded drawing.
  renderer.draw(nullptr, clippedItem, 64);
  renderer.drawClipped(nullptr, clippedItem, 64, 64, 152);
  CHECK(target->fillRectCount == fillsBeforeOutside);
  CHECK(target->printCount == printsBeforeOutside);

  printf("ForecastListItemRenderer tests passed.\n");
  return EXIT_SUCCESS;
}
