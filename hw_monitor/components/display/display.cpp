#include "display.hpp"
#include "board.hpp"

#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lvgl.h>

namespace hw {

namespace {

static const char* TAG = "display";

// ST7735 command set
constexpr uint8_t CMD_SWRESET   = 0x01;
constexpr uint8_t CMD_SLPOUT    = 0x11;
constexpr uint8_t CMD_FRMCTR1   = 0xB1;
constexpr uint8_t CMD_FRMCTR2   = 0xB2;
constexpr uint8_t CMD_FRMCTR3   = 0xB3;
constexpr uint8_t CMD_INVCTR    = 0xB4;
constexpr uint8_t CMD_PWCTR1    = 0xC0;
constexpr uint8_t CMD_PWCTR2    = 0xC1;
constexpr uint8_t CMD_PWCTR3    = 0xC2;
constexpr uint8_t CMD_PWCTR4    = 0xC3;
constexpr uint8_t CMD_PWCTR5    = 0xC4;
constexpr uint8_t CMD_VMCTR1    = 0xC5;
constexpr uint8_t CMD_INVOFF    = 0x20;
constexpr uint8_t CMD_MADCTL    = 0x36;
constexpr uint8_t CMD_COLMOD    = 0x3A;
constexpr uint8_t CMD_CASET     = 0x2A;
constexpr uint8_t CMD_RASET     = 0x2B;
constexpr uint8_t CMD_RAMWR     = 0x2C;
constexpr uint8_t CMD_DISPON    = 0x29;

// MY=1, MX=0, MV=1 -> 270° rotation (landscape 160x128).
// If colors are wrong (BGR instead of RGB), change bit 3 (0xA8).
constexpr uint8_t MADCTL_LANDSCAPE = 0xA0;

constexpr uint32_t SPI_CLOCK_HZ = 10 * 1000 * 1000;
constexpr spi_host_device_t SPI_HOST = SPI2_HOST;

spi_device_handle_t spi_dev = nullptr;

// Partial render buffer (about 1/10 of the screen).
static lv_color_t disp_buf1[xueersi::LCD_WIDTH * xueersi::LCD_HEIGHT / 10];

static void spi_pre_transfer_set_dc(spi_transaction_t* t) {
    gpio_num_t dc_pin = xueersi::PIN_LCD_DC;
    bool is_cmd = (reinterpret_cast<uintptr_t>(t->user) == 0);
    gpio_set_level(dc_pin, is_cmd ? 0 : 1);
}

static void lcd_send(const uint8_t* data, size_t len, bool is_cmd) {
    if (!spi_dev || !data || len == 0) return;

    gpio_set_level(xueersi::PIN_LCD_CS, 0);
    gpio_set_level(xueersi::PIN_LCD_DC, is_cmd ? 0 : 1);

    spi_transaction_t t = {};
    t.length = len * 8;
    t.tx_buffer = data;
    t.user = reinterpret_cast<void*>(is_cmd ? 0 : 1);
    spi_device_polling_transmit(spi_dev, &t);

    gpio_set_level(xueersi::PIN_LCD_CS, 1);
}

static void lcd_cmd(uint8_t cmd) {
    lcd_send(&cmd, 1, true);
}

static void lcd_data(const uint8_t* data, size_t len) {
    lcd_send(data, len, false);
}

static void lcd_cmd_data(uint8_t cmd, const uint8_t* data, size_t len) {
    lcd_cmd(cmd);
    if (data && len) lcd_data(data, len);
}

static void st7735_init_sequence() {
    lcd_cmd(CMD_SWRESET);
    vTaskDelay(pdMS_TO_TICKS(150));

    lcd_cmd(CMD_SLPOUT);
    vTaskDelay(pdMS_TO_TICKS(120));

    static const uint8_t frmctr1[] = {0x01, 0x2C, 0x2D};
    static const uint8_t frmctr2[] = {0x01, 0x2C, 0x2D};
    static const uint8_t frmctr3[] = {0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D};
    static const uint8_t invctr[]  = {0x07};
    static const uint8_t pwctr1[]  = {0xA2, 0x02, 0x84};
    static const uint8_t pwctr2[]  = {0xC5};
    static const uint8_t pwctr3[]  = {0x0A, 0x00};
    static const uint8_t pwctr4[]  = {0x8A, 0x2A};
    static const uint8_t pwctr5[]  = {0x8A, 0xEE};
    static const uint8_t vmctr1[]  = {0x0E};

    lcd_cmd_data(CMD_FRMCTR1, frmctr1, sizeof(frmctr1));
    lcd_cmd_data(CMD_FRMCTR2, frmctr2, sizeof(frmctr2));
    lcd_cmd_data(CMD_FRMCTR3, frmctr3, sizeof(frmctr3));
    lcd_cmd_data(CMD_INVCTR,  invctr,  sizeof(invctr));
    lcd_cmd_data(CMD_PWCTR1,  pwctr1,  sizeof(pwctr1));
    lcd_cmd_data(CMD_PWCTR2,  pwctr2,  sizeof(pwctr2));
    lcd_cmd_data(CMD_PWCTR3,  pwctr3,  sizeof(pwctr3));
    lcd_cmd_data(CMD_PWCTR4,  pwctr4,  sizeof(pwctr4));
    lcd_cmd_data(CMD_PWCTR5,  pwctr5,  sizeof(pwctr5));
    lcd_cmd_data(CMD_VMCTR1,  vmctr1,  sizeof(vmctr1));

    lcd_cmd(CMD_INVOFF);

    static const uint8_t madctl[] = {MADCTL_LANDSCAPE};
    lcd_cmd_data(CMD_MADCTL, madctl, sizeof(madctl));

    static const uint8_t colmod[] = {0x05}; // 16-bit/pixel
    lcd_cmd_data(CMD_COLMOD, colmod, sizeof(colmod));

    lcd_cmd(CMD_DISPON);
    vTaskDelay(pdMS_TO_TICKS(100));
}

static void set_addr_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    uint8_t xs[4] = {0, static_cast<uint8_t>(x1), 0, static_cast<uint8_t>(x2)};
    uint8_t ys[4] = {0, static_cast<uint8_t>(y1), 0, static_cast<uint8_t>(y2)};
    lcd_cmd_data(CMD_CASET, xs, sizeof(xs));
    lcd_cmd_data(CMD_RASET, ys, sizeof(ys));
}

static void flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    int32_t w = lv_area_get_width(area);
    int32_t h = lv_area_get_height(area);
    size_t len = static_cast<size_t>(w) * static_cast<size_t>(h) * sizeof(lv_color_t);

