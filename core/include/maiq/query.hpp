#pragma once

#include "config.hpp"
#include "http_client.hpp"
#include "types.hpp"

#include <vector>

namespace maiq {

AccountStatus query_one(HttpClient& client, const ProviderConfig& config);
std::vector<AccountStatus> query_all_statuses(HttpClient& client, const std::vector<ProviderConfig>& configs);

} // namespace maiq
