#include "maiq/query.hpp"

#include "maiq/provider.hpp"

namespace maiq {

AccountStatus query_one(HttpClient& client, const ProviderConfig& config) {
    auto provider = build_provider(config);
    return provider->query(client);
}

std::vector<AccountStatus> query_all_statuses(HttpClient& client,
                                              const std::vector<ProviderConfig>& configs) {
    std::vector<AccountStatus> results;
    results.reserve(configs.size());
    for (const auto& cfg : configs) {
        try {
            results.push_back(query_one(client, cfg));
        } catch (const Error& e) {
            results.push_back(AccountStatus::invalid(cfg.vendor, cfg.name, cfg.mode, e.what()));
        } catch (const std::exception& e) {
            results.push_back(AccountStatus::invalid(cfg.vendor, cfg.name, cfg.mode, e.what()));
        }
    }
    return results;
}

} // namespace maiq
