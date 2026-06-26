#pragma once

#include "maiq/config.hpp"
#include "../cli.hpp"

#include <string>

namespace maiq::cli::commands {

std::string run_query(const Config& config, const Args& args);

} // namespace maiq::cli::commands