    set_addr_window(area->x1, area->y1, area->x2, area->y2);
    lcd_cmd_data(CMD_RAMWR, px_map, len);

    lv_display_flush_ready(disp);
}

} // namespace

lv_display_t* display_init() {
    ESP_LOGI(TAG, "Initializing ST7735 display");

    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num = xueersi::PIN_LCD_MOSI;
    bus_cfg.miso_io_num = -1;
    bus_cfg.sclk_io_num = xueersi::PIN_LCD_SCLK;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.max_transfer_sz = 65536;

    esp_err_t ret = spi_bus_initialize(SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_initialize failed: %s", esp_err_to_name(ret));
        return nullptr;
    }

    spi_device_interface_config_t dev_cfg = {};
    dev_cfg.clock_speed_hz = SPI_CLOCK_HZ;
    dev_cfg.mode = 0;
    dev_cfg.spics_io_num = -1; // CS is handled manually
    dev_cfg.queue_size = 1;
    dev_cfg.pre_cb = spi_pre_transfer_set_dc;

    ret = spi_bus_add_device(SPI_HOST, &dev_cfg, &spi_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_add_device failed: %s", esp_err_to_name(ret));
        return nullptr;
    }

    st7735_init_sequence();

    lv_display_t* disp = lv_display_create(xueersi::LCD_WIDTH, xueersi::LCD_HEIGHT);
    lv_display_set_flush_cb(disp, flush_cb);
    lv_display_set_buffers(disp, disp_buf1, nullptr, sizeof(disp_buf1),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    return disp;
}

} // namespace hw
