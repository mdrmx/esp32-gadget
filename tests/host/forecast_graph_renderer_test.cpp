#include <stdio.h>
#include <stdlib.h>

#include "../../src/ui/ForecastGraphRenderer.h"

static void check(bool condition, const char* expression, int line) {
  if (!condition) {
    fprintf(stderr, "FAIL: %s (line %d)\n", expression, line);
    exit(EXIT_FAILURE);
  }
}

#define CHECK(expression) check((expression), #expression, __LINE__)

struct TestPoint {
  String timeText;
  String description;
  float temperature;
  float rainProbability;
};

struct TestData {
  TestPoint points[9];
  int count;
};

static bool readTestPoint(
  const void* context,
  int index,
  ForecastGraphRenderer::Item& item
) {
  if (context == nullptr) return false;
  const TestData* data = static_cast<const TestData*>(context);
  if (index < 0 || index >= data->count) return false;

  const TestPoint& point = data->points[index];
  item.timeText = &point.timeText;
  item.description = &point.description;
  item.temperature = point.temperature;
  item.rainProbability = point.rainProbability;
  return true;
}

static TestData makeTestData() {
  TestData data;
  data.count = 9;
  const char* times[] = {
    "Mon 10:00", "11:00", "12:00", "13:00",
    "14:00", "15:00", "16:00", "17:00", "18:00"
  };
  const float temperatures[] = {
    10.0f, 12.0f, 14.0f, 16.0f, 18.0f, 20.0f, 22.0f, 21.0f, 19.0f
  };
  const float rain[] = {
    0.0f, 0.25f, 0.50f, 0.75f, 1.0f, -0.2f, 1.2f, 0.5f, 0.4f
  };

  for (int i = 0; i < data.count; i++) {
    data.points[i].timeText = String(times[i]);
    data.points[i].description = String("Clouds");
    data.points[i].temperature = temperatures[i];
    data.points[i].rainProbability = rain[i];
  }
  return data;
}

int main() {
  const uint16_t PAPER = 0xFF9C;
  const uint16_t BURNT_ORANGE = 0xD303;
  const uint16_t INK = 0x28C1;

  const ForecastListItemRenderer::Colours listColours = {
    PAPER, INK, INK, INK, INK, INK
  };
  const ForecastGraphRenderer::Colours graphColours = {
    INK, BURNT_ORANGE, BURNT_ORANGE, INK, INK, BURNT_ORANGE
  };

  DisplayManager displayManager;
  CHECK(displayManager.begin());
  ForecastListItemRenderer listRenderer(displayManager, listColours);
  ForecastGraphRenderer graphRenderer(listRenderer, graphColours);
  Arduino_GFX* target = displayManager.target();
  CHECK(target != nullptr);

  TestData data = makeTestData();

  // Empty input, null target and an absent reader must be harmless.
  graphRenderer.draw(
    target, 320, 64, 0, 0, &data, readTestPoint);
  graphRenderer.draw(
    nullptr, 320, 64, data.count, 0, &data, readTestPoint);
  graphRenderer.draw(
    target, 320, 64, data.count, 0, &data, nullptr);
  CHECK(target->horizontalLineCount == 0);
  CHECK(target->drawLineCount == 0);
  CHECK(target->printCount == 0);
  CHECK(target->fillCircleCount == 0);

  // Seven hourly points fit across the plot at 46 px spacing. The eighth
  // record remains available as a neighbouring sample for scale continuity.
  graphRenderer.draw(
    target, 320, 64, data.count, 0, &data, readTestPoint);
  CHECK(target->horizontalLineCount == 2);
  CHECK(target->fillCircleCount == 28); // 7 graph points + 7 cloud icons x 3
  CHECK(target->drawBitmapCount == 0); // graph reuses the vector condition icons
  CHECK(target->fillRectCount == 14); // 7 cloud bodies + 7 rain bars
  CHECK(target->printCount == 21);
  CHECK(target->printedText[0] == String("10C"));
  CHECK(target->printedText[1] == String("10:00"));
  CHECK(target->printedText[2] == String("0%"));
  CHECK(target->printedText[3] == String("12C"));
  CHECK(target->printedText[4] == String("11:00"));
  CHECK(target->printedText[5] == String("25%"));
  CHECK(target->printedColour[0] == INK);
  CHECK(target->printedColour[1] == INK);
  CHECK(target->printedColour[2] == INK);
  CHECK(target->printedY[2] == 64 + 143);
  CHECK(target->lastFillRectColour == BURNT_ORANGE);
  CHECK(target->drawLineCount == 14); // 7 segments including edge continuity, 2 px each
  CHECK(target->lastLineColour == BURNT_ORANGE);
  CHECK(target->lastFillCircleX == 304);
  CHECK(target->lastFillCircleY == 92); // enlarged icon moved 4 px down
  CHECK(target->lastFillCircleRadius == 4); // cloud icon scaled by 120%

  // At one full point spacing, index zero moves off-screen and the next seven
  // forecast points become visible. Scroll is supplied by Weather, unchanged.
  const int printsBeforeScroll = target->printCount;
  const int circlesBeforeScroll = target->fillCircleCount;
  const int gridLinesBeforeScroll = target->horizontalLineCount;
  graphRenderer.draw(
    target, 320, 64, data.count, 46, &data, readTestPoint);
  CHECK(target->horizontalLineCount == gridLinesBeforeScroll + 2);
  CHECK(target->printCount == printsBeforeScroll + 21);
  CHECK(target->fillCircleCount == circlesBeforeScroll + 28);

  // Negative elastic scroll remains a caller concern, but the renderer keeps
  // the existing chart offset and uses zero for the visible-range calculation.
  const int printsBeforeElastic = target->printCount;
  graphRenderer.draw(
    target, 320, 64, data.count, -20, &data, readTestPoint);
  CHECK(target->printCount == printsBeforeElastic + 21);

  // Out-of-range readers produce no partial graph.
  const int linesBeforeInvalid = target->horizontalLineCount;
  const int printsBeforeInvalid = target->printCount;
  TestData empty = {};
  graphRenderer.draw(
    target, 320, 64, 1, 0, &empty, readTestPoint);
  CHECK(target->horizontalLineCount == linesBeforeInvalid);
  CHECK(target->printCount == printsBeforeInvalid);

  printf("ForecastGraphRenderer tests passed.\n");
  return EXIT_SUCCESS;
}
