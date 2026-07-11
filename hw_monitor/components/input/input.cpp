#include "input.hpp"
#include "board.hpp"

#include "iot_button.h"
#include "button_gpio.h"

#include <driver/gpio.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace hw {

static const char* TAG = "input";

// Asynchronous button event passed from the button component callback to the
// main task via a FreeRTOS queue.
struct ButtonEvent {
    uint8_t button_id;  // 1 for KEY1, 2 for KEY2
};

static QueueHandle_t button_event_queue = nullptr;
static button_handle_t btn1 = nullptr;
static button_handle_t btn2 = nullptr;

static void button_event_cb(void* button_handle, void* usr_data) {
    (void)button_handle;
    uint8_t id = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(usr_data));
    ESP_LOGI(TAG, "button %u press down", id);

    ButtonEvent ev{id};
    if (button_event_queue) {
        xQueueSend(button_event_queue, &ev, 0);
    }
}

static void create_button(gpio_num_t pin, button_handle_t* out, uint8_t id,
                          bool disable_pull) {
    button_config_t btn_cfg = {0};
    button_gpio_config_t gpio_cfg = {
        .gpio_num = pin,
        .active_level = 0,        // active-low buttons
        .enable_power_save = true,
        .disable_pull = disable_pull,
    };
    ESP_ERROR_CHECK(iot_button_new_gpio_device(&btn_cfg, &gpio_cfg, out));
    ESP_ERROR_CHECK(iot_button_register_cb(*out, BUTTON_PRESS_DOWN,
                                           nullptr, button_event_cb,
                                           reinterpret_cast<void*>(id)));
}

void input_init() {
    ESP_LOGI(TAG, "Initializing Xueersi buttons via espressif/button");

    button_event_queue = xQueueCreate(4, sizeof(ButtonEvent));

    // GPIO34 is input-only and has no internal pull-up; KEY2 (GPIO12) can use internal pull-up.
    create_button(xueersi::PIN_KEY1, &btn1, 1, true);
    create_button(xueersi::PIN_KEY2, &btn2, 2, false);
}

uint8_t input_take_button_press() {
    if (!button_event_queue) return 0;
    ButtonEvent ev;
    if (xQueueReceive(button_event_queue, &ev, 0) == pdTRUE) {
        return ev.button_id;  // 1 for KEY1, 2 for KEY2
    }
    return 0;
}

} // namespace hw
