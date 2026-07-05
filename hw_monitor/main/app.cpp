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
#include <freertos/queue.h>
#include <freertos/semphr.h>
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

struct RefreshResult {
    std::vector<maiq::AccountStatus> statuses;
    std::string error;
    std::optional<std::time_t> refresh_at;
};

static QueueHandle_t refresh_result_queue = nullptr;
static SemaphoreHandle_t refresh_mutex = nullptr;
static constexpr uint32_t kMainLoopMaxDelayMs = 5;

static const char* selected_account_name(const gui::AppState& state) {
    if (state.statuses.empty() || state.selected_account_index >= state.statuses.size()) {
        return "<none>";
    }
    return state.statuses[state.selected_account_index].account_name.c_str();
}

static void apply_refresh_result(AppContext& ctx, RefreshResult* result) {
    if (!result) return;
    if (!ctx.state || !ctx.page) {
        delete result;
        return;
    }

    if (result->error.empty()) {
        size_t before_index = ctx.state->selected_account_index;
        ctx.state->statuses = std::move(result->statuses);
        ctx.state->last_refresh_at = result->refresh_at;
        if (!ctx.state->statuses.empty() &&
            ctx.state->selected_account_index >= ctx.state->statuses.size()) {
            ctx.state->selected_account_index = 0;
        }
        ESP_LOGI(TAG, "received %zu status result(s), selected %zu -> %zu (%s)",
                 ctx.state->statuses.size(), before_index, ctx.state->selected_account_index,
                 selected_account_name(*ctx.state));
        for (const auto& s : ctx.state->statuses) {
            log_account_status(s);
        }
    } else {
        ESP_LOGE(TAG, "query failed: %s", result->error.c_str());
        ctx.state->last_error = std::move(result->error);
    }

    ctx.state->querying = false;
    delete result;
    ctx.page->update();
}

static void refresh_task(void* arg) {
    auto* ctx = static_cast<AppContext*>(arg);
    auto* result = new RefreshResult();

    xSemaphoreTake(refresh_mutex, portMAX_DELAY);
    try {
        if (ctx->client && ctx->cfg) {
            ESP_LOGI(TAG, "refreshing %zu account(s)", ctx->cfg->accounts.size());
            result->statuses = maiq::query_all_statuses(*ctx->client, ctx->cfg->accounts);
            result->refresh_at = std::time(nullptr);
        }
    } catch (const std::exception& e) {
        result->error = e.what();
    }
    xSemaphoreGive(refresh_mutex);

    xQueueSend(refresh_result_queue, &result, portMAX_DELAY);
    vTaskDelete(nullptr);
}

static void start_refresh(AppContext& ctx) {
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

    BaseType_t created = xTaskCreate(refresh_task, "refresh", 8192, &ctx, 5, nullptr);
    if (created != pdPASS) {
        ESP_LOGE(TAG, "failed to create refresh task");
        ctx.state->last_error = "Refresh task failed";
        ctx.state->querying = false;
        ctx.page->update();
    }
}

static void wait_for_refresh(AppContext& ctx) {
    while (ctx.state && ctx.state->querying) {
        RefreshResult* result = nullptr;
        if (xQueueReceive(refresh_result_queue, &result, 0) == pdTRUE) {
            apply_refresh_result(ctx, result);
        }
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

static void refresh_timer_cb(lv_timer_t* t) {
    auto* ctx = static_cast<AppContext*>(lv_timer_get_user_data(t));
    if (ctx) start_refresh(*ctx);
}

static void cycle_timer_cb(lv_timer_t* t) {
    auto* ctx = static_cast<AppContext*>(lv_timer_get_user_data(t));
    if (!ctx || !ctx->state || !ctx->page) return;
    if (ctx->state->statuses.empty()) return;
    size_t before_index = ctx->state->selected_account_index;
    ctx->state->selected_account_index =
        (ctx->state->selected_account_index + 1) % ctx->state->statuses.size();
    ESP_LOGI(TAG, "auto cycle selected %zu -> %zu (%s), statuses=%zu",
             before_index, ctx->state->selected_account_index,
             selected_account_name(*ctx->state), ctx->state->statuses.size());
    ctx->page->update();
}

static void handle_config_saved(AppContext& ctx,
                                maiq::Config& cfg,
                                std::unique_ptr<maiq::HttpClient>& client,
                                lv_timer_t* refresh_timer) {
    ESP_LOGI(TAG, "config saved from web UI, reloading and refreshing");
    if (refresh_timer) lv_timer_reset(refresh_timer);

    // Wait for any ongoing refresh before mutating config/client.
    wait_for_refresh(ctx);

    xSemaphoreTake(refresh_mutex, portMAX_DELAY);
    cfg = hw::storage_load_config();
    ctx.cfg = &cfg;
    ctx.client = client.get();
    xSemaphoreGive(refresh_mutex);

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
            xSemaphoreTake(refresh_mutex, portMAX_DELAY);
            ctx.client = client.get();
            xSemaphoreGive(refresh_mutex);
        } catch (const std::exception& e) {
            ctx.state->last_error = e.what();
            ctx.page->update();
            return;
        }
    }

    start_refresh(ctx);
}

struct SplashScreen {
    lv_obj_t* container = nullptr;
    lv_obj_t* status = nullptr;
};

