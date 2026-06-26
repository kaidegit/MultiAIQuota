#pragma once

#include "maiq/config.hpp"
#include "maiq/provider.hpp"
#include "maiq/types.hpp"

namespace maiq {

class KimiProvider : public Provider {
public:
    explicit KimiProvider(const ProviderConfig& config);

    const std::string& name() const override { return name_; }
    Vendor vendor() const override { return Vendor::Kimi; }
    QueryMode mode() const override { return mode_; }
    AccountStatus query(HttpClient& client) const override;

private:
    AccountStatus query_coding_plan(HttpClient& client) const;
    AccountStatus query_balance(HttpClient& client) const;

    std::string name_;
    std::string api_key_;
    QueryMode mode_;
    std::string base_url_;
};

} // namespace maiq
