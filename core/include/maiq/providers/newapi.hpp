#pragma once

#include "maiq/config.hpp"
#include "maiq/provider.hpp"

namespace maiq {

// New-API / RightCode provider is currently a placeholder.
class NewApiProvider : public Provider {
public:
    explicit NewApiProvider(const ProviderConfig& config);

    const std::string& name() const override { return name_; }
    Vendor vendor() const override { return Vendor::NewApi; }
    QueryMode mode() const override { return QueryMode::Balance; }
    AccountStatus query(HttpClient&) const override;

private:
    std::string name_;
};

} // namespace maiq
