#pragma once

// Stateless, policy-neutral scroll calculations shared by Weather views.
// Offsets, drag lifecycles, input ownership, rendering and animation remain
// with the view that uses these calculations.
namespace ScrollController {

// Keep the small helper visible before the functions that use it.
constexpr int minimum(int a, int b) {
  return a < b ? a : b;
}

// Bound positions used by renderers and animation frames. This permits the
// configured elastic margin while preventing unbounded off-screen movement.
constexpr int clampElasticPosition(
    int position,
    int maxPosition,
    int overscrollLimit) {
  return position < -overscrollLimit
    ? -overscrollLimit
    : position > maxPosition + overscrollLimit
      ? maxPosition + overscrollLimit
      : position;
}

// Preserve the existing finger-to-content movement convention.
constexpr int applyDragDelta(
    int startPosition,
    int startCoordinate,
    int currentCoordinate) {
  return startPosition + startCoordinate - currentCoordinate;
}

// Apply resistance only beyond either edge. The integer division and cap
// intentionally match the Weather sketch's previous implementation.
constexpr int elasticPosition(
    int position,
    int maxPosition,
    int overscrollLimit,
    int resistanceDivisor) {
  return resistanceDivisor <= 0
    ? clampElasticPosition(position, maxPosition, overscrollLimit)
    : position < 0
      ? -minimum(overscrollLimit, (-position) / resistanceDivisor)
      : position > maxPosition
        ? maxPosition + minimum(
            overscrollLimit, (position - maxPosition) / resistanceDivisor)
        : position;
}

constexpr int clampPosition(int position, int maxPosition) {
  return position < 0
    ? 0
    : position > maxPosition
      ? maxPosition
      : position;
}

// Round to the nearest step and clamp to the first/last valid position.
// Keep C++ integer-division semantics identical to the previous sketch code.
constexpr int snapToStep(
    int position,
    int stepPixels,
    int maxPosition) {
  return stepPixels <= 0
    ? 0
    : clampPosition(
        ((position + stepPixels / 2) / stepPixels) * stepPixels,
        maxPosition);
}

}  // namespace ScrollController

// Compile-time checks for the shared arithmetic. These deliberately cover
// boundary resistance and snap rounding without creating runtime test state.
static_assert(
  ScrollController::applyDragDelta(120, 80, 95) == 105,
  "Scroll drag delta must preserve finger movement");
static_assert(
  ScrollController::applyDragDelta(80, 40, 40) == 80,
  "An unchanged finger coordinate must produce zero scroll movement");
static_assert(
  ScrollController::elasticPosition(-60, 100, 20, 3) == -20,
  "Negative overscroll must retain the existing resistance and cap");
static_assert(
  ScrollController::elasticPosition(160, 100, 20, 3) == 120,
  "Positive overscroll must retain the existing resistance and cap");
static_assert(
  ScrollController::elasticPosition(50, 100, 20, 3) == 50,
  "Positions inside the scroll range must remain one-to-one");
static_assert(
  ScrollController::clampElasticPosition(-25, 100, 20) == -20,
  "Elastic lower bound must be enforced");
static_assert(
  ScrollController::clampElasticPosition(125, 100, 20) == 120,
  "Elastic upper bound must be enforced");
static_assert(
  ScrollController::snapToStep(-20, 59, 177) == 0,
  "Negative elastic positions must snap to the first item");
static_assert(
  ScrollController::snapToStep(26, 59, 177) == 0,
  "Positions below the nearest-step midpoint must round down");
static_assert(
  ScrollController::snapToStep(33, 59, 177) == 59,
  "Positions above the nearest-step midpoint must round up");
static_assert(
  ScrollController::snapToStep(180, 59, 177) == 177,
  "Snapped positions must remain within the last valid item");
static_assert(
  ScrollController::snapToStep(84, 56, 280) == 112,
  "Graph-style point spacing must use the same nearest-step rule");
