#pragma once

#include <stdint.h>

// Stable semantic names for the CST816 gesture register values currently used
// by the Weather application. Keep these numeric mappings unchanged until
// physical panel behaviour is deliberately revalidated.
enum class GestureCode : uint8_t {
  None = 0x00,
  ScreenLeft = 0x01,
  ScreenRight = 0x02,
  ScreenDown = 0x03,
  ScreenUp = 0x04,
  Unknown = 0xFF
};

// Pure conversion from the raw CST816 gesture register to a known code.
// Values outside the supported range are rejected as Unknown.
constexpr GestureCode decodeGestureCode(uint8_t rawCode) {
  switch (rawCode) {
    case 0x00: return GestureCode::None;
    case 0x01: return GestureCode::ScreenLeft;
    case 0x02: return GestureCode::ScreenRight;
    case 0x03: return GestureCode::ScreenDown;
    case 0x04: return GestureCode::ScreenUp;
    default: return GestureCode::Unknown;
  }
}
