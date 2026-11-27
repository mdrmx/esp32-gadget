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

  static constexpr int16_t TrackX = 179;
  static constexpr int16_t TrackY = 41;
  static constexpr int16_t TrackWidth = 137;
  static constexpr int16_t TrackHeight = 26;
  static constexpr int16_t TrackRadius = 6;
  static constexpr int16_t ListSegmentX = 181;
  static constexpr int16_t GraphSegmentX = 248;
  static constexpr int16_t SegmentY = 41;
  static constexpr int16_t SegmentWidth = 67;
  static constexpr int16_t SegmentHeight = 24;
  static constexpr int16_t SegmentRadius = 5;
  static constexpr int16_t ListLabelX = 201;
  static constexpr int16_t GraphLabelX = 265;
  static constexpr int16_t LabelY = 50;
  static constexpr uint8_t LabelTextSize = 1;

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
    target->drawRoundRect(
      TrackX, TrackY, TrackWidth, TrackHeight, TrackRadius,
      colours_.unselectedText);

    const bool graphSelected = mode == Mode::Graph;
    const int selectedX = graphSelected ? GraphSegmentX : ListSegmentX;
    target->fillRoundRect(
      selectedX,
      SegmentY,
      SegmentWidth,
      SegmentHeight,
      SegmentRadius,
      colours_.selectedSegment);
    target->drawRoundRect(
      selectedX,
      SegmentY,
      SegmentWidth,
      SegmentHeight,
      SegmentRadius,
      colours_.unselectedText);

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
