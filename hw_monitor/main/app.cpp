#include "app.hpp"

#include "gui/app_state.hpp"
#include "gui/display_spec.hpp"
#include "gui/pages/result_page.hpp"

#include "maiq/config.hpp"
#include "maiq/query.hpp"
#include "http_client_esp.hpp"

#include "board.hpp"
#include "display.hpp"
#include "input.hpp"
#include "storage.hpp"
#include "web_server.hpp"
#include "wifi.hpp"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lvgl.h>

namespace app {

static const char* TAG = "app";

void run() {
    if (!hw::board_init() || !hw::storage_init()) {
        ESP_LOGE(TAG, "init failed");
        return;
    }

    lv_init();
    lv_display_t* disp = hw::display_init();
    hw::input_init();

    if (!hw::wifi_ensure_connected()) {
        ESP_LOGW(TAG, "wifi not connected");
    }
    hw::web_server_start();

    lv_obj_t* scr = lv_display_get_screen_active(disp);
    gui::AppState state;
    state.wifi_status = hw::wifi_state_string();

    auto cfg = hw::storage_load_config();
    if (!cfg.accounts.empty()) {
        try {
            auto client = maiq::create_http_client_esp();
            state.statuses = maiq::query_all_statuses(*client, cfg.accounts);
        } catch (const std::exception& e) {
            ESP_LOGE(TAG, "query failed: %s", e.what());
            state.last_error = e.what();
        }
    } else {
        state.last_error = "No accounts configured";
    }

    gui::pages::ResultPage results(scr, state, gui::XUEERSI_SPEC);
    results.create();

    while (true) {
        uint32_t delay_ms = lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(delay_ms > 0 ? delay_ms : 5));
    }
}

} // namespace app
