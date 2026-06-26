#pragma once

#include <driver/gpio.h>

namespace hw {

// Xueersi ESP32 board pinout (from claude-desktop-buddy-esp32).
// The reference board uses an ESP32-WROVER-B with 4 MB flash and no PSRAM.
// If you build for ESP32-S3, these GPIOs are still available, but you should
// switch the IDF target to esp32 for the real hardware.
namespace xueersi {

constexpr gpio_num_t PIN_LCD_CS      = GPIO_NUM_5;
constexpr gpio_num_t PIN_LCD_SCLK    = GPIO_NUM_18;
constexpr gpio_num_t PIN_LCD_MOSI    = GPIO_NUM_23;
constexpr gpio_num_t PIN_LCD_DC      = GPIO_NUM_4;
constexpr gpio_num_t PIN_LCD_RESET   = GPIO_NUM_19;

constexpr gpio_num_t PIN_I2C_SDA     = GPIO_NUM_21;
constexpr gpio_num_t PIN_I2C_SCL     = GPIO_NUM_15;

constexpr gpio_num_t PIN_KEY1        = GPIO_NUM_34; // active-low
constexpr gpio_num_t PIN_KEY2        = GPIO_NUM_12; // active-low

constexpr gpio_num_t PIN_BUZZER      = GPIO_NUM_14;

constexpr uint16_t LCD_WIDTH         = 160;
constexpr uint16_t LCD_HEIGHT        = 128;
constexpr uint8_t  LCD_ROTATION      = 3; // landscape

} // namespace xueersi

// Board-level initialization (GPIO, reset sequences, pull-ups).
bool board_init();

} // namespace hw
