#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <algorithm>

#include "WeatherMetricCardRenderer.h"

// Composes the visible rows in the Weather metrics panel. Weather supplies
// formatted metric items and the chosen render target; it retains canvas
// lifecycle, scroll state, animation and screen composition.
class WeatherMetricsViewportRenderer {
public:
  struct Item {
    String label;
    String value;
  };

  struct Layout {
    int itemCount;
    int visibleItems;
    int rowTop;
    int rowStep;
    int x;
    int panelWidth;
    int viewportTop;
    int viewportHeight;
    uint16_t background;
  };

  typedef bool (*ItemReader)(
    const void* context,
    int index,
    Item& item
  );

  explicit WeatherMetricsViewportRenderer(
    const WeatherMetricCardRenderer& cardRenderer)
    : cardRenderer_(cardRenderer) {}

  void draw(
    Arduino_GFX* target,
    const Layout& layout,
    int baseOffset,
    int scrollPixels,
    const void* context,
    ItemReader readItem
  ) const {
    if (target == nullptr || readItem == nullptr ||
        layout.itemCount <= 0 || layout.visibleItems <= 0 ||
        layout.rowStep <= 0 || layout.panelWidth <= 0 ||
        layout.viewportHeight <= 0) {
      return;
    }

    const int maxOffset = (std::max)(
      0, layout.itemCount - layout.visibleItems);
    baseOffset = (std::max)(0, (std::min)(baseOffset, maxOffset));

    target->fillRect(
      layout.x,
      layout.viewportTop,
      layout.panelWidth,
      layout.viewportHeight,
      layout.background);

    const int firstIndex = (std::max)(0, baseOffset - 1);
    const int lastIndex = (std::min)(
      layout.itemCount,
      baseOffset + layout.visibleItems + 1);

    // Reuse the String buffers across visible rows to avoid repeated
    // allocation/free cycles (and heap fragmentation) on the ESP32.
    Item item;
    for (int index = firstIndex; index < lastIndex; ++index) {
      const int localY = layout.rowTop +
        (index - baseOffset) * layout.rowStep - scrollPixels;

      if (localY + WeatherMetricCardRenderer::CARD_HEIGHT <= 0 ||
          localY >= layout.viewportHeight) {
        continue;
      }

      if (!readItem(context, index, item)) {
        continue;
      }

      cardRenderer_.draw(
        target,
        item.label,
        item.value,
        layout.x,
        layout.viewportTop + localY,
        layout.panelWidth,
        layout.viewportTop,
        layout.viewportHeight);
    }
  }

private:
  const WeatherMetricCardRenderer& cardRenderer_;
};
