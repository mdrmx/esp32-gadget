#include <stdio.h>
#include <stdlib.h>

#include "../../src/ui/ForecastModeSelectorRenderer.h"

static void check(bool condition, const char* expression, int line) {
  if (!condition) {
    fprintf(stderr, "FAIL: %s (line %d)\n", expression, line);
    exit(EXIT_FAILURE);
  }
}

#define CHECK(expression) check((expression), #expression, __LINE__)

int main() {
  const uint16_t PAPER = 0xFF9C;
  const uint16_t BURNT_ORANGE = 0xF56A;
  const uint16_t INK = 0x28C1;

  DisplayManager displayManager;
  TextRenderer textRenderer(displayManager);
  const ForecastModeSelectorRenderer::Colours colours = {
    PAPER, BURNT_ORANGE, INK, INK
  };
  ForecastModeSelectorRenderer renderer(
    displayManager, textRenderer, colours);

  // No active render target must be a safe no-op.
  renderer.draw(ForecastModeSelectorRenderer::Mode::List);
  CHECK(displayManager.target() == nullptr);

  CHECK(displayManager.begin());
  Arduino_GFX* physicalTarget = displayManager.target();
  CHECK(physicalTarget != nullptr);

  renderer.draw(ForecastModeSelectorRenderer::Mode::List);
  CHECK(physicalTarget->fillRoundRectCount == 2);
  CHECK(physicalTarget->drawRoundRectCount == 2);
  CHECK(physicalTarget->drawRoundRectColour[0] == INK);
  CHECK(physicalTarget->drawRoundRectColour[1] == INK);
  CHECK(physicalTarget->fillRoundRectX[0] == 179);
  CHECK(physicalTarget->fillRoundRectY[0] == 41);
  CHECK(physicalTarget->fillRoundRectWidth[0] == 137);
  CHECK(physicalTarget->fillRoundRectHeight[0] == 26);
  CHECK(physicalTarget->fillRoundRectRadius[0] == 6);
  CHECK(physicalTarget->fillRoundRectColour[0] == PAPER);
  // The selected half sits one pixel inside the track outline on every
  // outer edge, with no extra exposed border along the left or bottom.
  CHECK(physicalTarget->fillRoundRectX[1] == 180);
  CHECK(physicalTarget->fillRoundRectY[1] == 42);
  CHECK(physicalTarget->fillRoundRectWidth[1] == 67);
  CHECK(physicalTarget->fillRoundRectHeight[1] == 24);
  CHECK(physicalTarget->fillRoundRectRadius[1] == 5);
  CHECK(physicalTarget->fillRoundRectColour[1] == BURNT_ORANGE);

  CHECK(physicalTarget->printCount == 2);
  CHECK(physicalTarget->printedText[0] == String("LIST"));
  CHECK(physicalTarget->printedX[0] == 201);
  CHECK(physicalTarget->printedY[0] == 50);
  CHECK(physicalTarget->printedSize[0] == 1);
  CHECK(physicalTarget->printedColour[0] == INK);
  CHECK(physicalTarget->printedText[1] == String("GRAPH"));
  CHECK(physicalTarget->printedX[1] == 265);
  CHECK(physicalTarget->printedY[1] == 50);
  CHECK(physicalTarget->printedSize[1] == 1);
  CHECK(physicalTarget->printedColour[1] == INK);

  // Graph mode moves the selected segment without changing the track or labels.
  renderer.draw(ForecastModeSelectorRenderer::Mode::Graph);
  CHECK(physicalTarget->fillRoundRectCount == 4);
  CHECK(physicalTarget->drawRoundRectCount == 4);
  CHECK(physicalTarget->fillRoundRectX[2] == 179);
  CHECK(physicalTarget->fillRoundRectY[2] == 41);
  CHECK(physicalTarget->fillRoundRectWidth[2] == 137);
  CHECK(physicalTarget->fillRoundRectHeight[2] == 26);
  CHECK(physicalTarget->fillRoundRectX[3] == 247);
  CHECK(physicalTarget->fillRoundRectY[3] == 42);
  CHECK(physicalTarget->fillRoundRectWidth[3] == 68);
  CHECK(physicalTarget->fillRoundRectHeight[3] == 24);
  CHECK(physicalTarget->fillRoundRectColour[3] == BURNT_ORANGE);
  CHECK(physicalTarget->printCount == 4);
  CHECK(physicalTarget->printedText[2] == String("LIST"));
  CHECK(physicalTarget->printedColour[2] == INK);
  CHECK(physicalTarget->printedText[3] == String("GRAPH"));
  CHECK(physicalTarget->printedColour[3] == INK);

  // The renderer follows DisplayManager onto the tab-transition canvas.
  CHECK(displayManager.createTabTransitionCanvas(
    displayManager.width(), displayManager.height()));
  displayManager.beginTabTransitionRender();
  Arduino_GFX* transitionTarget = displayManager.target();
  CHECK(transitionTarget != nullptr);
  CHECK(transitionTarget != physicalTarget);

  renderer.draw(ForecastModeSelectorRenderer::Mode::List);
  CHECK(transitionTarget->fillRoundRectCount == 2);
  CHECK(transitionTarget->drawRoundRectCount == 2);
  CHECK(transitionTarget->fillRoundRectX[1] == 180);
  CHECK(transitionTarget->fillRoundRectY[1] == 42);
  CHECK(transitionTarget->fillRoundRectWidth[1] == 67);
  CHECK(transitionTarget->fillRoundRectHeight[1] == 24);
  CHECK(transitionTarget->printCount == 2);
  CHECK(transitionTarget->printedText[0] == String("LIST"));
  CHECK(physicalTarget->fillRoundRectCount == 4);
  CHECK(physicalTarget->printCount == 4);

  displayManager.endTabTransitionRender();
  CHECK(displayManager.target() == physicalTarget);

  printf("ForecastModeSelectorRenderer tests passed.\n");
  return EXIT_SUCCESS;
}
