#pragma once

#include "maiq/config.hpp"
#include "maiq/provider.hpp"

namespace maiq {

// Volcengine provider is currently a placeholder.
class VolcengineProvider : public Provider {
public:
    explicit VolcengineProvider(const ProviderConfig& config);

    const std::string& name() const override { return name_; }
    Vendor vendor() const override { return Vendor::Volcengine; }
    QueryMode mode() const override { return mode_; }
    AccountStatus query(HttpClient&) const override;

private:
    std::string name_;
    QueryMode mode_;
};

} // namespace maiq
