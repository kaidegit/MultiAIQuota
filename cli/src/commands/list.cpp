#include "list.hpp"

#include "maiq/config.hpp"

#include <iomanip>
#include <sstream>

namespace maiq::cli::commands {

std::string run_list(const Config& config) {
    std::ostringstream oss;
    oss << std::left
        << std::setw(18) << "Account"
        << std::setw(14) << "Vendor"
        << std::setw(14) << "Mode"
        << "Credentials\n";

    for (const auto& acc : config.accounts) {
        oss << std::left
            << std::setw(18) << acc.name
            << std::setw(14) << to_string(acc.vendor)
            << std::setw(14) << to_string(acc.mode);

        std::visit([&oss](const auto& c) {
            using T = std::decay_t<decltype(c)>;
            if constexpr (std::is_same_v<T, BearerCredentials>) {
                oss << "bearer: " << mask_key(c.api_key);
            } else if constexpr (std::is_same_v<T, VolcengineCredentials>) {
                oss << "ak/sk: " << mask_key(c.ak);
            } else if constexpr (std::is_same_v<T, NewApiCredentials>) {
                oss << "newapi: " << mask_key(c.access_token);
            } else if constexpr (std::is_same_v<T, CodexOAuthCredentials>) {
                oss << "codex_oauth: " << mask_key(c.access_token);
            }
        }, acc.credentials);
        oss << "\n";
    }
    return oss.str();
}

} // namespace maiq::cli::commands
