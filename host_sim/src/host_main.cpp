#include "gui/app_state.hpp"
#include "gui/display_spec.hpp"
#include "gui/pages/result_page.hpp"

#include "maiq/config.hpp"
#include "maiq/platform/http_client_pc.hpp"
#include "maiq/query.hpp"

#include <lvgl.h>
#include <SDL2/SDL.h>

#include <cstdio>
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

    // Load config and query
    try {
        auto cfg = maiq::Config::from_json_string(read_file("./maiq.json"));
        auto client = maiq::create_http_client();
        state.statuses = maiq::query_all_statuses(*client, cfg.accounts);
    } catch (const std::exception& e) {
        state.last_error = e.what();
    }

    gui::pages::ResultPage results(scr, state, gui::XUEERSI_SPEC);
    results.create();

    // Run for 10 seconds then exit (host sim smoke test)
    auto start = std::chrono::steady_clock::now();
    bool running = true;
    while (running) {
        uint32_t delay = lv_timer_handler();
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
        }
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed > std::chrono::seconds(10)) running = false;

        if (delay > 0 && delay < 1000) {
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    lv_display_delete(disp);
    lv_deinit();
    return 0;
}
