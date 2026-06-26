#pragma once

#include "maiq/types.hpp"

#include <ctime>
#include <optional>
#include <string>
#include <vector>

namespace gui {

struct AppState {
    std::vector<maiq::AccountStatus> statuses;
    std::string wifi_status = "disconnected";
    bool querying = false;
    std::string last_error;

    size_t selected_account_index = 0;
    std::optional<std::time_t> last_refresh_at;

    bool wifi_connected() const {
        return wifi_status == "connected";
    }
};

} // namespace gui
