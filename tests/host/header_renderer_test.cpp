#include <stdio.h>
#include <stdlib.h>

#include "../../src/ui/HeaderRenderer.h"

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
  const uint16_t ACCENT = 0x5DDF;

  DisplayManager displayManager;
  CHECK(displayManager.begin());

  TextRenderer textRenderer(displayManager);
  HeaderRenderer renderer(displayManager, textRenderer);
  Arduino_GFX* physicalTarget = displayManager.target();
  CHECK(physicalTarget != nullptr);
  CHECK(displayManager.width() == 320);

  renderer.draw("Manchester", "13:58", PANEL, WHITE, ACCENT);

  CHECK(physicalTarget->fillRectCount == 1);
  CHECK(physicalTarget->lastFillRectX == 0);
  CHECK(physicalTarget->lastFillRectY == 0);
  CHECK(physicalTarget->lastFillRectWidth == 320);
  CHECK(physicalTarget->lastFillRectHeight == 36);
  CHECK(physicalTarget->lastFillRectColour == PANEL);

  CHECK(physicalTarget->printCount == 2);
  CHECK(physicalTarget->printedText[0] == String("Manchester"));
  CHECK(physicalTarget->printedX[0] == 10);
  CHECK(physicalTarget->printedY[0] == 10);
  CHECK(physicalTarget->printedSize[0] == 2);
  CHECK(physicalTarget->printedColour[0] == WHITE);
  CHECK(physicalTarget->printedText[1] == String("13:58"));
  CHECK(physicalTarget->printedX[1] == 250);
  CHECK(physicalTarget->printedY[1] == 10);
  CHECK(physicalTarget->printedSize[1] == 2);
  CHECK(physicalTarget->printedColour[1] == ACCENT);

  CHECK(physicalTarget->horizontalLineCount == 1);
  CHECK(physicalTarget->lastLineX == 0);
  CHECK(physicalTarget->lastLineY == 36);
  CHECK(physicalTarget->lastLineWidth == 320);
  CHECK(physicalTarget->lastLineColour == ACCENT);

  renderer.drawClockOnly("14:00", PANEL, ACCENT);
  CHECK(physicalTarget->fillRectCount == 2);
  CHECK(physicalTarget->lastFillRectX == 242);
  CHECK(physicalTarget->lastFillRectY == 0);
  CHECK(physicalTarget->lastFillRectWidth == 78);
  CHECK(physicalTarget->lastFillRectHeight == 36);
  CHECK(physicalTarget->lastFillRectColour == PANEL);
  CHECK(physicalTarget->horizontalLineCount == 1);
  CHECK(physicalTarget->printCount == 3);
  CHECK(physicalTarget->printedText[2] == String("14:00"));
  CHECK(physicalTarget->printedX[2] == 250);
  CHECK(physicalTarget->printedY[2] == 10);
  CHECK(physicalTarget->printedColour[2] == ACCENT);

  // Preserve the existing minimum x position when the clock text is wide.
  renderer.drawClockOnly("123456789012", PANEL, ACCENT);
  CHECK(physicalTarget->printedX[3] == 250);

  // A null active target must not cause a dereference or partial drawing.
  const int fillCountBeforeNull = physicalTarget->fillRectCount;
  const int printCountBeforeNull = physicalTarget->printCount;
  const int lineCountBeforeNull = physicalTarget->horizontalLineCount;
  displayManager.beginTabTransitionRender();
  CHECK(displayManager.target() == nullptr);
  renderer.draw("No target", "00:00", PANEL, WHITE, ACCENT);
  renderer.drawClockOnly("00:00", PANEL, ACCENT);
  CHECK(physicalTarget->fillRectCount == fillCountBeforeNull);
  CHECK(physicalTarget->printCount == printCountBeforeNull);
  CHECK(physicalTarget->horizontalLineCount == lineCountBeforeNull);
  displayManager.endTabTransitionRender();

  // Header rendering must follow DisplayManager to the transition canvas.
  CHECK(displayManager.createTabTransitionCanvas(
    displayManager.width(), displayManager.height()));
  displayManager.beginTabTransitionRender();
  Arduino_GFX* transitionTarget = displayManager.target();
  CHECK(transitionTarget != nullptr);
  CHECK(transitionTarget != physicalTarget);

  renderer.draw("Transition", "15:01", PANEL, WHITE, ACCENT);
  CHECK(transitionTarget->fillRectCount == 1);
  CHECK(transitionTarget->printCount == 2);
  CHECK(transitionTarget->printedText[0] == String("Transition"));
  CHECK(transitionTarget->printedText[1] == String("15:01"));
  CHECK(transitionTarget->horizontalLineCount == 1);
  CHECK(physicalTarget->lastPrinted == String("123456789012"));

  displayManager.endTabTransitionRender();
  CHECK(displayManager.target() == physicalTarget);

  printf("HeaderRenderer tests passed.\n");
  return EXIT_SUCCESS;
}
