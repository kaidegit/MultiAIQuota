#include "maiq/providers/codex.hpp"

#include "maiq/error.hpp"

namespace maiq {

CodexProvider::CodexProvider(const ProviderConfig& config) : name_(config.name) {}

AccountStatus CodexProvider::query(HttpClient&) const {
    throw Error(Error::Code::UnsupportedMode, "codex provider not implemented");
    return AccountStatus::invalid(Vendor::Codex, name_, QueryMode::CodingPlan, "not implemented");
}

} // namespace maiq
