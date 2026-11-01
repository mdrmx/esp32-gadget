#pragma once

#include <stdint.h>

// Raw touch-controller sample shared by the touch reader and gesture logic.
// Kept in a header so Arduino's generated function prototypes can see the type.
struct TouchSample {
  uint8_t gesture;
  uint8_t fingerCount;
  uint8_t event;
  uint16_t rawX;
  uint16_t rawY;
};
