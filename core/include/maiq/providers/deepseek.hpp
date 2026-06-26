#pragma once

#include "maiq/config.hpp"
#include "maiq/provider.hpp"
#include "maiq/types.hpp"

namespace maiq {

class DeepSeekProvider : public Provider {
public:
    explicit DeepSeekProvider(const ProviderConfig& config);

    const std::string& name() const override { return name_; }
    Vendor vendor() const override { return Vendor::DeepSeek; }
    QueryMode mode() const override { return QueryMode::Balance; }
    AccountStatus query(HttpClient& client) const override;

private:
    std::string name_;
    std::string api_key_;
    std::string base_url_;
};

} // namespace maiq
