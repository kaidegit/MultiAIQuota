#pragma once

#include "maiq/config.hpp"
#include "maiq/provider.hpp"
#include "maiq/types.hpp"

#include <optional>
#include <string>

namespace maiq {

class DeepSeekProvider : public Provider {
public:
    explicit DeepSeekProvider(const ProviderConfig& config);

    const std::string& name() const override { return name_; }
    Vendor vendor() const override { return Vendor::DeepSeek; }
    QueryMode mode() const override { return QueryMode::Balance; }
    AccountStatus query(HttpClient& client) const override;

private:
    // Query today's cost via the platform internal usage API (needs web token).
    std::optional<double> query_today_cost(HttpClient& client) const;

    std::string name_;
    std::string api_key_;
    // Optional web login token (from browser localStorage.userToken); enables
    // the daily-usage query on top of the balance query.
    std::optional<std::string> web_token_;
    std::string base_url_;
};

} // namespace maiq
