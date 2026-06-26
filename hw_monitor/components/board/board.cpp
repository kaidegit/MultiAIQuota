#include "board.hpp"

#include <driver/gpio.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace hw {

static const char* TAG = "board";

static void configure_output(gpio_num_t pin, uint32_t level) {
    gpio_config_t cfg = {};
    cfg.pin_bit_mask = 1ULL << pin;
    cfg.mode = GPIO_MODE_OUTPUT;
    cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    cfg.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&cfg);
    gpio_set_level(pin, level);
}

static void configure_input(gpio_num_t pin) {
    gpio_config_t cfg = {};
    cfg.pin_bit_mask = 1ULL << pin;
    cfg.mode = GPIO_MODE_INPUT;
    // GPIO34-39 are input-only and have no internal pull-up/down.
    // The Xueersi board is expected to have external pull-ups for the buttons.
    cfg.pull_up_en = GPIO_IS_VALID_OUTPUT_GPIO(pin) ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
    cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    cfg.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&cfg);
}

bool board_init() {
    ESP_LOGI(TAG, "Initializing Xueersi board");

    // LCD control pins.
    configure_output(xueersi::PIN_LCD_CS, 1);
    configure_output(xueersi::PIN_LCD_DC, 0);
    configure_output(xueersi::PIN_LCD_RESET, 1);

    // I2C pins are present but unpopulated; leave them as inputs for now.
    configure_input(xueersi::PIN_I2C_SDA);
    configure_input(xueersi::PIN_I2C_SCL);

    // Active-low buttons.
    configure_input(xueersi::PIN_KEY1);
    configure_input(xueersi::PIN_KEY2);

    // Buzzer (passive); default low.
    configure_output(xueersi::PIN_BUZZER, 0);

    // LCD reset sequence.
    gpio_set_level(xueersi::PIN_LCD_RESET, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(xueersi::PIN_LCD_RESET, 1);
    vTaskDelay(pdMS_TO_TICKS(20));

    return true;
}

} // namespace hw
