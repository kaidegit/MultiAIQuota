#include "display.hpp"
#include "board.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <esp_check.h>
#include <esp_heap_caps.h>
#include <esp_lcd_panel_commands.h>
#include <esp_lcd_panel_dev.h>
#include <esp_lcd_panel_interface.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_io_spi.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lvgl.h>

namespace hw {

namespace {

static const char* TAG = "display";

constexpr uint32_t SPI_CLOCK_HZ = 10 * 1000 * 1000;
constexpr spi_host_device_t SPI_HOST = SPI2_HOST;

// ST7735-specific commands not covered by esp_lcd_panel_commands.h.
constexpr uint8_t ST7735_CMD_FRMCTR1 = 0xB1;
constexpr uint8_t ST7735_CMD_FRMCTR2 = 0xB2;
constexpr uint8_t ST7735_CMD_FRMCTR3 = 0xB3;
constexpr uint8_t ST7735_CMD_INVCTR  = 0xB4;
constexpr uint8_t ST7735_CMD_PWCTR1  = 0xC0;
constexpr uint8_t ST7735_CMD_PWCTR2  = 0xC1;
constexpr uint8_t ST7735_CMD_PWCTR3  = 0xC2;
constexpr uint8_t ST7735_CMD_PWCTR4  = 0xC3;
constexpr uint8_t ST7735_CMD_PWCTR5  = 0xC4;
constexpr uint8_t ST7735_CMD_VMCTR1  = 0xC5;

// Landscape orientation for the 160x128 Xueersi screen, rotated 180°:
// MY=1, MX=1, MV=1, RGB order (bit 3 clear).
constexpr uint8_t ST7735_MADCTL_LANDSCAPE = 0x60;

struct st7735_panel_t {
    esp_lcd_panel_t base;
    esp_lcd_panel_io_handle_t io;
    int reset_gpio_num;
    bool reset_level;
    int x_gap;
    int y_gap;
    uint8_t fb_bits_per_pixel;
    uint8_t madctl_val;
    uint8_t colmod_val;
};

static inline st7735_panel_t* panel_to_st7735(esp_lcd_panel_t* panel) {
    return reinterpret_cast<st7735_panel_t*>(
        reinterpret_cast<char*>(panel) - offsetof(st7735_panel_t, base));
}

static esp_err_t panel_st7735_del(esp_lcd_panel_t* panel) {
    st7735_panel_t* st7735 = panel_to_st7735(panel);
    if (st7735->reset_gpio_num >= 0) {
        gpio_reset_pin(static_cast<gpio_num_t>(st7735->reset_gpio_num));
    }
    free(st7735);
    return ESP_OK;
}

static esp_err_t panel_st7735_reset(esp_lcd_panel_t* panel) {
    st7735_panel_t* st7735 = panel_to_st7735(panel);

    if (st7735->reset_gpio_num >= 0) {
        gpio_set_level(static_cast<gpio_num_t>(st7735->reset_gpio_num),
                       st7735->reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(static_cast<gpio_num_t>(st7735->reset_gpio_num),
                       !st7735->reset_level);
        vTaskDelay(pdMS_TO_TICKS(20));
    } else {
        ESP_RETURN_ON_ERROR(
            esp_lcd_panel_io_tx_param(st7735->io, LCD_CMD_SWRESET, nullptr, 0),
            TAG, "SWRESET failed");
        vTaskDelay(pdMS_TO_TICKS(150));
    }
    return ESP_OK;
}

static esp_err_t panel_st7735_init(esp_lcd_panel_t* panel) {
    st7735_panel_t* st7735 = panel_to_st7735(panel);
    esp_lcd_panel_io_handle_t io = st7735->io;

    ESP_RETURN_ON_ERROR(
        esp_lcd_panel_io_tx_param(io, LCD_CMD_SWRESET, nullptr, 0),
        TAG, "SWRESET failed");
    vTaskDelay(pdMS_TO_TICKS(150));

    ESP_RETURN_ON_ERROR(
        esp_lcd_panel_io_tx_param(io, LCD_CMD_SLPOUT, nullptr, 0),
        TAG, "SLPOUT failed");
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

    ESP_RETURN_ON_ERROR(
        esp_lcd_panel_io_tx_param(io, ST7735_CMD_FRMCTR1, frmctr1, sizeof(frmctr1)),
        TAG, "FRMCTR1 failed");
    ESP_RETURN_ON_ERROR(
        esp_lcd_panel_io_tx_param(io, ST7735_CMD_FRMCTR2, frmctr2, sizeof(frmctr2)),
        TAG, "FRMCTR2 failed");
    ESP_RETURN_ON_ERROR(
        esp_lcd_panel_io_tx_param(io, ST7735_CMD_FRMCTR3, frmctr3, sizeof(frmctr3)),
        TAG, "FRMCTR3 failed");
    ESP_RETURN_ON_ERROR(
        esp_lcd_panel_io_tx_param(io, ST7735_CMD_INVCTR, invctr, sizeof(invctr)),
        TAG, "INVCTR failed");
    ESP_RETURN_ON_ERROR(
        esp_lcd_panel_io_tx_param(io, ST7735_CMD_PWCTR1, pwctr1, sizeof(pwctr1)),
        TAG, "PWCTR1 failed");
    ESP_RETURN_ON_ERROR(
        esp_lcd_panel_io_tx_param(io, ST7735_CMD_PWCTR2, pwctr2, sizeof(pwctr2)),
        TAG, "PWCTR2 failed");
    ESP_RETURN_ON_ERROR(
        esp_lcd_panel_io_tx_param(io, ST7735_CMD_PWCTR3, pwctr3, sizeof(pwctr3)),
        TAG, "PWCTR3 failed");
    ESP_RETURN_ON_ERROR(
        esp_lcd_panel_io_tx_param(io, ST7735_CMD_PWCTR4, pwctr4, sizeof(pwctr4)),
        TAG, "PWCTR4 failed");
    ESP_RETURN_ON_ERROR(
        esp_lcd_panel_io_tx_param(io, ST7735_CMD_PWCTR5, pwctr5, sizeof(pwctr5)),
        TAG, "PWCTR5 failed");
    ESP_RETURN_ON_ERROR(
        esp_lcd_panel_io_tx_param(io, ST7735_CMD_VMCTR1, vmctr1, sizeof(vmctr1)),
        TAG, "VMCTR1 failed");

    ESP_RETURN_ON_ERROR(
        esp_lcd_panel_io_tx_param(io, LCD_CMD_INVOFF, nullptr, 0),
        TAG, "INVOFF failed");

    ESP_RETURN_ON_ERROR(
        esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, &st7735->madctl_val, 1),
        TAG, "MADCTL failed");

    ESP_RETURN_ON_ERROR(
        esp_lcd_panel_io_tx_param(io, LCD_CMD_COLMOD, &st7735->colmod_val, 1),
        TAG, "COLMOD failed");

    ESP_RETURN_ON_ERROR(
        esp_lcd_panel_io_tx_param(io, LCD_CMD_DISPON, nullptr, 0),
        TAG, "DISPON failed");
    vTaskDelay(pdMS_TO_TICKS(100));

    return ESP_OK;
}

static esp_err_t panel_st7735_draw_bitmap(esp_lcd_panel_t* panel,
                                          int x_start, int y_start,
                                          int x_end, int y_end,
                                          const void* color_data) {
    st7735_panel_t* st7735 = panel_to_st7735(panel);
    esp_lcd_panel_io_handle_t io = st7735->io;

    x_start += st7735->x_gap;
    x_end   += st7735->x_gap;
    y_start += st7735->y_gap;
    y_end   += st7735->y_gap;

    const uint8_t caset[] = {
        static_cast<uint8_t>((x_start >> 8) & 0xFF),
        static_cast<uint8_t>(x_start & 0xFF),
        static_cast<uint8_t>(((x_end - 1) >> 8) & 0xFF),
        static_cast<uint8_t>((x_end - 1) & 0xFF),
    };
    const uint8_t raset[] = {
        static_cast<uint8_t>((y_start >> 8) & 0xFF),
        static_cast<uint8_t>(y_start & 0xFF),
        static_cast<uint8_t>(((y_end - 1) >> 8) & 0xFF),
        static_cast<uint8_t>((y_end - 1) & 0xFF),
    };

    ESP_RETURN_ON_ERROR(
        esp_lcd_panel_io_tx_param(io, LCD_CMD_CASET, caset, sizeof(caset)),
        TAG, "CASET failed");
    ESP_RETURN_ON_ERROR(
        esp_lcd_panel_io_tx_param(io, LCD_CMD_RASET, raset, sizeof(raset)),
        TAG, "RASET failed");

    const size_t len = static_cast<size_t>(x_end - x_start) *
                       static_cast<size_t>(y_end - y_start) *
                       st7735->fb_bits_per_pixel / 8;
    ESP_RETURN_ON_ERROR(
        esp_lcd_panel_io_tx_color(io, LCD_CMD_RAMWR, color_data, len),
        TAG, "RAMWR failed");

    return ESP_OK;
}

static esp_err_t panel_st7735_invert_color(esp_lcd_panel_t* panel,
                                           bool invert_color_data) {
    st7735_panel_t* st7735 = panel_to_st7735(panel);
    const int cmd = invert_color_data ? LCD_CMD_INVON : LCD_CMD_INVOFF;
    ESP_RETURN_ON_ERROR(
        esp_lcd_panel_io_tx_param(st7735->io, cmd, nullptr, 0),
        TAG, "invert_color failed");
    return ESP_OK;
}

static esp_err_t panel_st7735_mirror(esp_lcd_panel_t* panel,
                                     bool mirror_x, bool mirror_y) {
    st7735_panel_t* st7735 = panel_to_st7735(panel);
    if (mirror_x) {
        st7735->madctl_val |= LCD_CMD_MX_BIT;
    } else {
        st7735->madctl_val &= ~LCD_CMD_MX_BIT;
    }
    if (mirror_y) {
        st7735->madctl_val |= LCD_CMD_MY_BIT;
    } else {
        st7735->madctl_val &= ~LCD_CMD_MY_BIT;
    }
    ESP_RETURN_ON_ERROR(
        esp_lcd_panel_io_tx_param(st7735->io, LCD_CMD_MADCTL,
                                  &st7735->madctl_val, 1),
        TAG, "mirror failed");
    return ESP_OK;
}

static esp_err_t panel_st7735_swap_xy(esp_lcd_panel_t* panel, bool swap_axes) {
    st7735_panel_t* st7735 = panel_to_st7735(panel);
    if (swap_axes) {
        st7735->madctl_val |= LCD_CMD_MV_BIT;
    } else {
        st7735->madctl_val &= ~LCD_CMD_MV_BIT;
    }
    ESP_RETURN_ON_ERROR(
        esp_lcd_panel_io_tx_param(st7735->io, LCD_CMD_MADCTL,
                                  &st7735->madctl_val, 1),
        TAG, "swap_xy failed");
    return ESP_OK;
}

static esp_err_t panel_st7735_set_gap(esp_lcd_panel_t* panel,
                                      int x_gap, int y_gap) {
    st7735_panel_t* st7735 = panel_to_st7735(panel);
    st7735->x_gap = x_gap;
    st7735->y_gap = y_gap;
    return ESP_OK;
}

static esp_err_t panel_st7735_disp_on_off(esp_lcd_panel_t* panel, bool on_off) {
    st7735_panel_t* st7735 = panel_to_st7735(panel);
    const int cmd = on_off ? LCD_CMD_DISPON : LCD_CMD_DISPOFF;
    ESP_RETURN_ON_ERROR(
        esp_lcd_panel_io_tx_param(st7735->io, cmd, nullptr, 0),
        TAG, "disp_on_off failed");
    return ESP_OK;
}

static esp_err_t panel_st7735_sleep(esp_lcd_panel_t* panel, bool sleep) {
    st7735_panel_t* st7735 = panel_to_st7735(panel);
    const int cmd = sleep ? LCD_CMD_SLPIN : LCD_CMD_SLPOUT;
    ESP_RETURN_ON_ERROR(
        esp_lcd_panel_io_tx_param(st7735->io, cmd, nullptr, 0),
        TAG, "sleep failed");
    return ESP_OK;
}

esp_err_t esp_lcd_new_panel_st7735(const esp_lcd_panel_io_handle_t io,
                                   const esp_lcd_panel_dev_config_t* panel_dev_config,
                                   esp_lcd_panel_handle_t* ret_panel) {
    esp_err_t ret = ESP_OK;
    st7735_panel_t* st7735 = nullptr;

    ESP_GOTO_ON_FALSE(io && panel_dev_config && ret_panel,
                      ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");

    st7735 = reinterpret_cast<st7735_panel_t*>(calloc(1, sizeof(st7735_panel_t)));
    ESP_GOTO_ON_FALSE(st7735, ESP_ERR_NO_MEM, err, TAG, "no memory for ST7735 panel");

    if (panel_dev_config->reset_gpio_num >= 0) {
        gpio_config_t io_conf = {
            .pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num,
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG,
                          "configure GPIO for RST line failed");
    }

    st7735->madctl_val = ST7735_MADCTL_LANDSCAPE;
    switch (panel_dev_config->rgb_ele_order) {
        case LCD_RGB_ELEMENT_ORDER_RGB:
            break;
        case LCD_RGB_ELEMENT_ORDER_BGR:
            st7735->madctl_val |= LCD_CMD_BGR_BIT;
            break;
        default:
            ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG,
                              "unsupported RGB element order");
            break;
    }

    switch (panel_dev_config->bits_per_pixel) {
        case 16: // RGB565
            st7735->colmod_val = 0x05;
            st7735->fb_bits_per_pixel = 16;
            break;
        default:
            ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG,
                              "unsupported pixel width");
            break;
    }

