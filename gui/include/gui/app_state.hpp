#pragma once

#include "maiq/types.hpp"

#include <vector>
#include <string>

namespace gui {

struct AppState {
    std::vector<maiq::AccountStatus> statuses;
    std::string wifi_status = "disconnected";
    bool querying = false;
    std::string last_error;
};

} // namespace gui
