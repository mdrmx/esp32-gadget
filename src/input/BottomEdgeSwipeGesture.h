#ifndef ESP32_GADGET_BOTTOM_EDGE_SWIPE_GESTURE_H
#define ESP32_GADGET_BOTTOM_EDGE_SWIPE_GESTURE_H

#include <stdint.h>
#include <stdlib.h>

// Recognises the platform's quick bottom-edge swipe-up launcher gesture.
// This class only interprets one contact; the sketch decides whether the
// launcher is currently available and what to do when the gesture completes.
class BottomEdgeSwipeGesture {
 public:
  static const int BottomEdgeHeightPx = 24;
  static const int MinimumUpwardDistancePx = 52;
  static const int MaximumHorizontalDriftPx = 40;
  static const uint32_t MaximumDurationMs = 650UL;

  BottomEdgeSwipeGesture()
      : active_(false),
        candidate_(false),
        startX_(0),
        startY_(0),
        startedAtMs_(0) {}

  // Coordinates are already transformed into display orientation. Supply the
  // release sample's coordinates too: CST816 event=UP still carries its final
  // X/Y values even though GestureObservation::touchPresent is false.
  bool update(
      bool touchStarted,
      bool touchPresent,
      bool touchReleased,
      int x,
      int y,
      uint32_t nowMs,
      int screenHeight) {
    if (touchStarted) {
      reset();
      active_ = true;
      candidate_ = screenHeight > 0 &&
          y >= screenHeight - BottomEdgeHeightPx;
      startX_ = x;
      startY_ = y;
      startedAtMs_ = nowMs;
    }

    if (!active_) {
      return false;
    }

    if (touchPresent && candidate_) {
      if (absolute(x - startX_) > MaximumHorizontalDriftPx ||
          elapsed(nowMs, startedAtMs_) > MaximumDurationMs) {
        candidate_ = false;
      }
    }

    if (!touchReleased) {
      return false;
    }

    const bool recognised =
        candidate_ &&
        elapsed(nowMs, startedAtMs_) <= MaximumDurationMs &&
        y <= startY_ - MinimumUpwardDistancePx &&
        absolute(x - startX_) <= MaximumHorizontalDriftPx;
    reset();
    return recognised;
  }

  void reset() {
    active_ = false;
    candidate_ = false;
    startX_ = 0;
    startY_ = 0;
    startedAtMs_ = 0;
  }

 private:
  bool active_;
  bool candidate_;
  int startX_;
  int startY_;
  uint32_t startedAtMs_;

  static int absolute(int value) {
    return value < 0 ? -value : value;
  }

  static uint32_t elapsed(uint32_t nowMs, uint32_t startMs) {
    return static_cast<uint32_t>(nowMs - startMs);
  }
};

#endif  // ESP32_GADGET_BOTTOM_EDGE_SWIPE_GESTURE_H