    st7735->io = io;
    st7735->reset_gpio_num = panel_dev_config->reset_gpio_num;
    st7735->reset_level = panel_dev_config->flags.reset_active_high;
    st7735->base.del = panel_st7735_del;
    st7735->base.reset = panel_st7735_reset;
    st7735->base.init = panel_st7735_init;
    st7735->base.draw_bitmap = panel_st7735_draw_bitmap;
    st7735->base.invert_color = panel_st7735_invert_color;
    st7735->base.set_gap = panel_st7735_set_gap;
    st7735->base.mirror = panel_st7735_mirror;
    st7735->base.swap_xy = panel_st7735_swap_xy;
    st7735->base.disp_on_off = panel_st7735_disp_on_off;
    st7735->base.disp_sleep = panel_st7735_sleep;
    *ret_panel = &(st7735->base);
    return ESP_OK;

err:
    if (st7735) {
        if (panel_dev_config && panel_dev_config->reset_gpio_num >= 0) {
            gpio_reset_pin(static_cast<gpio_num_t>(panel_dev_config->reset_gpio_num));
        }
        free(st7735);
    }
    return ret;
}

static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

} // namespace

static bool color_trans_done_cb(esp_lcd_panel_io_handle_t panel_io,
                                esp_lcd_panel_io_event_data_t* edata,
                                void* user_ctx) {
    lv_display_t* disp = static_cast<lv_display_t*>(user_ctx);
    lv_display_flush_ready(disp);
    return false;
}

