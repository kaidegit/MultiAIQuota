#pragma once

#include "maiq/config.hpp"
#include "maiq/provider.hpp"

namespace maiq {

// OpenAI Codex provider.
// Uses the ChatGPT backend /wham/usage endpoint to fetch rate-limit and credit status.
class CodexProvider : public Provider {
public:
    explicit CodexProvider(const ProviderConfig& config);

    const std::string& name() const override { return name_; }
    Vendor vendor() const override { return Vendor::Codex; }
    QueryMode mode() const override { return mode_; }
    AccountStatus query(HttpClient&) const override;

private:
    std::string name_;
    std::string access_token_;
    std::string account_id_;
    std::string base_url_;
    QueryMode mode_;
};

} // namespace maiq
