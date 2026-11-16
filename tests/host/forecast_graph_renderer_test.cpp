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
  TestPoint points[8];
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
  data.count = 7;
  const char* times[] = {
    "Mon 10:00", "11:00", "12:00", "13:00",
    "14:00", "15:00", "16:00"
  };
  const float temperatures[] = {
    10.0f, 12.0f, 14.0f, 16.0f, 18.0f, 20.0f, 22.0f
  };
  const float rain[] = {
    0.0f, 0.25f, 0.50f, 0.75f, 1.0f, -0.2f, 1.2f
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
  const uint16_t PANEL = 0x2128;
  const uint16_t WHITE = 0xFFFF;
  const uint16_t YELLOW = 0xFEA0;
  const uint16_t MUTED = 0xBDF7;
  const uint16_t ACCENT = 0x5DDF;

  const ForecastListItemRenderer::Colours listColours = {
    PANEL, WHITE, YELLOW, MUTED, ACCENT
  };
  const ForecastGraphRenderer::Colours graphColours = {
    PANEL, YELLOW, WHITE, WHITE, MUTED, ACCENT
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
  CHECK(target->printCount == 0);
  CHECK(target->fillCircleCount == 0);

  // At the first page, six points are visible. The seventh is outside the
  // logical display. Each visible point gets temperature, time and rain text.
  graphRenderer.draw(
    target, 320, 64, data.count, 0, &data, readTestPoint);
  CHECK(target->horizontalLineCount == 3);
  CHECK(target->fillCircleCount == 24); // 6 graph points + 6 cloud icons x 3
  CHECK(target->fillRectCount == 12); // 6 cloud bodies + 6 rain bars
  CHECK(target->printCount == 18);
  CHECK(target->printedText[0] == String("10C"));
  CHECK(target->printedText[1] == String("10:00"));
  CHECK(target->printedText[2] == String("0%"));
  CHECK(target->printedText[3] == String("12C"));
  CHECK(target->printedText[4] == String("11:00"));
  CHECK(target->printedText[5] == String("25%"));
  CHECK(target->printedColour[0] == WHITE);
  CHECK(target->printedColour[1] == MUTED);
  CHECK(target->printedColour[2] == WHITE);
  CHECK(target->printedY[2] == 64 + 143);
  CHECK(target->lastFillRectColour == ACCENT);
  CHECK(target->drawLineCount >= 5);
  CHECK(target->lastLineColour == YELLOW);

  // At one full point spacing, index zero moves off-screen and the next six
  // forecast points become visible. Scroll is supplied by Weather, unchanged.
  const int printsBeforeScroll = target->printCount;
  const int circlesBeforeScroll = target->fillCircleCount;
  const int gridLinesBeforeScroll = target->horizontalLineCount;
  graphRenderer.draw(
    target, 320, 64, data.count, 56, &data, readTestPoint);
  CHECK(target->horizontalLineCount == gridLinesBeforeScroll + 3);
  CHECK(target->printCount == printsBeforeScroll + 18);
  CHECK(target->fillCircleCount == circlesBeforeScroll + 24);
  CHECK(target->printedText[printsBeforeScroll] == String("12C"));
  CHECK(target->printedText[printsBeforeScroll + 1] == String("11:00"));

  // Negative elastic scroll remains a caller concern, but the renderer keeps
  // the existing chart offset and uses zero for the visible-range calculation.
  const int printsBeforeElastic = target->printCount;
  graphRenderer.draw(
    target, 320, 64, data.count, -20, &data, readTestPoint);
  CHECK(target->printCount == printsBeforeElastic + 18);

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
