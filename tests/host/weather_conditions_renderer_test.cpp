#include <stdio.h>
#include <stdlib.h>

#include "../../src/ui/WeatherConditionsRenderer.h"

static void check(bool condition, const char* expression, int line) {
  if (!condition) {
    fprintf(stderr, "FAIL: %s (line %d)\n", expression, line);
    exit(EXIT_FAILURE);
  }
}

#define CHECK(expression) check((expression), #expression, __LINE__)

int main() {
  const uint16_t MUTED = 0xBDF7;
  const uint16_t YELLOW = 0xFEA0;
  const WeatherConditionsRenderer::Colours colours = {
    MUTED, YELLOW, MUTED
  };

  DisplayManager displayManager;
  CHECK(displayManager.begin());

  TextRenderer textRenderer(displayManager);
  WeatherConditionsRenderer renderer(textRenderer);
  Arduino_GFX* physicalTarget = displayManager.target();
  CHECK(physicalTarget != nullptr);
  CHECK(displayManager.width() == 320);

  // With a provider icon 42 px high, the description begins at y=72:
  // 37 px content origin + 42 px icon - 7 px overlap.
  renderer.draw("Clear sky", "18.2 C", "Feels 17.5 C", 42, colours);

  CHECK(physicalTarget->printCount == 3);
  CHECK(physicalTarget->printedText[0] == String("Clear sky"));
  CHECK(physicalTarget->printedX[0] == 42);
  CHECK(physicalTarget->printedY[0] == 72);
  CHECK(physicalTarget->printedSize[0] == 2);
  CHECK(physicalTarget->printedColour[0] == MUTED);

  CHECK(physicalTarget->printedText[1] == String("18.2 C"));
  CHECK(physicalTarget->printedX[1] == 60);
  CHECK(physicalTarget->printedY[1] == 98);
  CHECK(physicalTarget->printedSize[1] == 2);
  CHECK(physicalTarget->printedColour[1] == YELLOW);

  CHECK(physicalTarget->printedText[2] == String("Feels 17.5 C"));
  CHECK(physicalTarget->printedX[2] == 60);
  CHECK(physicalTarget->printedY[2] == 122);
  CHECK(physicalTarget->printedSize[2] == 1);
  CHECK(physicalTarget->printedColour[2] == MUTED);

  // A two-line description moves both temperature lines down by one
  // measured line-height. A missing icon keeps the original y=37 origin.
  renderer.draw(
    "Partly cloudy with showers",
    "16.4 C",
    "Feels 15.8 C",
    0,
    colours);

  CHECK(physicalTarget->printCount == 7);
  CHECK(physicalTarget->printedText[3] == String("Partly cloudy"));
  CHECK(physicalTarget->printedX[3] == 18);
  CHECK(physicalTarget->printedY[3] == 37);
  CHECK(physicalTarget->printedText[4] == String("with showers"));
  CHECK(physicalTarget->printedX[4] == 24);
  CHECK(physicalTarget->printedY[4] == 53);
  CHECK(physicalTarget->printedText[5] == String("16.4 C"));
  CHECK(physicalTarget->printedY[5] == 79);
  CHECK(physicalTarget->printedText[6] == String("Feels 15.8 C"));
  CHECK(physicalTarget->printedY[6] == 103);

  // An absent render target is a safe no-op through TextRenderer.
  const int printsBeforeNull = physicalTarget->printCount;
  displayManager.beginTabTransitionRender();
  CHECK(displayManager.target() == nullptr);
  renderer.draw("No target", "0.0 C", "Feels 0.0 C", 20, colours);
  CHECK(physicalTarget->printCount == printsBeforeNull);
  displayManager.endTabTransitionRender();

  // The same renderer follows DisplayManager to its transition canvas.
  CHECK(displayManager.createTabTransitionCanvas(
    displayManager.width(), displayManager.height()));
  displayManager.beginTabTransitionRender();
  Arduino_GFX* transitionTarget = displayManager.target();
  CHECK(transitionTarget != nullptr);
  CHECK(transitionTarget != physicalTarget);

  renderer.draw("Transition", "20.0 C", "Feels 19.0 C", 30, colours);
  CHECK(transitionTarget->printCount == 3);
  CHECK(transitionTarget->printedText[0] == String("Transition"));
  CHECK(transitionTarget->printedText[1] == String("20.0 C"));
  CHECK(transitionTarget->printedText[2] == String("Feels 19.0 C"));
  CHECK(physicalTarget->printCount == printsBeforeNull);

  displayManager.endTabTransitionRender();
  CHECK(displayManager.target() == physicalTarget);

  printf("WeatherConditionsRenderer tests passed.\n");
  return EXIT_SUCCESS;
}
