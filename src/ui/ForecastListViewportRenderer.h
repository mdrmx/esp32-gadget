#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <algorithm>

#include "ForecastListItemRenderer.h"

// Composes the visible rows in the Forecast List viewport. Weather supplies
// forecast items and retains scroll policy, canvas lifecycle, graph selection,
// controls, transitions and full-screen composition.
class ForecastListViewportRenderer {
public:
  struct Layout {
    int itemCount;
    int visibleItems;
    int rowTop;
    int rowStep;
    int viewportTop;
    int viewportHeight;
  };

  typedef bool (*ItemReader)(
    const void* context,
    int index,
    ForecastListItemRenderer::Item& item
  );

  explicit ForecastListViewportRenderer(
    const ForecastListItemRenderer& itemRenderer)
    : itemRenderer_(itemRenderer) {}

  // Draw the buffered viewport, including neighbouring partial rows. rowTop
  // and viewportTop are screen-space coordinates; drawClipped() translates
  // them into the viewport canvas' local coordinate space.
  void drawClippedViewport(
    Arduino_GFX* target,
    const Layout& layout,
    int baseOffset,
    int scrollPixels,
    const void* context,
    ItemReader readItem
  ) const {
    if (!valid(target, layout, readItem)) return;

    const int maxOffset = (std::max)(
      0, layout.itemCount - layout.visibleItems);
    baseOffset = (std::max)(
      0, (std::min)(baseOffset, maxOffset));

    const int firstIndex = (std::max)(0, baseOffset - 1);
    const int lastIndex = (std::min)(
      layout.itemCount,
      baseOffset + layout.visibleItems + 1);

    // Keep one Item alive for the full row pass so Arduino String buffers
    // can be reused rather than constructed and destroyed for each card.
    ForecastListItemRenderer::Item item;
    for (int index = firstIndex; index < lastIndex; ++index) {
      const int y = layout.rowTop +
        (index - baseOffset) * layout.rowStep - scrollPixels;
      const int cardTop = y - layout.viewportTop;

      // Avoid item construction and draw calls for rows wholly outside the
      // viewport. Partially visible rows are delegated to the card clipper.
      if (cardTop + ForecastListItemRenderer::CARD_HEIGHT <= 0 ||
          cardTop >= layout.viewportHeight) {
        continue;
      }

      if (!readItem(context, index, item)) continue;
      itemRenderer_.drawClipped(
        target,
        item,
        y,
        layout.viewportTop,
        layout.viewportHeight);
    }
  }

  // Direct-display / transition fallback deliberately renders settled rows.
  // It preserves the existing no-canvas behaviour and does not attempt the
  // smooth partial-row movement used by the buffered path.
  void drawSettledFallback(
    Arduino_GFX* target,
    const Layout& layout,
    int baseOffset,
    const void* context,
    ItemReader readItem
  ) const {
    if (!valid(target, layout, readItem)) return;

    const int maxOffset = (std::max)(
      0, layout.itemCount - layout.visibleItems);
    baseOffset = (std::max)(
      0, (std::min)(baseOffset, maxOffset));
    const int visible = (std::min)(
      layout.visibleItems,
      (std::max)(0, layout.itemCount - baseOffset));

    ForecastListItemRenderer::Item item;
    for (int row = 0; row < visible; ++row) {
      const int index = baseOffset + row;
      if (!readItem(context, index, item)) continue;

      itemRenderer_.draw(
        target,
        item,
        layout.rowTop + row * layout.rowStep);
    }
  }

private:
  static bool valid(
    Arduino_GFX* target,
    const Layout& layout,
    ItemReader readItem
  ) {
    return target != nullptr &&
      readItem != nullptr &&
      layout.itemCount > 0 &&
      layout.visibleItems > 0 &&
      layout.rowStep > 0 &&
      layout.viewportHeight > 0;
  }

  const ForecastListItemRenderer& itemRenderer_;
};