static SplashScreen create_splash(lv_obj_t* parent) {
    SplashScreen splash;
    splash.container = lv_obj_create(parent);
    lv_obj_set_size(splash.container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(splash.container, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(splash.container, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(splash.container, 0, 0);
    lv_obj_set_style_pad_all(splash.container, 0, 0);

    lv_obj_t* title = lv_label_create(splash.container);
    lv_label_set_text(title, "MultiAIQuota");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -10);

    splash.status = lv_label_create(splash.container);
    lv_label_set_text(splash.status, "Booting...");
    lv_obj_set_style_text_color(splash.status, lv_color_hex(0x9ca3af), 0);
    lv_obj_set_style_text_font(splash.status, &lv_font_montserrat_10, 0);
    lv_obj_align(splash.status, LV_ALIGN_CENTER, 0, 12);

    return splash;
}

static void splash_set_status(SplashScreen& splash, const char* text) {
    if (splash.status) {
        lv_label_set_text(splash.status, text);
    }
    lv_refr_now(nullptr);
}

} // namespace

void run() {
    ESP_LOGI(TAG, "starting hw_monitor with status logging");

    if (!hw::board_init()) {
        ESP_LOGE(TAG, "board init failed");
        return;
    }

    lv_init();
    lv_display_t* disp = hw::display_init();
    if (!disp) {
        ESP_LOGE(TAG, "display init failed");
        return;
    }
    hw::input_init();

    lv_obj_t* scr = lv_display_get_screen_active(disp);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    SplashScreen splash = create_splash(scr);
    lv_refr_now(nullptr);  // Flush a black frame to clear power-on garbage.

    splash_set_status(splash, "Initializing storage...");
    if (!hw::storage_init()) {
        ESP_LOGE(TAG, "storage init failed");
        splash_set_status(splash, "Storage init failed");
        return;
    }

    splash_set_status(splash, "Connecting Wi-Fi...");
    if (!hw::wifi_ensure_connected()) {
        ESP_LOGW(TAG, "wifi not connected");
    } else {
        splash_set_status(splash, "Syncing time...");
        hw::time_sync();
    }

    splash_set_status(splash, "Starting web server...");
    hw::web_server_start();

    splash_set_status(splash, "Loading config...");
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

    // Remove splash and create the main UI.
    lv_obj_clean(scr);
    gui::pages::DashboardPage page(scr, state, gui::XUEERSI_SPEC);
    page.create();
    ESP_LOGI(TAG, "state addr=%p", static_cast<void*>(&state));

    AppContext ctx;
    ctx.state = &state;
    ctx.page = &page;
    ctx.cfg = &cfg;
    ctx.client = client.get();

    // Synchronization primitives for asynchronous refresh.
    refresh_result_queue = xQueueCreate(1, sizeof(RefreshResult*));
    refresh_mutex = xSemaphoreCreateMutex();

    // Perform initial query if we have a client ready.
    if (client) {
        start_refresh(ctx);
    }

    lv_timer_create(cycle_timer_cb, 10000, &ctx);
    lv_timer_t* refresh_timer = lv_timer_create(refresh_timer_cb, 5 * 60 * 1000, &ctx);

    while (true) {
        uint32_t delay_ms = lv_timer_handler();

        state.wifi_status = hw::wifi_state_string();

        // Apply any completed asynchronous refresh.
        RefreshResult* result = nullptr;
        if (xQueueReceive(refresh_result_queue, &result, 0) == pdTRUE) {
            apply_refresh_result(ctx, result);
            lv_refr_now(nullptr);  // Force immediate render of refreshed data.
        }

        uint8_t btn = 0;
        while ((btn = hw::input_take_button_press()) != 0) {
            ESP_LOGI(TAG, "button event btn=%u selected=%zu statuses=%zu account=%s",
                     btn, state.selected_account_index, state.statuses.size(),
                     selected_account_name(state));
            if (btn == 1 && !state.statuses.empty()) {
                size_t before_index = state.selected_account_index;
                state.selected_account_index =
                    (state.selected_account_index + 1) % state.statuses.size();
                ESP_LOGI(TAG, "button next selected %zu -> %zu (%s)",
                         before_index, state.selected_account_index,
                         selected_account_name(state));
                page.update();
                lv_refr_now(nullptr);  // Force immediate render of selected page.
            } else if (btn == 2) {
                ESP_LOGI(TAG, "button refresh selected=%zu (%s), querying=%d",
                         state.selected_account_index, selected_account_name(state),
                         static_cast<int>(state.querying));
                if (refresh_timer) lv_timer_reset(refresh_timer);
                start_refresh(ctx);
                lv_refr_now(nullptr);  // Force immediate render of loading state.
            } else if (btn == 1) {
                ESP_LOGI(TAG, "button next ignored because statuses is empty");
            } else {
                ESP_LOGW(TAG, "unknown button event btn=%u", btn);
            }
        }

        if (hw::web_server_take_config_saved_flag()) {
            handle_config_saved(ctx, cfg, client, refresh_timer);
            lv_refr_now(nullptr);
        }

        if (delay_ms == LV_NO_TIMER_READY || delay_ms > kMainLoopMaxDelayMs) {
            delay_ms = kMainLoopMaxDelayMs;
        }
        vTaskDelay(pdMS_TO_TICKS(delay_ms > 0 ? delay_ms : kMainLoopMaxDelayMs));
    }
}

} // namespace app