static void flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    auto panel = static_cast<esp_lcd_panel_handle_t>(lv_display_get_user_data(disp));

    int x1 = area->x1;
    int y1 = area->y1;
    int x2 = area->x2 + 1; // esp_lcd uses exclusive end coordinates
    int y2 = area->y2 + 1;

    esp_err_t err = esp_lcd_panel_draw_bitmap(panel, x1, y1, x2, y2, px_map);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "draw_bitmap failed: %s", esp_err_to_name(err));
        lv_display_flush_ready(disp);
    }
}

lv_display_t* display_init() {
    ESP_LOGI(TAG, "Initializing ST7735 display via esp_lcd for LVGL");

    lv_display_t* disp = lv_display_create(xueersi::LCD_WIDTH, xueersi::LCD_HEIGHT);
    if (!disp) {
        ESP_LOGE(TAG, "lv_display_create failed");
        return nullptr;
    }

    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num = xueersi::PIN_LCD_MOSI;
    bus_cfg.miso_io_num = -1;
    bus_cfg.sclk_io_num = xueersi::PIN_LCD_SCLK;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.max_transfer_sz = xueersi::LCD_WIDTH * xueersi::LCD_HEIGHT *
                              sizeof(uint16_t);

    esp_err_t ret = spi_bus_initialize(SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_initialize failed: %s", esp_err_to_name(ret));
        return nullptr;
    }

    esp_lcd_panel_io_spi_config_t io_cfg = {};
    io_cfg.cs_gpio_num = xueersi::PIN_LCD_CS;
    io_cfg.dc_gpio_num = xueersi::PIN_LCD_DC;
    io_cfg.spi_mode = 0;
    io_cfg.pclk_hz = SPI_CLOCK_HZ;
    io_cfg.trans_queue_depth = 10;
    io_cfg.lcd_cmd_bits = 8;
    io_cfg.lcd_param_bits = 8;
    io_cfg.on_color_trans_done = color_trans_done_cb;
    io_cfg.user_ctx = disp;
    // Default DC polarity: command = low, parameter/data = high.

    esp_lcd_panel_io_handle_t io_handle = nullptr;
    ret = esp_lcd_new_panel_io_spi(SPI_HOST, &io_cfg, &io_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_lcd_new_panel_io_spi failed: %s", esp_err_to_name(ret));
        return nullptr;
    }

    esp_lcd_panel_dev_config_t panel_cfg = {};
    panel_cfg.reset_gpio_num = xueersi::PIN_LCD_RESET;
    panel_cfg.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
    panel_cfg.data_endian = LCD_RGB_DATA_ENDIAN_LITTLE;
    panel_cfg.bits_per_pixel = 16;

    esp_lcd_panel_handle_t panel = nullptr;
    ret = esp_lcd_new_panel_st7735(io_handle, &panel_cfg, &panel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_lcd_new_panel_st7735 failed: %s", esp_err_to_name(ret));
        return nullptr;
    }

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));

    lv_display_set_user_data(disp, panel);
    lv_display_set_flush_cb(disp, flush_cb);

    static uint16_t disp_buf1[xueersi::LCD_WIDTH * xueersi::LCD_HEIGHT / 10];
    lv_display_set_buffers(disp, disp_buf1, nullptr, sizeof(disp_buf1),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    ESP_LOGI(TAG, "Display initialized");
    return disp;
}

