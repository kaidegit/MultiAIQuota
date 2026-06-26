#pragma once

#include "maiq/config.hpp"

#include <optional>
#include <string>

namespace maiq::cli {

// Load config from (in order):
// 1. explicit path
// 2. MAIQ_CONFIG env variable
// 3. ./maiq.json
// 4. ~/.config/multi-ai-quota/config.json
Config load_config(const std::optional<std::string>& explicit_path);

} // namespace maiq::cli
