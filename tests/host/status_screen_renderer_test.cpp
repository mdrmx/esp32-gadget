#include <stdio.h>
#include <stdlib.h>

#include "../../src/ui/StatusScreenRenderer.h"

static void check(bool condition, const char* expression, int line) {
  if (!condition) {
    fprintf(stderr, "FAIL: %s (line %d)\n", expression, line);
    exit(EXIT_FAILURE);
  }
}

#define CHECK(expression) check((expression), #expression, __LINE__)

int main() {
  const uint16_t BG = 0x1082;
  const uint16_t PANEL = 0x2128;
  const uint16_t WHITE = 0xFFFF;
  const uint16_t ACCENT = 0x5DDF;
  const uint16_t MUTED = 0xBDF7;
  const uint16_t ERROR = 0xF986;
  const StatusScreenRenderer::Colours colours = {
    BG, PANEL, WHITE, ACCENT, WHITE, ERROR, MUTED
  };

  DisplayManager displayManager;
  CHECK(displayManager.begin());

  TextRenderer textRenderer(displayManager);
  HeaderRenderer headerRenderer(displayManager, textRenderer);
  StatusScreenRenderer renderer(
    displayManager, headerRenderer, textRenderer);

  Arduino_GFX* physicalTarget = displayManager.target();
  CHECK(physicalTarget != nullptr);
  CHECK(displayManager.width() == 320);

  renderer.drawLoading(
    "Manchester", "15:30", "Connecting Wi-Fi...", colours);

  CHECK(physicalTarget->fillScreenCount == 1);
  CHECK(physicalTarget->lastFillScreenColour == BG);
  CHECK(physicalTarget->fillRectCount == 1);
  CHECK(physicalTarget->lastFillRectX == 0);
  CHECK(physicalTarget->lastFillRectY == 0);
  CHECK(physicalTarget->lastFillRectWidth == 320);
  CHECK(physicalTarget->lastFillRectHeight == 36);
  CHECK(physicalTarget->lastFillRectColour == PANEL);
  CHECK(physicalTarget->horizontalLineCount == 1);
  CHECK(physicalTarget->lastLineY == 36);
  CHECK(physicalTarget->lastLineColour == ACCENT);

  CHECK(physicalTarget->printCount == 4);
  CHECK(physicalTarget->printedText[0] == String("Manchester"));
  CHECK(physicalTarget->printedText[1] == String("15:30"));
  CHECK(physicalTarget->printedText[2] == String("OpenWeatherMap"));
  CHECK(physicalTarget->printedX[2] == 76);
  CHECK(physicalTarget->printedY[2] == 83);
  CHECK(physicalTarget->printedSize[2] == 2);
  CHECK(physicalTarget->printedColour[2] == ACCENT);
  CHECK(physicalTarget->printedText[3] == String("Connecting Wi-Fi..."));
  CHECK(physicalTarget->printedY[3] == 119);
  CHECK(physicalTarget->printedSize[3] == 1);
  CHECK(physicalTarget->printedColour[3] == WHITE);

  renderer.drawError(
    "Manchester", "15:31", "Wi-Fi unavailable", colours);

  CHECK(physicalTarget->fillScreenCount == 2);
  CHECK(physicalTarget->lastFillScreenColour == BG);
  CHECK(physicalTarget->fillRectCount == 2);
  CHECK(physicalTarget->horizontalLineCount == 2);
  CHECK(physicalTarget->printCount == 9);
  CHECK(physicalTarget->printedText[6] == String("WEATHER ERROR"));
  CHECK(physicalTarget->printedY[6] == 82);
  CHECK(physicalTarget->printedSize[6] == 2);
  CHECK(physicalTarget->printedColour[6] == ERROR);
  CHECK(physicalTarget->printedText[7] == String("Wi-Fi unavailable"));
  CHECK(physicalTarget->printedY[7] == 117);
  CHECK(physicalTarget->printedColour[7] == WHITE);
  CHECK(physicalTarget->printedText[8] == String("Will retry automatically"));
  CHECK(physicalTarget->printedY[8] == 143);
  CHECK(physicalTarget->printedColour[8] == MUTED);

  // A missing active target must not partially clear or draw a status screen.
  const int fillScreenCountBeforeNull = physicalTarget->fillScreenCount;
  const int fillRectCountBeforeNull = physicalTarget->fillRectCount;
  const int printCountBeforeNull = physicalTarget->printCount;
  const int lineCountBeforeNull = physicalTarget->horizontalLineCount;
  displayManager.beginTabTransitionRender();
  CHECK(displayManager.target() == nullptr);
  renderer.drawLoading("No target", "00:00", "Loading", colours);
  renderer.drawError("No target", "00:00", "Failed", colours);
  CHECK(physicalTarget->fillScreenCount == fillScreenCountBeforeNull);
  CHECK(physicalTarget->fillRectCount == fillRectCountBeforeNull);
  CHECK(physicalTarget->printCount == printCountBeforeNull);
  CHECK(physicalTarget->horizontalLineCount == lineCountBeforeNull);
  displayManager.endTabTransitionRender();

  // Status screens must follow DisplayManager when composing a transition.
  CHECK(displayManager.createTabTransitionCanvas(
    displayManager.width(), displayManager.height()));
  displayManager.beginTabTransitionRender();
  Arduino_GFX* transitionTarget = displayManager.target();
  CHECK(transitionTarget != nullptr);
  CHECK(transitionTarget != physicalTarget);

  renderer.drawLoading(
    "Transition", "15:32", "Starting up...", colours);
  CHECK(transitionTarget->fillScreenCount == 1);
  CHECK(transitionTarget->lastFillScreenColour == BG);
  CHECK(transitionTarget->printCount == 4);
  CHECK(transitionTarget->printedText[0] == String("Transition"));
  CHECK(transitionTarget->printedText[1] == String("15:32"));
  CHECK(transitionTarget->printedText[2] == String("OpenWeatherMap"));
  CHECK(transitionTarget->printedText[3] == String("Starting up..."));
  CHECK(physicalTarget->fillScreenCount == fillScreenCountBeforeNull);
  CHECK(physicalTarget->printCount == printCountBeforeNull);

  displayManager.endTabTransitionRender();
  CHECK(displayManager.target() == physicalTarget);

  printf("StatusScreenRenderer tests passed.\n");
  return EXIT_SUCCESS;
}
