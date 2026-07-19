#include "maiq/providers/kimi.hpp"

#include "maiq/error.hpp"
#include "maiq/json.hpp"

#include <optional>
#include <vector>

namespace maiq {

namespace {

constexpr const char* CODING_BASE_URL = "https://api.kimi.com/coding/v1";
constexpr const char* PLATFORM_BASE_URL = "https://api.moonshot.cn/v1";

std::optional<QuotaEntry> parse_usage_object(const char* name, JsonObjectConst obj) {
    auto limit = json_number(obj["limit"]);
    auto used = json_number(obj["used"]);
    auto remaining = json_number(obj["remaining"]);
    if (!limit || (!used && !remaining)) return std::nullopt;

    QuotaEntry entry(name, "requests");
    entry.with_total(*limit);
    if (used) {
        entry.with_used(*used);
    } else {
        entry.with_used(*limit - *remaining);
    }
    if (remaining) {
        entry.with_remaining(*remaining);
    } else {
        entry.with_remaining(*limit - *used);
    }
    if (auto rt = json_reset_time(obj["resetTime"])) {
        entry.with_reset_at(*rt);
    }
    return entry;
}

} // namespace

KimiProvider::KimiProvider(const ProviderConfig& config)
    : name_(config.name), api_key_([](const Credentials& c) -> std::string {
          if (std::holds_alternative<BearerCredentials>(c)) {
              return std::get<BearerCredentials>(c).api_key;
          }
          throw Error(Error::Code::InvalidCredentials, "kimi expects bearer api_key");
          return "";
      }(config.credentials)),
      mode_(config.mode),
      base_url_(config.base_url.value_or(
          config.mode == QueryMode::Balance ? PLATFORM_BASE_URL : CODING_BASE_URL)) {}

AccountStatus KimiProvider::query(HttpClient& client) const {
    switch (mode_) {
        case QueryMode::CodingPlan:
            return query_coding_plan(client);
        case QueryMode::Balance:
            return query_balance(client);
        default:
            throw Error(Error::Code::UnsupportedMode, "kimi does not support token_plan");
    }
    return AccountStatus::invalid(Vendor::Kimi, name_, QueryMode::Balance, "unreachable");
}

AccountStatus KimiProvider::query_coding_plan(HttpClient& client) const {
    std::string url = base_url_;
    if (!url.empty() && url.back() == '/') url.pop_back();
    url += "/usages";

    std::vector<std::pair<std::string, std::string>> headers = {
        {"Authorization", "Bearer " + api_key_},
    };

    JsonDocument doc = client.request_json("GET", url, headers, "", "kimi");
    std::vector<QuotaEntry> entries;

    JsonObjectConst usage = doc["usage"];
    if (usage) {
        if (auto e = parse_usage_object("weekly", usage)) {
            entries.push_back(*e);
        }
    }

    JsonArrayConst limits = doc["limits"];
    for (JsonObjectConst limit : limits) {
        const char* time_unit = limit["window"]["timeUnit"] | "";
        std::string name = std::string(time_unit) + "-window";
        if (name == "-window") name = "window";
        JsonObjectConst detail = limit["detail"];
        if (detail) {
            if (auto e = parse_usage_object(name.c_str(), detail)) {
                entries.push_back(*e);
            }
        }
    }

    return AccountStatus(Vendor::Kimi, name_, QueryMode::CodingPlan, std::move(entries));
}

AccountStatus KimiProvider::query_balance(HttpClient& client) const {
    std::string url = base_url_;
    if (!url.empty() && url.back() == '/') url.pop_back();
    url += "/users/me/balance";

    std::vector<std::pair<std::string, std::string>> headers = {
        {"Authorization", "Bearer " + api_key_},
    };

    JsonDocument doc = client.request_json("GET", url, headers, "", "kimi");
    JsonObjectConst data = doc["data"];
    if (!data) {
        throw Error(Error::Code::MissingField, "kimi balance response missing 'data'");
    }

    std::vector<QuotaEntry> entries;
    auto available = json_number(data["available_balance"]);
    auto cash = json_number(data["cash_balance"]);
    auto voucher = json_number(data["voucher_balance"]);
    if (available) entries.emplace_back(QuotaEntry("available", "CNY").with_remaining(*available));
    if (cash) entries.emplace_back(QuotaEntry("cash", "CNY").with_remaining(*cash));
    if (voucher) entries.emplace_back(QuotaEntry("voucher", "CNY").with_remaining(*voucher));

    return AccountStatus(Vendor::Kimi, name_, QueryMode::Balance, std::move(entries));
}

} // namespace maiq
