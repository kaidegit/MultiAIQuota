#pragma once

#include <cstdint>

namespace hw {

// Initialize input devices (buttons, touch) and register them with LVGL.
void input_init();

// Bitmask of currently pressed hardware keys.
// Bit 0 = KEY1 (next account), Bit 1 = KEY2 (refresh).
// Active-low buttons are debounced internally.
uint8_t input_read_keys();

} // namespace hw
