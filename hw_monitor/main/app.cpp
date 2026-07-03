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
#include "time_sync.hpp"
#include "web_server.hpp"
#include "wifi.hpp"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lvgl.h>

#include <cstdio>
#include <ctime>
#include <memory>
#include <optional>
#include <string>

namespace app {

static const char* TAG = "app";

namespace {

std::string fmt_optional_double(const std::optional<double>& v) {
    if (!v) return "null";
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%g", *v);
    return buf;
}

std::string fmt_optional_time(std::optional<time_t> t) {
    if (!t) return "null";
    char buf[32];
    struct tm tm_info;
    localtime_r(&*t, &tm_info);
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_info);
    return buf;
}

void log_account_status(const maiq::AccountStatus& s) {
    if (!s.is_valid) {
        ESP_LOGI(TAG, "parsed account %s (%s, %s): invalid - %s",
                 s.account_name.c_str(), maiq::to_string(s.vendor).c_str(),
                 maiq::to_string(s.mode).c_str(),
                 s.invalid_message ? s.invalid_message->c_str() : "unknown error");
        return;
    }

    std::string entries_str;
    for (const auto& e : s.entries) {
        if (!entries_str.empty()) entries_str += "; ";
        entries_str += e.name + "={"
                         "used:" + fmt_optional_double(e.used) + ", "
                         "remaining:" + fmt_optional_double(e.remaining) + ", "
                         "total:" + fmt_optional_double(e.total) + ", "
                         "unit:" + e.unit + ", "
                         "reset:" + fmt_optional_time(e.reset_at) + "}";
    }

    ESP_LOGI(TAG, "parsed account %s (%s, %s): %zu entries - %s",
             s.account_name.c_str(), maiq::to_string(s.vendor).c_str(),
             maiq::to_string(s.mode).c_str(), s.entries.size(),
             entries_str.empty() ? "none" : entries_str.c_str());
}

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
        ESP_LOGI(TAG, "refreshing %zu account(s)", ctx.cfg->accounts.size());
        ctx.state->statuses = maiq::query_all_statuses(*ctx.client, ctx.cfg->accounts);
        ctx.state->last_refresh_at = std::time(nullptr);
        ESP_LOGI(TAG, "received %zu status result(s)", ctx.state->statuses.size());
        if (!ctx.state->statuses.empty() &&
            ctx.state->selected_account_index >= ctx.state->statuses.size()) {
            ctx.state->selected_account_index = 0;
        }
        for (const auto& s : ctx.state->statuses) {
            log_account_status(s);
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

static void handle_config_saved(AppContext& ctx,
                                maiq::Config& cfg,
                                std::unique_ptr<maiq::HttpClient>& client,
                                lv_timer_t* refresh_timer) {
    ESP_LOGI(TAG, "config saved from web UI, reloading and refreshing");
    if (refresh_timer) lv_timer_reset(refresh_timer);

    cfg = hw::storage_load_config();
    ctx.cfg = &cfg;
    ctx.client = client.get();

    if (cfg.accounts.empty()) {
        ctx.state->statuses.clear();
        ctx.state->last_error = "No accounts configured";
        ctx.page->update();
        return;
    }

    if (hw::wifi_state() != hw::WifiState::Connected) {
        ctx.state->last_error = "Wi-Fi not connected";
        ctx.page->update();
        return;
    }

    if (!client) {
        try {
            client = maiq::create_http_client_esp();
            ctx.client = client.get();
        } catch (const std::exception& e) {
            ctx.state->last_error = e.what();
            ctx.page->update();
            return;
        }
    }

    perform_refresh(ctx);
}

} // namespace

void run() {
    ESP_LOGI(TAG, "starting hw_monitor with status logging");
    if (!hw::board_init() || !hw::storage_init()) {
        ESP_LOGE(TAG, "init failed");
        return;
    }

    lv_init();
    lv_display_t* disp = hw::display_init();
    hw::input_init();

    if (!hw::wifi_ensure_connected()) {
        ESP_LOGW(TAG, "wifi not connected");
    } else {
        hw::time_sync();
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
    ESP_LOGI(TAG, "state addr=%p", static_cast<void*>(&state));

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

        if (hw::web_server_take_config_saved_flag()) {
            handle_config_saved(ctx, cfg, client, refresh_timer);
            lv_refr_now(nullptr);
        }

        vTaskDelay(pdMS_TO_TICKS(delay_ms > 0 ? delay_ms : 5));
    }
}

} // namespace app