void display_test(esp_lcd_panel_handle_t panel) {
    if (!panel) {
        ESP_LOGE(TAG, "panel is null");
        return;
    }

    constexpr int w = xueersi::LCD_WIDTH;
    constexpr int h = xueersi::LCD_HEIGHT;
    constexpr size_t bufsize = w * h * sizeof(uint16_t);

    uint16_t* buf = reinterpret_cast<uint16_t*>(
        heap_caps_malloc(bufsize, MALLOC_CAP_DMA));
    if (!buf) {
        ESP_LOGE(TAG, "Failed to allocate DMA buffer");
        return;
    }

    // Pre-swap bytes so the SPI peripheral transmits the high byte first.
    const uint16_t red   = __builtin_bswap16(rgb565(0xFF, 0x00, 0x00));
    const uint16_t green = __builtin_bswap16(rgb565(0x00, 0xFF, 0x00));
    const uint16_t blue  = __builtin_bswap16(rgb565(0x00, 0x00, 0xFF));
    const uint16_t white = __builtin_bswap16(rgb565(0xFF, 0xFF, 0xFF));
    const uint16_t black = 0x0000;

    const uint16_t colors[] = {red, green, blue, white, black};
    const char* names[] = {"red", "green", "blue", "white", "black"};

    ESP_LOGI(TAG, "Starting screen communication test");
    while (true) {
        for (size_t i = 0; i < sizeof(colors) / sizeof(colors[0]); ++i) {
            std::fill(buf, buf + w * h, colors[i]);
            esp_err_t err = esp_lcd_panel_draw_bitmap(panel, 0, 0, w, h, buf);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "draw_bitmap failed: %s", esp_err_to_name(err));
            } else {
                ESP_LOGI(TAG, "Screen filled with %s", names[i]);
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

} // namespace hw
