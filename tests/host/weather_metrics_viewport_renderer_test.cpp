#include <stdio.h>
#include <stdlib.h>

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

// Simulate Arduino variants that expose function-like min/max macros. The
// renderer must remain safe when included after those macros are defined.
#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))
#include "../../src/ui/WeatherMetricCardRenderer.h"
#include "../../src/ui/WeatherMetricsViewportRenderer.h"
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

struct Source {
  int itemCount;
  int failIndex;
  int readCount;
  int indices[16];
};

static bool readItem(
  const void* context,
  int index,
  WeatherMetricsViewportRenderer::Item& item
) {
  if (context == nullptr) return false;
  Source* source = static_cast<Source*>(const_cast<void*>(context));
  if (index < 0 || index >= source->itemCount) return false;

  if (source->readCount < 16) {
    source->indices[source->readCount] = index;
  }
  ++source->readCount;

  if (index == source->failIndex) return false;

  item.label = String("METRIC ") + String(index);
  item.value = String("VALUE ") + String(index);
  return true;
}

int main() {
  const uint16_t PANEL = 0x2128;
  const uint16_t MUTED = 0xBDF7;
  const uint16_t ACCENT = 0x5DDF;
  const uint16_t BG = 0x1082;

  const WeatherMetricCardRenderer::Colours cardColours = {
    PANEL, MUTED, ACCENT
  };
  WeatherMetricCardRenderer cardRenderer(cardColours);
  WeatherMetricsViewportRenderer renderer(cardRenderer);

  Source source = {8, -1, 0, {}};

  // The viewport selects only rows that intersect the visible area. At
  // offset 1, row 0 is fully above and row 4 is fully below the viewport.
  const WeatherMetricsViewportRenderer::Layout canvasLayout = {
    8, 3, 2, 59, 0, 118, 0, 177, BG
  };
  TestGfx canvasTarget;
  renderer.draw(
    &canvasTarget, canvasLayout, 1, 0, &source, readItem);

  CHECK(canvasTarget.fillRectCount == 1);
  CHECK(canvasTarget.lastFillRectX == 0);
  CHECK(canvasTarget.lastFillRectY == 0);
  CHECK(canvasTarget.lastFillRectWidth == 118);
  CHECK(canvasTarget.lastFillRectHeight == 177);
  CHECK(canvasTarget.lastFillRectColour == BG);
  CHECK(canvasTarget.fillRoundRectCount == 3);
  CHECK(canvasTarget.printCount == 6);
  CHECK(source.readCount == 3);
  CHECK(source.indices[0] == 1);
  CHECK(source.indices[1] == 2);
  CHECK(source.indices[2] == 3);
  CHECK(canvasTarget.lastFillRoundRectX == 2);
  CHECK(canvasTarget.lastFillRoundRectY == 120);
  CHECK(canvasTarget.lastFillRoundRectHeight == 54);

  // The same renderer supports physical-display coordinates. The panel origin
  // is added only by the supplied viewport origin, not by the renderer.
  Source directSource = {8, -1, 0, {}};
  const WeatherMetricsViewportRenderer::Layout directLayout = {
    8, 3, 2, 59, 192, 118, 47, 177, BG
  };
  TestGfx directTarget;
  renderer.draw(
    &directTarget, directLayout, 0, 0, &directSource, readItem);

  CHECK(directTarget.fillRectCount == 1);
  CHECK(directTarget.lastFillRectX == 192);
  CHECK(directTarget.lastFillRectY == 47);
  CHECK(directTarget.lastFillRectWidth == 118);
  CHECK(directTarget.lastFillRectHeight == 177);
  CHECK(directTarget.fillRoundRectCount == 3);
  CHECK(directTarget.lastFillRoundRectX == 194);
  CHECK(directTarget.lastFillRoundRectY == 167);
  CHECK(directTarget.printCount == 6);
  CHECK(directTarget.printedY[0] == 56);

  // A partial top row is clipped by the card renderer: panel slice remains
  // visible, while text outside the viewport is omitted.
  Source clippedSource = {8, -1, 0, {}};
  TestGfx clippedTarget;
  renderer.draw(
    &clippedTarget, directLayout, 0, 10, &clippedSource, readItem);

  CHECK(clippedTarget.fillRectCount == 2); // viewport + clipped card slice
  CHECK(clippedTarget.lastFillRectX == 194);
  CHECK(clippedTarget.lastFillRectY == 47);
  CHECK(clippedTarget.lastFillRectWidth == 114);
  CHECK(clippedTarget.lastFillRectHeight == 46);
  CHECK(clippedTarget.printCount == 5); // clipped first row has value only

  // A high requested offset is bounded to the final legal page.
  Source boundedSource = {5, -1, 0, {}};
  const WeatherMetricsViewportRenderer::Layout boundedLayout = {
    5, 3, 2, 59, 0, 118, 0, 177, BG
  };
  TestGfx boundedTarget;
  renderer.draw(
    &boundedTarget, boundedLayout, 99, 0, &boundedSource, readItem);
  CHECK(boundedSource.readCount == 3);
  CHECK(boundedSource.indices[0] == 2);
  CHECK(boundedSource.indices[1] == 3);
  CHECK(boundedSource.indices[2] == 4);
  CHECK(boundedTarget.fillRoundRectCount == 3);

  // Failed data reads leave the background intact and skip only that card.
  Source missingSource = {1, 0, 0, {}};
  const WeatherMetricsViewportRenderer::Layout oneItemLayout = {
    1, 1, 2, 59, 0, 118, 0, 177, BG
  };
  TestGfx missingTarget;
  renderer.draw(
    &missingTarget, oneItemLayout, 0, 0, &missingSource, readItem);
  CHECK(missingTarget.fillRectCount == 1);
  CHECK(missingTarget.fillRoundRectCount == 0);
  CHECK(missingTarget.printCount == 0);

  // Null targets/readers and invalid layouts are safe no-ops.
  TestGfx invalidTarget;
  renderer.draw(
    nullptr, canvasLayout, 0, 0, &source, readItem);
  renderer.draw(
    &invalidTarget, canvasLayout, 0, 0, &source, nullptr);
  WeatherMetricsViewportRenderer::Layout invalidLayout = canvasLayout;
  invalidLayout.rowStep = 0;
  renderer.draw(
    &invalidTarget, invalidLayout, 0, 0, &source, readItem);
  invalidLayout = canvasLayout;
  invalidLayout.viewportHeight = 0;
  renderer.draw(
    &invalidTarget, invalidLayout, 0, 0, &source, readItem);
  invalidLayout = canvasLayout;
  invalidLayout.itemCount = 0;
  renderer.draw(
    &invalidTarget, invalidLayout, 0, 0, &source, readItem);
  CHECK(invalidTarget.fillRectCount == 0);
  CHECK(invalidTarget.fillRoundRectCount == 0);
  CHECK(invalidTarget.printCount == 0);

  printf("WeatherMetricsViewportRenderer tests passed.\n");
  return EXIT_SUCCESS;
}
