#include "maiq/providers/codex.hpp"

#include "maiq/error.hpp"
#include "maiq/json.hpp"

#include <optional>
#include <vector>

namespace maiq {

namespace {

constexpr const char* DEFAULT_BASE_URL = "https://chatgpt.com/backend-api";

std::string window_label_from_seconds(int64_t seconds, const char* fallback) {
    const int64_t minutes = seconds / 60;
    const int64_t minutes_per_5_hours = 5 * 60;
    const int64_t minutes_per_day = 24 * 60;
    const int64_t minutes_per_week = 7 * minutes_per_day;
    const int64_t minutes_per_month = 30 * minutes_per_day;

    auto is_approx = [minutes](int64_t expected, double tolerance) {
        return static_cast<double>(minutes) >= expected * (1.0 - tolerance)
            && static_cast<double>(minutes) <= expected * (1.0 + tolerance);
    };

    if (is_approx(minutes_per_5_hours, 0.05)) return "5h";
    if (is_approx(minutes_per_day, 0.05)) return "daily";
    if (is_approx(minutes_per_week, 0.05)) return "weekly";
    if (is_approx(minutes_per_month, 0.05)) return "monthly";
    return fallback;
}

std::optional<QuotaEntry> parse_window_entry(const char* fallback_name, JsonObjectConst window) {
    if (!window) return std::nullopt;
    auto used_percent = json_number(window["used_percent"]);
    auto limit_window_seconds = json_number(window["limit_window_seconds"]);
    if (!used_percent) return std::nullopt;

    std::string name = fallback_name;
    if (limit_window_seconds) {
        name = window_label_from_seconds(static_cast<int64_t>(*limit_window_seconds), fallback_name);
    }

    QuotaEntry entry(name, "%");
    entry.with_used(*used_percent)
         .with_remaining(100.0 - *used_percent)
         .with_total(100.0);

    if (auto reset_at = json_number(window["reset_at"])) {
        entry.with_reset_at(static_cast<time_t>(*reset_at));
    }
    return entry;
}

std::optional<QuotaEntry> parse_rate_limit_entry(const char* name, JsonObjectConst rate_limit) {
    if (!rate_limit) return std::nullopt;
    auto window = rate_limit["primary_window"];
    if (!window) window = rate_limit["secondary_window"];
    return parse_window_entry(name, window);
}

} // namespace

CodexProvider::CodexProvider(const ProviderConfig& config)
    : name_(config.name),
      access_token_([](const Credentials& c) -> std::string {
          if (std::holds_alternative<CodexOAuthCredentials>(c)) {
              return std::get<CodexOAuthCredentials>(c).access_token;
          }
          throw Error(Error::Code::InvalidCredentials, "codex expects codex_oauth credentials");
          return "";
      }(config.credentials)),
      account_id_([](const Credentials& c) -> std::string {
          if (std::holds_alternative<CodexOAuthCredentials>(c)) {
              return std::get<CodexOAuthCredentials>(c).account_id.value_or("");
          }
          return "";
      }(config.credentials)),
      base_url_(config.base_url.value_or(DEFAULT_BASE_URL)),
      mode_(config.mode) {}

AccountStatus CodexProvider::query(HttpClient& client) const {
    if (mode_ != QueryMode::CodingPlan) {
        throw Error(Error::Code::UnsupportedMode, "codex only supports coding_plan");
    }

    if (access_token_.empty()) {
        throw Error(Error::Code::InvalidCredentials, "codex access_token is empty");
    }

    std::string url = base_url_;
    if (!url.empty() && url.back() == '/') url.pop_back();
    url += "/wham/usage";

    std::vector<std::pair<std::string, std::string>> headers = {
        {"Authorization", "Bearer " + access_token_},
        {"OAI-Product-Sku", "codex"},
    };
    if (!account_id_.empty()) {
        headers.emplace_back("ChatGPT-Account-Id", account_id_);
    }

    JsonDocument doc = client.request_json("GET", url, headers, "", "codex");
    std::vector<QuotaEntry> entries;

    const char* plan_type = doc["plan_type"] | "";
    if (*plan_type != '\0') {
        entries.emplace_back(QuotaEntry(std::string("plan-") + plan_type, ""));
    }

    JsonObjectConst rate_limit = doc["rate_limit"];
    if (rate_limit) {
        if (auto e = parse_window_entry("primary", rate_limit["primary_window"])) {
            entries.push_back(*e);
        }
        if (auto e = parse_window_entry("secondary", rate_limit["secondary_window"])) {
            entries.push_back(*e);
        }
    }

    JsonObjectConst credits = doc["credits"];
    if (credits) {
        bool has_credits = credits["has_credits"] | false;
        if (has_credits) {
            bool unlimited = credits["unlimited"] | false;
            if (unlimited) {
                entries.emplace_back(QuotaEntry("credits-unlimited", "credits"));
            } else {
                auto balance_num = json_number(credits["balance"]);
                QuotaEntry entry("credits", "credits");
                if (balance_num) {
                    entry.with_remaining(*balance_num);
                }
                entries.push_back(entry);
            }
        }
    }

    JsonObjectConst spend_control = doc["spend_control"];
    if (spend_control) {
        JsonObjectConst individual = spend_control["individual_limit"];
        if (individual) {
            auto limit = json_number(individual["limit"]);
            auto used = json_number(individual["used"]);
            auto remaining = json_number(individual["remaining"]);
            if (limit || used || remaining) {
                QuotaEntry entry("monthly-limit", "credits");
                if (used) entry.with_used(*used);
                if (remaining) entry.with_remaining(*remaining);
                if (limit) entry.with_total(*limit);
                if (auto reset_at = json_number(individual["reset_at"])) {
                    entry.with_reset_at(static_cast<time_t>(*reset_at));
                }
                entries.push_back(entry);
            }
        }
    }

    JsonArrayConst additional = doc["additional_rate_limits"];
    for (JsonObjectConst item : additional) {
        const char* limit_name = item["limit_name"] | "";
        const char* metered_feature = item["metered_feature"] | "";
        std::string name = limit_name;
        if (name.empty()) name = metered_feature;
        if (name.empty()) name = "additional";
        if (auto e = parse_rate_limit_entry(name.c_str(), item["rate_limit"])) {
            entries.push_back(*e);
        }
    }

    return AccountStatus(Vendor::Codex, name_, QueryMode::CodingPlan, std::move(entries));
}

} // namespace maiq
