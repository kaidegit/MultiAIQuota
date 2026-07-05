#pragma once

#include <cstdint>

namespace hw {

// Initialize input devices (buttons, touch) and register them with LVGL.
void input_init();

// Check and consume the next button press event.
// Returns 0 if no event, 1 for KEY1 (next account / page), 2 for KEY2 (refresh).
uint8_t input_take_button_press();

} // namespace hw
