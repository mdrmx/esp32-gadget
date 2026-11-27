#include <stdio.h>
#include <stdlib.h>

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "../../src/hardware/DisplayManager.h"
#include "../../src/ui/ForecastListItemRenderer.h"
#include "../../src/ui/ForecastListViewportRenderer.h"

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
  mutable int readCount;
  mutable int indices[16];
};

static bool readItem(
  const void* context,
  int index,
  ForecastListItemRenderer::Item& item
) {
  if (context == nullptr) return false;
  const Source* source = static_cast<const Source*>(context);
  if (index < 0 || index >= source->itemCount) return false;

  if (source->readCount < 16) {
    source->indices[source->readCount] = index;
  }
  ++source->readCount;
  if (index == source->failIndex) return false;

  item.time = String("TIME ") + String(index);
  item.description = "Clear";
  item.temperature = 18.6f;
  item.feelsLike = 17.4f;
  item.rainProbability = 0.256f;
  return true;
}

int main() {
  const uint16_t PAPER = 0xFF9C;
  const uint16_t INK = 0x28C1;
  const ForecastListItemRenderer::Colours colours = {
    PAPER, INK, INK, INK, INK, INK
  };

  DisplayManager displayManager;
  CHECK(displayManager.begin());
  ForecastListItemRenderer cardRenderer(displayManager, colours);
  ForecastListViewportRenderer renderer(cardRenderer);

  const ForecastListViewportRenderer::Layout layout = {
    8, 3, 72, 48, 72, 144
  };

  // The first row begins at the viewport's upper edge. The preceding row is
  // fully clipped, while the three visible rows are drawn at local y=0, 48, 96.
  Source bufferedSource = {8, -1, 0, {}};
  TestGfx bufferedTarget;
  renderer.drawClippedViewport(
    &bufferedTarget, layout, 1, 0, &bufferedSource, readItem);

  CHECK(bufferedSource.readCount == 3);
  CHECK(bufferedSource.indices[0] == 1);
  CHECK(bufferedSource.indices[1] == 2);
  CHECK(bufferedSource.indices[2] == 3);
  CHECK(bufferedTarget.fillRectCount == 0);
  CHECK(bufferedTarget.fillRoundRectCount == 3);
  CHECK(bufferedTarget.drawRoundRectCount == 3);
  CHECK(bufferedTarget.drawRoundRectColour[0] == INK);
  CHECK(bufferedTarget.printCount == 12);
  CHECK(bufferedTarget.printedText[0] == String("TIME 1"));
  CHECK(bufferedTarget.printedY[0] == 18);
  CHECK(bufferedTarget.lastFillRoundRectY == 96);

  // The direct-display and transition fallback renders only settled rows at
  // screen coordinates, preserving the old fallback's non-animated layout.
  Source fallbackSource = {8, -1, 0, {}};
  TestGfx fallbackTarget;
  renderer.drawSettledFallback(
    &fallbackTarget, layout, 1, &fallbackSource, readItem);

  CHECK(fallbackSource.readCount == 3);
  CHECK(fallbackSource.indices[0] == 1);
  CHECK(fallbackSource.indices[1] == 2);
  CHECK(fallbackSource.indices[2] == 3);
  CHECK(fallbackTarget.fillRoundRectCount == 3);
  CHECK(fallbackTarget.drawRoundRectCount == 3);
  CHECK(fallbackTarget.drawRoundRectColour[0] == INK);
  CHECK(fallbackTarget.printCount == 12);
  CHECK(fallbackTarget.lastFillRoundRectY == 168);
  CHECK(fallbackTarget.printedText[0] == String("TIME 1"));
  CHECK(fallbackTarget.printedY[0] == 90);

  // Negative scroll near the first page exposes a partial bottom card.
  // The viewport renderer keeps that slice and omits rows below the viewport.
  Source bottomClipSource = {8, -1, 0, {}};
  TestGfx bottomClipTarget;
  renderer.drawClippedViewport(
    &bottomClipTarget, layout, 0, -20, &bottomClipSource, readItem);
  CHECK(bottomClipSource.readCount == 3);
  CHECK(bottomClipSource.indices[0] == 0);
  CHECK(bottomClipSource.indices[1] == 1);
  CHECK(bottomClipSource.indices[2] == 2);
  CHECK(bottomClipTarget.fillRectCount == 1);
  CHECK(bottomClipTarget.lastFillRectX == 4);
  CHECK(bottomClipTarget.lastFillRectY == 116);
  CHECK(bottomClipTarget.lastFillRectWidth == 312);
  CHECK(bottomClipTarget.lastFillRectHeight == 28);
  CHECK(bottomClipTarget.printCount == 11);
  CHECK(bottomClipTarget.printedText[8] == String("TIME 2"));
  CHECK(bottomClipTarget.printedY[8] == 134);

  // A high requested offset is bounded to the last valid page.
  Source boundedSource = {8, -1, 0, {}};
  TestGfx boundedTarget;
  renderer.drawSettledFallback(
    &boundedTarget, layout, 99, &boundedSource, readItem);
  CHECK(boundedSource.readCount == 3);
  CHECK(boundedSource.indices[0] == 5);
  CHECK(boundedSource.indices[1] == 6);
  CHECK(boundedSource.indices[2] == 7);

  // Failed reads skip only the affected card.
  ForecastListViewportRenderer::Layout oneItemLayout = {
    1, 1, 72, 48, 72, 144
  };
  Source failedSource = {1, 0, 0, {}};
  TestGfx failedTarget;
  renderer.drawClippedViewport(
    &failedTarget, oneItemLayout, 0, 0, &failedSource, readItem);
  CHECK(failedSource.readCount == 1);
  CHECK(failedTarget.fillRoundRectCount == 0);
  CHECK(failedTarget.printCount == 0);

  // During upward scrolling a card may be partially visible at the viewport
  // edge, but no part of it is painted above y=72. Once its final visible
  // portion passes that edge, the renderer skips it entirely.
  Source topClipSource = {1, -1, 0, {}};
  TestGfx topClipTarget;
  renderer.drawClippedViewport(
    &topClipTarget, oneItemLayout, 0, 20, &topClipSource, readItem);
  CHECK(topClipSource.readCount == 1);
  CHECK(topClipTarget.fillRectCount == 1);
  CHECK(topClipTarget.lastFillRectY == 0);
  CHECK(topClipTarget.lastFillRectHeight == 24);
  CHECK(topClipTarget.fillRoundRectCount == 0);
  CHECK(topClipTarget.printCount == 0);

  Source hiddenSource = {1, -1, 0, {}};
  TestGfx hiddenTarget;
  renderer.drawClippedViewport(
    &hiddenTarget, oneItemLayout, 0, 45, &hiddenSource, readItem);
  CHECK(hiddenSource.readCount == 0);
  CHECK(hiddenTarget.fillRectCount == 0);
  CHECK(hiddenTarget.fillRoundRectCount == 0);
  CHECK(hiddenTarget.printCount == 0);

  // Null targets/readers and invalid layouts are no-ops.
  TestGfx invalidTarget;
  renderer.drawClippedViewport(
    nullptr, layout, 0, 0, &bufferedSource, readItem);
  renderer.drawClippedViewport(
    &invalidTarget, layout, 0, 0, &bufferedSource, nullptr);
  renderer.drawSettledFallback(
    &invalidTarget, layout, 0, &bufferedSource, nullptr);
  ForecastListViewportRenderer::Layout invalidLayout = layout;
  invalidLayout.rowStep = 0;
  renderer.drawClippedViewport(
    &invalidTarget, invalidLayout, 0, 0, &bufferedSource, readItem);
  invalidLayout = layout;
  invalidLayout.viewportHeight = 0;
  renderer.drawSettledFallback(
    &invalidTarget, invalidLayout, 0, &bufferedSource, readItem);
  invalidLayout = layout;
  invalidLayout.itemCount = 0;
  renderer.drawClippedViewport(
    &invalidTarget, invalidLayout, 0, 0, &bufferedSource, readItem);
  CHECK(invalidTarget.fillRoundRectCount == 0);
  CHECK(invalidTarget.fillRectCount == 0);
  CHECK(invalidTarget.printCount == 0);

  printf("ForecastListViewportRenderer tests passed.\n");
  return EXIT_SUCCESS;
}
