#pragma once

#include "maiq/config.hpp"
#include "maiq/provider.hpp"

namespace maiq {

// OpenAI Codex provider is currently a placeholder.
class CodexProvider : public Provider {
public:
    explicit CodexProvider(const ProviderConfig& config);

    const std::string& name() const override { return name_; }
    Vendor vendor() const override { return Vendor::Codex; }
    QueryMode mode() const override { return QueryMode::CodingPlan; }
    AccountStatus query(HttpClient&) const override;

private:
    std::string name_;
};

} // namespace maiq
