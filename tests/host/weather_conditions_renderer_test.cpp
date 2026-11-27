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
  const uint16_t INK = 0x28C1;
  const WeatherConditionsRenderer::Colours colours = {
    INK, INK, INK, INK
  };

  DisplayManager displayManager;
  CHECK(displayManager.begin());

  TextRenderer textRenderer(displayManager);
  WeatherConditionsRenderer renderer(textRenderer);
  Arduino_GFX* physicalTarget = displayManager.target();
  CHECK(physicalTarget != nullptr);
  CHECK(displayManager.width() == 320);

  // The location remains above the vertically stacked condition summary:
  // description, current temperature, then feels-like temperature.
  renderer.draw(
    "Manchester",
    "Clear sky",
    "18.2 C",
    "Feels: 17.5 C",
    colours);

  CHECK(physicalTarget->printCount == 4);
  CHECK(physicalTarget->printedText[0] == String("Manchester"));
  CHECK(physicalTarget->printedX[0] == 8);
  CHECK(physicalTarget->printedY[0] == 47);
  CHECK(physicalTarget->printedSize[0] == 2);
  CHECK(physicalTarget->printedColour[0] == INK);

  CHECK(physicalTarget->printedText[1] == String("Clear sky"));
  CHECK(physicalTarget->printedX[1] == 42);
  CHECK(physicalTarget->printedY[1] == 142);
  CHECK(physicalTarget->printedSize[1] == 2);

  CHECK(physicalTarget->printedText[2] == String("18.2 C"));
  CHECK(physicalTarget->printedX[2] == 60);
  CHECK(physicalTarget->printedY[2] == 162);
  CHECK(physicalTarget->printedSize[2] == 2);

  CHECK(physicalTarget->printedText[3] == String("Feels: 17.5 C"));
  CHECK(physicalTarget->printedX[3] == 60);
  CHECK(physicalTarget->printedY[3] == 182);
  CHECK(physicalTarget->printedSize[3] == 1);

  // Long location labels are shortened to fit the left content column and
  // long descriptions wrap to at most two centred lines above the readings.
  renderer.draw(
    "A very long resolved location name",
    "Partly cloudy with showers",
    "16.4 C",
    "Feels: 15.8 C",
    colours);
  CHECK(physicalTarget->printCount == 9);
  CHECK(physicalTarget->printedText[4].length() <= 14);
  CHECK(physicalTarget->printedX[4] == 8);
  CHECK(physicalTarget->printedY[4] == 47);
  CHECK(physicalTarget->printedText[5] == String("Partly cloudy"));
  CHECK(physicalTarget->printedX[5] == 18);
  CHECK(physicalTarget->printedY[5] == 142);
  CHECK(physicalTarget->printedText[6] == String("with showers"));
  CHECK(physicalTarget->printedX[6] == 24);
  CHECK(physicalTarget->printedY[6] == 158);
  CHECK(physicalTarget->printedText[7] == String("16.4 C"));
  CHECK(physicalTarget->printedY[7] == 178);
  CHECK(physicalTarget->printedText[8] == String("Feels: 15.8 C"));
  CHECK(physicalTarget->printedY[8] == 198);

  // An absent render target is a safe no-op through TextRenderer.
  const int printsBeforeNull = physicalTarget->printCount;
  displayManager.beginTabTransitionRender();
  CHECK(displayManager.target() == nullptr);
  renderer.draw(
    "No target", "No data", "0.0 C", "Feels: 0.0 C", colours);
  CHECK(physicalTarget->printCount == printsBeforeNull);
  displayManager.endTabTransitionRender();

  // The same renderer follows DisplayManager to its transition canvas.
  CHECK(displayManager.createTabTransitionCanvas(
    displayManager.width(), displayManager.height()));
  displayManager.beginTabTransitionRender();
  Arduino_GFX* transitionTarget = displayManager.target();
  CHECK(transitionTarget != nullptr);
  CHECK(transitionTarget != physicalTarget);

  renderer.draw(
    "Transition", "Overcast", "20.0 C", "Feels: 19.0 C", colours);
  CHECK(transitionTarget->printCount == 4);
  CHECK(transitionTarget->printedText[0] == String("Transition"));
  CHECK(transitionTarget->printedText[1] == String("Overcast"));
  CHECK(transitionTarget->printedText[2] == String("20.0 C"));
  CHECK(transitionTarget->printedText[3] == String("Feels: 19.0 C"));
  CHECK(physicalTarget->printCount == printsBeforeNull);

  displayManager.endTabTransitionRender();
  CHECK(displayManager.target() == physicalTarget);

  printf("WeatherConditionsRenderer tests passed.\n");
  return EXIT_SUCCESS;
}
