#include <stdio.h>
#include <stdlib.h>

#include "../../src/ui/TextRenderer.h"

static void check(bool condition, const char* expression, int line) {
  if (!condition) {
    fprintf(stderr, "FAIL: %s (line %d)\n", expression, line);
    exit(EXIT_FAILURE);
  }
}

#define CHECK(expression) check((expression), #expression, __LINE__)

int main() {
  DisplayManager displayManager;
  CHECK(displayManager.begin());

  TextRenderer renderer(displayManager);
  Arduino_GFX* target = displayManager.target();
  CHECK(target != nullptr);

  renderer.drawText("Hello", 12, 18, 2, 0x1234);
  CHECK(target->textSize == 2);
  CHECK(target->textColour == 0x1234);
  CHECK(target->cursorX == 12);
  CHECK(target->cursorY == 18);
  CHECK(target->lastPrinted == String("Hello"));

  renderer.drawCenteredText("Hi", 40, 2, 0x4321);
  CHECK(target->cursorX == 148);
  CHECK(target->cursorY == 40);
  CHECK(target->lastPrinted == String("Hi"));
  CHECK(target->textColour == 0x4321);

  CHECK(renderer.measureTextWidth("abc", 2) == 36);
  CHECK(renderer.measureTextHeight("abc", 2) == 16);

  const int wrappedHeight = renderer.drawWrappedCenteredText(
    "heavy rain showers ahead", 160, 60, 20, 1, 0x00FF);
  CHECK(wrappedHeight == 16);
  CHECK(target->printCount == 4);
  CHECK(target->lastPrinted == String("showers..."));
  CHECK(target->cursorX == 130);
  CHECK(target->cursorY == 28);

  const int lineHeight = renderer.drawCenteredWeatherLine(
    "rain", 160, 44, 2, 0x0F0F);
  CHECK(lineHeight == 16);
  CHECK(target->cursorX == 136);
  CHECK(target->cursorY == 44);
  CHECK(target->lastPrinted == String("rain"));

  CHECK(displayManager.createTabTransitionCanvas(
    displayManager.width(), displayManager.height()));
  displayManager.beginTabTransitionRender();
  Arduino_GFX* transitionTarget = displayManager.target();
  CHECK(transitionTarget != nullptr);
  CHECK(transitionTarget != target);

  renderer.drawText("transition", 3, 4, 1, 0xFFFF);
  CHECK(transitionTarget->lastPrinted == String("transition"));
  CHECK(target->lastPrinted == String("rain"));

  displayManager.endTabTransitionRender();
  CHECK(displayManager.target() == target);
  renderer.drawText("physical", 5, 6, 1, 0xAAAA);
  CHECK(target->lastPrinted == String("physical"));

  printf("TextRenderer tests passed.\n");
  return EXIT_SUCCESS;
}
