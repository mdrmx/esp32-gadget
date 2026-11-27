#ifndef ESP32_GADGET_BOTTOM_EDGE_SWIPE_GESTURE_H
#define ESP32_GADGET_BOTTOM_EDGE_SWIPE_GESTURE_H

#include <stdint.h>
#include <stdlib.h>

// Recognises the platform's quick bottom-edge swipe-up launcher gesture.
// The gesture can only begin in a narrow central band. Matching contacts are
// claimed so an incomplete home swipe is not interpreted as app scrolling.
class BottomEdgeSwipeGesture {
 public:
  static const int BottomEdgeHeightPx = 24;
  static const int ActivationBandWidthPx = 64;
  static const int MinimumUpwardDistancePx = 52;
  static const int MaximumHorizontalDriftPx = 40;
  static const uint32_t MaximumDurationMs = 650UL;

  BottomEdgeSwipeGesture()
      : active_(false),
        candidate_(false),
        startedInActivationBand_(false),
        completedActivationBand_(false),
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
      int screenWidth,
      int screenHeight) {
    if (touchStarted) {
      reset();
      active_ = true;
      startedInActivationBand_ = screenWidth > 0 &&
          isInsideActivationBand(x, screenWidth) &&
          screenHeight > 0 &&
          y >= screenHeight - BottomEdgeHeightPx;
      candidate_ = startedInActivationBand_;
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
    completedActivationBand_ = startedInActivationBand_;
    active_ = false;
    candidate_ = false;
    startedInActivationBand_ = false;
    startX_ = 0;
    startY_ = 0;
    startedAtMs_ = 0;
    return recognised;
  }

  // Consume each sample belonging to a bottom-origin contact, even when it
  // fails the swipe threshold. This prevents an attempted home gesture from
  // leaking into Weather's metric/list drag handlers.
  bool consumeTouchSample() {
    const bool consume = (active_ && startedInActivationBand_) ||
        completedActivationBand_;
    completedActivationBand_ = false;
    return consume;
  }

  void reset() {
    active_ = false;
    candidate_ = false;
    startedInActivationBand_ = false;
    completedActivationBand_ = false;
    startX_ = 0;
    startY_ = 0;
    startedAtMs_ = 0;
  }

 private:
  bool active_;
  bool candidate_;
  bool startedInActivationBand_;
  bool completedActivationBand_;
  int startX_;
  int startY_;
  uint32_t startedAtMs_;

  static bool isInsideActivationBand(int x, int screenWidth) {
    const int bandLeft = (screenWidth - ActivationBandWidthPx) / 2;
    const int bandRight = bandLeft + ActivationBandWidthPx;
    return x >= bandLeft && x < bandRight;
  }

  static int absolute(int value) {
    return value < 0 ? -value : value;
  }

  static uint32_t elapsed(uint32_t nowMs, uint32_t startMs) {
    return static_cast<uint32_t>(nowMs - startMs);
  }
};

#endif  // ESP32_GADGET_BOTTOM_EDGE_SWIPE_GESTURE_H
