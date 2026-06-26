#pragma once

#include "maiq/types.hpp"

#include <string>
#include <vector>

namespace maiq::cli {

enum class OutputFormat;

std::string render(OutputFormat format, const std::vector<AccountStatus>& statuses);

} // namespace maiq::cli
