#include "query.hpp"

#include "maiq/platform/http_client_pc.hpp"
#include "maiq/query.hpp"
#include "../output.hpp"

#include <algorithm>
#include <cctype>

namespace maiq::cli::commands {

namespace {

bool iequals(const std::string& a, const std::string& b) {
    return std::equal(a.begin(), a.end(), b.begin(), b.end(),
                      [](char ca, char cb) { return std::tolower(ca) == std::tolower(cb); });
}

} // namespace

std::string run_query(const Config& config, const Args& args) {
    std::vector<ProviderConfig> filtered;
    for (const auto& acc : config.accounts) {
        if (args.vendor_filter && !iequals(to_string(acc.vendor), *args.vendor_filter)) continue;
        if (args.account_filter && !iequals(acc.name, *args.account_filter)) continue;
        filtered.push_back(acc);
    }
    if (filtered.empty()) {
        return "No accounts matched the filter.";
    }

    auto client = create_http_client();
    auto statuses = query_all_statuses(*client, filtered);
    return render(args.format, statuses);
}

} // namespace maiq::cli::commands
