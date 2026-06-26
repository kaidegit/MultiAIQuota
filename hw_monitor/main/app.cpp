#include "app.hpp"

#include "gui/app_state.hpp"
#include "gui/display_spec.hpp"
#include "gui/pages/dashboard_page.hpp"

#include "maiq/config.hpp"
#include "maiq/http_client.hpp"
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

#include <ctime>
#include <memory>

namespace app {

static const char* TAG = "app";

namespace {

struct AppContext {
    gui::AppState* state = nullptr;
    gui::pages::DashboardPage* page = nullptr;
    const maiq::Config* cfg = nullptr;
    maiq::HttpClient* client = nullptr;
};

static void perform_refresh(AppContext& ctx) {
    if (!ctx.state || !ctx.page || !ctx.cfg || !ctx.client) return;
    if (ctx.state->querying) return;

    ctx.state->wifi_status = hw::wifi_state_string();
    if (hw::wifi_state() != hw::WifiState::Connected) {
        ctx.state->last_error = "Wi-Fi not connected";
        ctx.page->update();
        return;
    }
    if (ctx.cfg->accounts.empty()) return;

    ctx.state->querying = true;
    ctx.state->last_error.clear();
    ctx.page->update();

    try {
        ctx.state->statuses = maiq::query_all_statuses(*ctx.client, ctx.cfg->accounts);
        ctx.state->last_refresh_at = std::time(nullptr);
        if (!ctx.state->statuses.empty() &&
            ctx.state->selected_account_index >= ctx.state->statuses.size()) {
            ctx.state->selected_account_index = 0;
        }
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "query failed: %s", e.what());
        ctx.state->last_error = e.what();
    }
    ctx.state->querying = false;
    ctx.page->update();
}

static void refresh_timer_cb(lv_timer_t* t) {
    auto* ctx = static_cast<AppContext*>(lv_timer_get_user_data(t));
    if (ctx) perform_refresh(*ctx);
}

static void cycle_timer_cb(lv_timer_t* t) {
    auto* ctx = static_cast<AppContext*>(lv_timer_get_user_data(t));
    if (!ctx || !ctx->state || !ctx->page) return;
    if (ctx->state->statuses.empty()) return;
    ctx->state->selected_account_index =
        (ctx->state->selected_account_index + 1) % ctx->state->statuses.size();
    ctx->page->update();
}

} // namespace

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
    std::unique_ptr<maiq::HttpClient> client;
    if (hw::wifi_state() == hw::WifiState::Connected && !cfg.accounts.empty()) {
        try {
            client = maiq::create_http_client_esp();
        } catch (const std::exception& e) {
            state.last_error = e.what();
        }
    } else if (hw::wifi_state() != hw::WifiState::Connected) {
        state.last_error = "Wi-Fi not connected";
    } else {
        state.last_error = "No accounts configured";
    }

    gui::pages::DashboardPage page(scr, state, gui::XUEERSI_SPEC);
    page.create();

    AppContext ctx;
    ctx.state = &state;
    ctx.page = &page;
    ctx.cfg = &cfg;
    ctx.client = client.get();

    // Perform initial query if we have a client ready.
    if (client) {
        perform_refresh(ctx);
    }

    lv_timer_t* cycle_timer = lv_timer_create(cycle_timer_cb, 10000, &ctx);
    lv_timer_t* refresh_timer = lv_timer_create(refresh_timer_cb, 5 * 60 * 1000, &ctx);

    uint8_t prev_keys = 0;
    while (true) {
        uint32_t delay_ms = lv_timer_handler();

        state.wifi_status = hw::wifi_state_string();

        uint8_t keys = hw::input_read_keys();
        uint8_t pressed = keys & ~prev_keys;
        if ((pressed & 0x01) && !state.statuses.empty()) {
            state.selected_account_index =
                (state.selected_account_index + 1) % state.statuses.size();
            page.update();
        }
        if (pressed & 0x02) {
            if (refresh_timer) lv_timer_reset(refresh_timer);
            perform_refresh(ctx);
        }
        prev_keys = keys;

        vTaskDelay(pdMS_TO_TICKS(delay_ms > 0 ? delay_ms : 5));
    }
}

} // namespace app
