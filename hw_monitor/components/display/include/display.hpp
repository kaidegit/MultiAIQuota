#pragma once

#include <esp_lcd_types.h>
#include <lvgl.h>

namespace hw {

// Initialize the ST7735 display using the esp_lcd framework and return the
// LVGL display handle. Returns nullptr on failure.
lv_display_t* display_init();

// Stand-alone screen-communication test (no LVGL). Useful for debugging.
void display_test(esp_lcd_panel_handle_t panel);

} // namespace hw
