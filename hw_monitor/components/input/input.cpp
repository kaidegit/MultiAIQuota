#include "input.hpp"
#include "board.hpp"

#include <driver/gpio.h>
#include <esp_log.h>
#include <lvgl.h>

namespace hw {

namespace {

static const char* TAG = "input";

static void key_read_cb(lv_indev_t* indev, lv_indev_data_t* data) {
    auto pin = static_cast<gpio_num_t>(reinterpret_cast<uintptr_t>(lv_indev_get_user_data(indev)));
    data->state = (gpio_get_level(pin) == 0)
                      ? LV_INDEV_STATE_PRESSED
                      : LV_INDEV_STATE_RELEASED;
}

static lv_indev_t* register_button(gpio_num_t pin, lv_coord_t x, lv_coord_t y) {
    lv_indev_t* indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_BUTTON);
    lv_indev_set_read_cb(indev, key_read_cb);
    lv_indev_set_user_data(indev, reinterpret_cast<void*>(static_cast<uintptr_t>(pin)));

    static const lv_point_t btn_point = {x, y};
    lv_indev_set_button_points(indev, &btn_point);
    return indev;
}

} // namespace

void input_init() {
    ESP_LOGI(TAG, "Initializing Xueersi buttons");

    // KEY1 maps to the left side of the screen, KEY2 to the right side.
    register_button(xueersi::PIN_KEY1,
                    20,
                    static_cast<lv_coord_t>(xueersi::LCD_HEIGHT / 2));
    register_button(xueersi::PIN_KEY2,
                    static_cast<lv_coord_t>(xueersi::LCD_WIDTH - 20),
                    static_cast<lv_coord_t>(xueersi::LCD_HEIGHT / 2));
}

} // namespace hw
