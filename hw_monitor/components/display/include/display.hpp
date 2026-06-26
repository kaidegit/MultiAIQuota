#pragma once

#include <lvgl.h>

namespace hw {

// Initialize the display and return the LVGL display handle.
lv_display_t* display_init();

} // namespace hw
