#pragma once

#include <stdint.h>

// Temporary compile-only marker for validating Arduino's sketch /src layout.
// This file is intentionally tiny and must not become a production abstraction.
namespace phase0_layout_spike {
constexpr uint8_t kVersion = 1;
uint8_t linkCheck();
}
