#pragma once

#include <stdint.h>

#include "TouchSample.h"
#include "GestureCode.h"

// A controller-level interpretation of one CST816 sample.
// This deliberately does not decide which Weather view owns the contact,
// classify view-specific drags/taps, or trigger navigation.
struct GestureObservation {
  uint8_t rawGesture;
  uint8_t previousRawGesture;
  GestureCode gesture;
  bool rawGestureChanged;
  bool touchPresent;
  bool touchStarted;
  bool touchReleased;
};

// Converts raw controller samples into typed gesture codes and contact edges.
// Weather-specific hit testing, drag ownership, suppression and actions remain
// in the application layer.
class GestureRecognizer {
 public:
  GestureObservation update(const TouchSample& sample) {
    GestureObservation observation;

    observation.rawGesture = sample.gesture;
    observation.previousRawGesture = previousRawGesture_;
    observation.rawGestureChanged =
      sample.gesture != previousRawGesture_;
    observation.gesture = decodeGestureCode(sample.gesture);

    // Match the existing Weather input rule: controller event 1 is touch-up,
    // even if the sample still reports a non-zero finger count.
    observation.touchPresent =
      sample.fingerCount > 0 && sample.event != 1;
    observation.touchStarted =
      observation.touchPresent && !touchWasPresent_;
    observation.touchReleased =
      !observation.touchPresent && touchWasPresent_;

    previousRawGesture_ = sample.gesture;
    touchWasPresent_ = observation.touchPresent;

    return observation;
  }

 private:
  uint8_t previousRawGesture_ = 0;
  bool touchWasPresent_ = false;
};
