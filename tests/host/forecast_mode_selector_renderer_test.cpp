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
  const uint16_t PANEL = 0x2128;
  const uint16_t WHITE = 0xFFFF;
  const uint16_t BG = 0x1082;
  const uint16_t MUTED = 0xBDF7;

  DisplayManager displayManager;
  TextRenderer textRenderer(displayManager);
  const ForecastModeSelectorRenderer::Colours colours = {
    PANEL, WHITE, BG, MUTED
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
  CHECK(physicalTarget->fillRoundRectX[0] == 179);
  CHECK(physicalTarget->fillRoundRectY[0] == 41);
  CHECK(physicalTarget->fillRoundRectWidth[0] == 137);
  CHECK(physicalTarget->fillRoundRectHeight[0] == 26);
  CHECK(physicalTarget->fillRoundRectRadius[0] == 6);
  CHECK(physicalTarget->fillRoundRectColour[0] == PANEL);
  CHECK(physicalTarget->fillRoundRectX[1] == 181);
  CHECK(physicalTarget->fillRoundRectY[1] == 41);
  CHECK(physicalTarget->fillRoundRectWidth[1] == 67);
  CHECK(physicalTarget->fillRoundRectHeight[1] == 24);
  CHECK(physicalTarget->fillRoundRectRadius[1] == 5);
  CHECK(physicalTarget->fillRoundRectColour[1] == WHITE);

  CHECK(physicalTarget->printCount == 2);
  CHECK(physicalTarget->printedText[0] == String("LIST"));
  CHECK(physicalTarget->printedX[0] == 201);
  CHECK(physicalTarget->printedY[0] == 50);
  CHECK(physicalTarget->printedSize[0] == 1);
  CHECK(physicalTarget->printedColour[0] == BG);
  CHECK(physicalTarget->printedText[1] == String("GRAPH"));
  CHECK(physicalTarget->printedX[1] == 265);
  CHECK(physicalTarget->printedY[1] == 50);
  CHECK(physicalTarget->printedSize[1] == 1);
  CHECK(physicalTarget->printedColour[1] == MUTED);

  // Graph mode moves the selected segment without changing the track or labels.
  renderer.draw(ForecastModeSelectorRenderer::Mode::Graph);
  CHECK(physicalTarget->fillRoundRectCount == 4);
  CHECK(physicalTarget->fillRoundRectX[2] == 179);
  CHECK(physicalTarget->fillRoundRectX[3] == 248);
  CHECK(physicalTarget->fillRoundRectColour[3] == WHITE);
  CHECK(physicalTarget->printCount == 4);
  CHECK(physicalTarget->printedText[2] == String("LIST"));
  CHECK(physicalTarget->printedColour[2] == MUTED);
  CHECK(physicalTarget->printedText[3] == String("GRAPH"));
  CHECK(physicalTarget->printedColour[3] == BG);

  // The renderer follows DisplayManager onto the tab-transition canvas.
  CHECK(displayManager.createTabTransitionCanvas(
    displayManager.width(), displayManager.height()));
  displayManager.beginTabTransitionRender();
  Arduino_GFX* transitionTarget = displayManager.target();
  CHECK(transitionTarget != nullptr);
  CHECK(transitionTarget != physicalTarget);

  renderer.draw(ForecastModeSelectorRenderer::Mode::List);
  CHECK(transitionTarget->fillRoundRectCount == 2);
  CHECK(transitionTarget->fillRoundRectX[1] == 181);
  CHECK(transitionTarget->printCount == 2);
  CHECK(transitionTarget->printedText[0] == String("LIST"));
  CHECK(physicalTarget->fillRoundRectCount == 4);
  CHECK(physicalTarget->printCount == 4);

  displayManager.endTabTransitionRender();
  CHECK(displayManager.target() == physicalTarget);

  printf("ForecastModeSelectorRenderer tests passed.\n");
  return EXIT_SUCCESS;
}
