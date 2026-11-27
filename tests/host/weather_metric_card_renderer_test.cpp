#include <stdio.h>
#include <stdlib.h>

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

// Simulate Arduino variants that expose function-like min/max macros. The
// renderer must remain safe when included after those macros are defined.
#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))
#include "../../src/ui/WeatherMetricCardRenderer.h"
#undef min
#undef max

static void check(bool condition, const char* expression, int line) {
  if (!condition) {
    fprintf(stderr, "FAIL: %s (line %d)\n", expression, line);
    exit(EXIT_FAILURE);
  }
}

#define CHECK(expression) check((expression), #expression, __LINE__)

class TestGfx : public Arduino_GFX {
public:
  TestGfx() : Arduino_GFX(320, 240) {}
};

int main() {
  const uint16_t PAPER = 0xFF9C;
  const uint16_t INK = 0x28C1;
  const WeatherMetricCardRenderer::Colours colours = {
    PAPER, INK, INK, INK
  };

  WeatherMetricCardRenderer renderer(colours);
  TestGfx target;

  // Full metric card: geometry, label position and centred value are stable.
  renderer.draw(
    &target, "HUMIDITY", "64%", 192, 49, 118, 47, 177);

  CHECK(target.fillRoundRectCount == 1);
  CHECK(target.lastFillRoundRectX == 194);
  CHECK(target.lastFillRoundRectY == 49);
  CHECK(target.lastFillRoundRectWidth == 114);
  CHECK(target.lastFillRoundRectHeight == 54);
  CHECK(target.lastFillRoundRectRadius == 6);
  CHECK(target.lastFillRoundRectColour == PAPER);
  CHECK(target.drawRoundRectCount == 1);
  CHECK(target.lastDrawRoundRectColour == INK);

  CHECK(target.printCount == 2);
  CHECK(target.printedText[0] == String("HUMIDITY"));
  CHECK(target.printedX[0] == 201);
  CHECK(target.printedY[0] == 56);
  CHECK(target.printedSize[0] == 1);
  CHECK(target.printedColour[0] == INK);

  CHECK(target.printedText[1] == String("64%"));
  CHECK(target.printedX[1] == 233);
  CHECK(target.printedY[1] == 72);
  CHECK(target.printedSize[1] == 2);
  CHECK(target.printedColour[1] == INK);

  // Longer values keep the original smaller type size and vertical offset.
  renderer.draw(
    &target, "CLOUD COVER", "12.4 km", 192, 108, 118, 47, 177);
  CHECK(target.fillRoundRectCount == 2);
  CHECK(target.printCount == 4);
  CHECK(target.printedText[2] == String("CLOUD COVER"));
  CHECK(target.printedText[3] == String("12.4 km"));
  CHECK(target.printedX[3] == 230);
  CHECK(target.printedY[3] == 136);
  CHECK(target.printedSize[3] == 1);
  CHECK(target.printedColour[3] == INK);

  // A card clipped at the top draws its panel slice and only the value,
  // because its label would be above the viewport.
  const int roundRectsBeforeTopClip = target.fillRoundRectCount;
  const int fillRectsBeforeTopClip = target.fillRectCount;
  const int printsBeforeTopClip = target.printCount;
  renderer.draw(
    &target, "PRESSURE", "1013 hPa", 192, 35, 118, 47, 177);
  CHECK(target.fillRoundRectCount == roundRectsBeforeTopClip);
  CHECK(target.fillRectCount == fillRectsBeforeTopClip + 1);
  CHECK(target.drawFastVLineCount >= 2);
  CHECK(target.lastFillRectX == 194);
  CHECK(target.lastFillRectY == 47);
  CHECK(target.lastFillRectWidth == 114);
  CHECK(target.lastFillRectHeight == 42);
  CHECK(target.lastFillRectColour == PAPER);
  CHECK(target.printCount == printsBeforeTopClip + 1);
  CHECK(target.printedText[printsBeforeTopClip] == String("1013 hPa"));
  CHECK(target.printedY[printsBeforeTopClip] == 63);

  // A card clipped at the bottom draws the label but omits a value whose
  // full glyph height would extend below the viewport.
  const int printsBeforeBottomClip = target.printCount;
  const int fillRectsBeforeBottomClip = target.fillRectCount;
  renderer.draw(
    &target, "WIND", "14 km/h", 192, 197, 118, 47, 177);
  CHECK(target.fillRectCount == fillRectsBeforeBottomClip + 1);
  CHECK(target.lastFillRectY == 197);
  CHECK(target.lastFillRectHeight == 27);
  CHECK(target.printCount == printsBeforeBottomClip + 1);
  CHECK(target.printedText[printsBeforeBottomClip] == String("WIND"));
  CHECK(target.printedY[printsBeforeBottomClip] == 204);

  // Fully offscreen cards and invalid targets/viewports are safe no-ops.
  const int fillRectsBeforeOutside = target.fillRectCount;
  const int roundRectsBeforeOutside = target.fillRoundRectCount;
  const int printsBeforeOutside = target.printCount;
  renderer.draw(
    &target, "UV INDEX", "3.2", 192, -20, 118, 47, 177);
  renderer.draw(
    &target, "UV INDEX", "3.2", 192, 230, 118, 47, 177);
  renderer.draw(
    &target, "UV INDEX", "3.2", 192, 49, 118, 47, 0);
  renderer.draw(
    nullptr, "UV INDEX", "3.2", 192, 49, 118, 47, 177);
  CHECK(target.fillRectCount == fillRectsBeforeOutside);
  CHECK(target.fillRoundRectCount == roundRectsBeforeOutside);
  CHECK(target.printCount == printsBeforeOutside);

  printf("WeatherMetricCardRenderer tests passed.\n");
  return EXIT_SUCCESS;
}
