#include "input.hpp"
#include "board.hpp"

#include "multi_button.h"

#include <driver/gpio.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/timers.h>

namespace hw {

namespace {

static const char* TAG = "input";

// Asynchronous button event passed from the MultiButton timer task to the
// LVGL/main task via a FreeRTOS queue.
struct ButtonEvent {
    uint8_t button_id;
};

static QueueHandle_t button_event_queue = nullptr;
static TimerHandle_t button_tick_timer = nullptr;

// MultiButton instances for KEY1 (next account) and KEY2 (refresh).
Button btn1;
Button btn2;

uint8_t read_button_GPIO(uint8_t button_id) {
    if (button_id == 0) {
        return gpio_get_level(xueersi::PIN_KEY1) == 0 ? 0 : 1;
    }
    if (button_id == 1) {
        return gpio_get_level(xueersi::PIN_KEY2) == 0 ? 0 : 1;
    }
    return 1;
}

void button_event_handler(Button* handle, void* user_data) {
    (void)handle;
    uint8_t id = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(user_data));
    ESP_LOGI(TAG, "button %u press down", id);

    ButtonEvent ev{id};
    if (button_event_queue) {
        xQueueSend(button_event_queue, &ev, 0);
    }
}

static void button_tick_timer_cb(TimerHandle_t xTimer) {
    (void)xTimer;
    button_ticks();
    // static int cnt = 0;
    // cnt++;
    // if (cnt % 100 == 0) {
    //     for (int i = 0; i < 2; ++i) {
    //         auto level = read_button_GPIO(i);
    //         ESP_LOGI(TAG, "button %d level=%d", i, level);
    //     }
    // }
}

} // namespace

void input_init() {
    ESP_LOGI(TAG, "Initializing Xueersi buttons");

    // Queue used to pass button events from the timer task to the LVGL task.
    button_event_queue = xQueueCreate(4, sizeof(ButtonEvent));

    // MultiButton setup: active level 0 (active-low buttons).
    button_init(&btn1, read_button_GPIO, 0, 0);
    button_init(&btn2, read_button_GPIO, 0, 1);
    button_attach(&btn1, BTN_PRESS_DOWN, button_event_handler, reinterpret_cast<void*>(0));
    button_attach(&btn2, BTN_PRESS_DOWN, button_event_handler, reinterpret_cast<void*>(1));
    
    button_start(&btn1);
    button_start(&btn2);

    // 5 ms FreeRTOS software timer drives the MultiButton state machine.
    button_tick_timer = xTimerCreate("btn_tick",
                                     pdMS_TO_TICKS(5),
                                     pdTRUE,
                                     nullptr,
                                     button_tick_timer_cb);
    if (button_tick_timer) {
        xTimerStart(button_tick_timer, 0);
    }
}

uint8_t input_take_button_press() {
    if (!button_event_queue) return 0;
    ButtonEvent ev;
    if (xQueueReceive(button_event_queue, &ev, 0) == pdTRUE) {
        return ev.button_id + 1;  // 0 -> 1 (KEY1), 1 -> 2 (KEY2)
    }
    return 0;
}

} // namespace hw
