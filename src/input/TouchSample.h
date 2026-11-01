#pragma once

#include <stdint.h>

// Raw touch-controller sample shared by the hardware reader and Weather
// interaction code. Keep this as a data-only value type: gesture recognition
// and view-specific ownership remain in their existing sketch functions.
struct TouchSample {
  uint8_t gesture;
  uint8_t fingerCount;
  uint8_t event;
  uint16_t rawX;
  uint16_t rawY;
};
