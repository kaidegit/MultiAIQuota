#include "maiq/providers/deepseek.hpp"

#include "maiq/error.hpp"
#include "maiq/json.hpp"

namespace maiq {

namespace {

constexpr const char* DEFAULT_BASE_URL = "https://api.deepseek.com";

} // namespace

DeepSeekProvider::DeepSeekProvider(const ProviderConfig& config)
    : name_(config.name), api_key_([](const Credentials& c) -> std::string {
          if (std::holds_alternative<BearerCredentials>(c)) {
              return std::get<BearerCredentials>(c).api_key;
          }
          throw Error(Error::Code::InvalidCredentials, "deepseek expects bearer api_key");
          return "";
      }(config.credentials)),
      base_url_(config.base_url.value_or(DEFAULT_BASE_URL)) {}

AccountStatus DeepSeekProvider::query(HttpClient& client) const {
    std::string url = base_url_;
    if (!url.empty() && url.back() == '/') url.pop_back();
    url += "/user/balance";

    std::vector<std::pair<std::string, std::string>> headers = {
        {"Authorization", "Bearer " + api_key_},
    };

    JsonDocument doc = client.request_json("GET", url, headers, "", "deepseek");
    std::vector<QuotaEntry> entries;

    JsonArrayConst infos = doc["balance_infos"];
    for (JsonObjectConst info : infos) {
        auto total = json_number(info["total_balance"]);
        const char* currency = info["currency"] | "CNY";
        if (total && *total > 0.0) {
            entries.emplace_back(QuotaEntry(currency, currency).with_total(*total).with_remaining(*total));
        }
    }

    return AccountStatus(Vendor::DeepSeek, name_, QueryMode::Balance, std::move(entries));
}

} // namespace maiq
