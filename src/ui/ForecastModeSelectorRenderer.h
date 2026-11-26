#pragma once

#include <Arduino.h>

#include "../hardware/DisplayManager.h"
#include "TextRenderer.h"

// Owns the Forecast List / Graph selector's visual geometry and selected state.
// Weather retains the mode state, touch hit-testing and gesture policy.
class ForecastModeSelectorRenderer {
public:
  enum class Mode : uint8_t {
    List = 0,
    Graph
  };

  struct Colours {
    uint16_t track;
    uint16_t selectedSegment;
    uint16_t selectedText;
    uint16_t unselectedText;
  };

  enum {
    TrackX = 179,
    TrackY = 41,
    TrackWidth = 137,
    TrackHeight = 26,
    TrackRadius = 6,
    ListSegmentX = 181,
    GraphSegmentX = 248,
    SegmentY = 41,
    SegmentWidth = 67,
    SegmentHeight = 24,
    SegmentRadius = 5,
    ListLabelX = 201,
    GraphLabelX = 265,
    LabelY = 50,
    LabelTextSize = 1
  };

  ForecastModeSelectorRenderer(
    DisplayManager& displayManager,
    TextRenderer& textRenderer,
    const Colours& colours
  )
    : displayManager_(displayManager),
      textRenderer_(textRenderer),
      colours_(colours) {}

  void draw(Mode mode) const {
    Arduino_GFX* target = displayManager_.target();
    if (target == nullptr) return;

    target->fillRoundRect(
      TrackX, TrackY, TrackWidth, TrackHeight, TrackRadius, colours_.track);

    const bool graphSelected = mode == Mode::Graph;
    target->fillRoundRect(
      graphSelected ? GraphSegmentX : ListSegmentX,
      SegmentY,
      SegmentWidth,
      SegmentHeight,
      SegmentRadius,
      colours_.selectedSegment);

    textRenderer_.drawText(
      "LIST",
      ListLabelX,
      LabelY,
      LabelTextSize,
      graphSelected ? colours_.unselectedText : colours_.selectedText);
    textRenderer_.drawText(
      "GRAPH",
      GraphLabelX,
      LabelY,
      LabelTextSize,
      graphSelected ? colours_.selectedText : colours_.unselectedText);
  }

private:
  DisplayManager& displayManager_;
  TextRenderer& textRenderer_;
  Colours colours_;
};
