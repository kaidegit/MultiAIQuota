#include "gui/app_state.hpp"
#include "gui/display_spec.hpp"
#include "gui/pages/dashboard_page.hpp"

#include "maiq/config.hpp"
#include "maiq/platform/http_client_pc.hpp"
#include "maiq/query.hpp"

#include <lvgl.h>
#include <SDL2/SDL.h>

#ifndef MAIQ_GUI_HOST_SAVE_SNAPSHOT
#define MAIQ_GUI_HOST_SAVE_SNAPSHOT 0
#endif

#if MAIQ_GUI_HOST_SAVE_SNAPSHOT
#include <src/others/snapshot/lv_snapshot.h>
#endif

#include <cstdio>
#include <ctime>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>

namespace {

std::string read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

struct AppContext {
    gui::AppState* state = nullptr;
    gui::pages::DashboardPage* page = nullptr;
    maiq::Config* cfg = nullptr;
    maiq::HttpClient* client = nullptr;
};

static void perform_refresh(AppContext& ctx) {
    if (!ctx.state || !ctx.page || !ctx.cfg || !ctx.client) return;
    if (ctx.state->querying) return;
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

#if MAIQ_GUI_HOST_SAVE_SNAPSHOT
static void save_snapshot(lv_obj_t* scr, const char* path) {
    if (!scr) return;

    int32_t w = lv_obj_get_width(scr);
    int32_t h = lv_obj_get_height(scr);
    if (w <= 0 || h <= 0) return;

    lv_draw_buf_t* snap = lv_snapshot_take(scr, LV_COLOR_FORMAT_RGB888);
    if (!snap || !snap->data) return;

    FILE* f = std::fopen(path, "wb");
    if (f) {
        std::fprintf(f, "P6\n%d %d\n255\n", w, h);
        const uint8_t* data = static_cast<const uint8_t*>(snap->data);
        int32_t stride = snap->header.stride;
        for (int32_t y = 0; y < h; ++y) {
            const uint8_t* row = data + y * stride;
            for (int32_t x = 0; x < w; ++x) {
                std::fwrite(row + x * 3, 1, 3, f);
            }
        }
        std::fclose(f);
    }
    lv_draw_buf_destroy(snap);
}

static void load_snapshot_demo_data(gui::AppState& state) {
    std::time_t now = std::time(nullptr);

    maiq::AccountStatus demo(maiq::Vendor::Kimi, "demo-kimi", maiq::QueryMode::CodingPlan, {});
    demo.entries.emplace_back("3h", "requests");
    demo.entries.back().with_remaining(42).with_total(50).with_reset_at(now + 78 * 60);
    demo.entries.emplace_back("weekly", "requests");
    demo.entries.back().with_remaining(318).with_total(500).with_reset_at(now + 3 * 86400 + 5 * 3600);

    state.statuses = {demo};
    state.last_refresh_at = now;
}
#endif

} // namespace

int main(int argc, char** argv) {
    (void)argc; (void)argv;

    lv_init();

    lv_display_t* disp = lv_sdl_window_create(160, 128);
    if (!disp) {
        fprintf(stderr, "Failed to create SDL window\n");
        return 1;
    }

    lv_obj_t* scr = lv_display_get_screen_active(disp);

    gui::AppState state;
    state.wifi_status = "PC host mode";

    maiq::Config cfg;
    std::unique_ptr<maiq::HttpClient> client;
#if MAIQ_GUI_HOST_SAVE_SNAPSHOT
    load_snapshot_demo_data(state);
#else
    try {
        std::string config_text = read_file("./tests/live_config.json");
        if (config_text.empty()) {
            config_text = read_file("./maiq.json");
        }
        cfg = maiq::Config::from_json_string(config_text);
        client = maiq::create_http_client();
    } catch (const std::exception& e) {
        state.last_error = e.what();
    }
#endif

    gui::pages::DashboardPage page(scr, state, gui::XUEERSI_SPEC);
    page.create();

    AppContext ctx;
    ctx.state = &state;
    ctx.page = &page;
    ctx.cfg = &cfg;
    ctx.client = client.get();

    if (client) {
        perform_refresh(ctx);
    }

    lv_timer_t* cycle_timer = lv_timer_create(cycle_timer_cb, 10000, &ctx);
    lv_timer_t* refresh_timer = lv_timer_create(refresh_timer_cb, 5 * 60 * 1000, &ctx);

    bool running = true;
#if MAIQ_GUI_HOST_SAVE_SNAPSHOT
    bool snapshot_done = false;
    auto snapshot_start = std::chrono::steady_clock::now();
#endif
    while (running) {
        uint32_t delay = lv_timer_handler();

#if MAIQ_GUI_HOST_SAVE_SNAPSHOT
        if (!snapshot_done) {
            auto elapsed = std::chrono::steady_clock::now() - snapshot_start;
            if (elapsed > std::chrono::seconds(3)) {
                save_snapshot(scr, "/tmp/maiq_snapshot.ppm");
                snapshot_done = true;
                running = false;
            }
        }
#endif

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = false;
            } else if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_1:
                        if (!state.statuses.empty()) {
                            state.selected_account_index =
                                (state.selected_account_index + 1) % state.statuses.size();
                            page.update();
                        }
                        break;
                    case SDLK_2:
                        if (refresh_timer) lv_timer_reset(refresh_timer);
                        perform_refresh(ctx);
                        break;
                    default:
                        break;
                }
            }
        }

        if (delay > 0 && delay < 1000) {
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    lv_timer_delete(cycle_timer);
    lv_timer_delete(refresh_timer);
    lv_display_delete(disp);
    lv_deinit();
    return 0;
}
