#pragma once

#include <stdint.h>

// The Weather view currently responsible for an active physical contact.
// Hit-testing and view-specific policy remain in the Weather application.
enum class InteractionOwner : uint8_t {
  None = 0,
  MetricPanel,
  ForecastList,
  ForecastGraph,
  ForecastControl
};

// Tracks one active contact's interaction owner and exposes the arbitration
// decision used to prevent the coarse controller gesture from duplicating a
// coordinate-tracked interaction.
//
// This class deliberately does not perform hit-testing, classify drag intent,
// manage release grace periods, render, or trigger Weather actions. Those
// policies remain in the application layer.
class InteractionArbiter {
 public:
  void beginContact() {
    owner_ = InteractionOwner::None;
  }

  bool claim(InteractionOwner owner) {
    if (owner == InteractionOwner::None ||
        owner_ != InteractionOwner::None) {
      return false;
    }

    owner_ = owner;
    return true;
  }

  bool release(InteractionOwner owner) {
    if (owner == InteractionOwner::None || owner_ != owner) {
      return false;
    }

    owner_ = InteractionOwner::None;
    return true;
  }

  void releaseContact() {
    owner_ = InteractionOwner::None;
  }

  InteractionOwner owner() const {
    return owner_;
  }

  bool owns(InteractionOwner owner) const {
    return owner != InteractionOwner::None && owner_ == owner;
  }

  bool suppressesControllerGesture() const {
    return owner_ != InteractionOwner::None;
  }

 private:
  InteractionOwner owner_ = InteractionOwner::None;
};
