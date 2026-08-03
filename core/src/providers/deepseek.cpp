#include "maiq/providers/deepseek.hpp"

#include "maiq/error.hpp"
#include "maiq/json.hpp"

#include <cstdio>
#include <ctime>
#include <sstream>
#include <vector>

namespace maiq {

namespace {

constexpr const char* DEFAULT_BASE_URL = "https://api.deepseek.com";
constexpr const char* USAGE_BASE_URL = "https://platform.deepseek.com";

std::string truncate_body(const std::string& body, size_t max_len = 400) {
    if (body.size() <= max_len) return body;
    return body.substr(0, max_len) + "...(truncated)";
}

// Whether a JSON leaf key carries a cost (money) value. The cost endpoint may
// mix in token counters, so exclude those explicitly.
bool is_cost_key(const std::string& k) {
    if (k.find("token") != std::string::npos) return false;
    if (k.find("request") != std::string::npos) return false;
    if (k.find("count") != std::string::npos) return false;
    if (k == "code" || k == "msg" || k == "message") return false;
    if (k == "date" || k == "day" || k == "month" || k == "year" || k == "model") return false;
    return k.find("cost") != std::string::npos ||
           k.find("amount") != std::string::npos ||
           k.find("price") != std::string::npos ||
           k.find("fee") != std::string::npos ||
           k.find("cny") != std::string::npos ||
           k.find("total") != std::string::npos;
}

double sum_costish(JsonVariantConst v) {
    double sum = 0.0;
    if (v.is<JsonObjectConst>()) {
        for (JsonPairConst member : v.as<JsonObjectConst>()) {
            JsonVariantConst val = member.value();
            if (val.is<JsonObjectConst>() || val.is<JsonArrayConst>()) {
                sum += sum_costish(val);
            } else if (is_cost_key(std::string(member.key().c_str()))) {
                if (auto n = json_number(val)) sum += *n;
            }
        }
    } else if (v.is<JsonArrayConst>()) {
        for (JsonVariantConst item : v.as<JsonArrayConst>()) {
            sum += sum_costish(item);
        }
    }
    return sum;
}

// Collect every record that belongs to `today`, from a response that may nest
// the daily data in several plausible shapes (array of days, days keyed by
// date, or a flat list of daily records).
void collect_day_records(JsonVariantConst v, const std::string& today,
                         std::vector<JsonVariantConst>& out) {
    if (v.is<JsonObjectConst>()) {
        JsonObjectConst obj = v.as<JsonObjectConst>();
        for (const char* key : {"date", "day", "biz_date", "stat_date"}) {
            if (obj[key].is<const char*>() && today == obj[key].as<const char*>()) {
                out.push_back(v);
                return;
            }
        }
        // Days keyed by date string, e.g. {"days": {"2026-08-03": {...}}}.
        JsonVariantConst days = obj["days"];
        if (days.is<JsonObjectConst>()) {
            JsonObjectConst days_obj = days.as<JsonObjectConst>();
            if (days_obj[today.c_str()].is<JsonObjectConst>()) {
                out.push_back(days_obj[today.c_str()]);
            }
        }
        for (JsonPairConst member : obj) {
            JsonVariantConst val = member.value();
            if (val.is<JsonArrayConst>() || val.is<JsonObjectConst>()) {
                collect_day_records(val, today, out);
            }
        }
    } else if (v.is<JsonArrayConst>()) {
        for (JsonVariantConst item : v.as<JsonArrayConst>()) {
            collect_day_records(item, today, out);
        }
    }
}

// Best-effort extraction of today's total cost from the usage cost response.
std::optional<double> parse_today_cost(JsonVariantConst root, const std::string& today) {
    JsonVariantConst biz = root["biz_data"];
    if (biz.isNull()) biz = root;
    std::vector<JsonVariantConst> records;
    collect_day_records(biz, today, records);
    if (records.empty()) return std::nullopt;
    double sum = 0.0;
    for (const auto& r : records) sum += sum_costish(r);
    return sum;
}

} // namespace

DeepSeekProvider::DeepSeekProvider(const ProviderConfig& config)
    : name_(config.name),
      api_key_([](const Credentials& c) -> std::string {
          if (std::holds_alternative<BearerCredentials>(c)) {
              return std::get<BearerCredentials>(c).api_key;
          }
          throw Error(Error::Code::InvalidCredentials, "deepseek expects bearer api_key");
          return "";
      }(config.credentials)),
      web_token_([](const Credentials& c) -> std::optional<std::string> {
          if (std::holds_alternative<BearerCredentials>(c)) {
              return std::get<BearerCredentials>(c).web_token;
          }
          return std::nullopt;
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
    std::string currency = "CNY";

    JsonArrayConst infos = doc["balance_infos"];
    for (JsonObjectConst info : infos) {
        auto total = json_number(info["total_balance"]);
        currency = info["currency"] | "CNY";
        if (total && *total > 0.0) {
            entries.emplace_back(QuotaEntry(currency, currency).with_total(*total).with_remaining(*total));
        }
    }

    // Daily usage is best-effort: always expose a "today-cost" slot so the UI
    // can render "-", and populate it only when the optional usage query
    // succeeds. Failures (no web_token, expired token, API change) never
    // invalidate the account — the balance result stays usable.
    QuotaEntry today_entry("today-cost", currency);
    if (web_token_ && !web_token_->empty()) {
        try {
            if (auto today_cost = query_today_cost(client)) {
                today_entry.with_used(*today_cost);
            }
        } catch (const Error&) {
            // keep the slot empty -> displayed as "-"
        }
    }
    entries.insert(entries.begin(), std::move(today_entry));

    return AccountStatus(Vendor::DeepSeek, name_, QueryMode::Balance, std::move(entries));
}

std::optional<double> DeepSeekProvider::query_today_cost(HttpClient& client) const {
    std::time_t now = std::time(nullptr);
    std::tm utc{};
    gmtime_r(&now, &utc);
    const int year = utc.tm_year + 1900;
    const int month = utc.tm_mon + 1;

    const auto two_digits = [](int v) {
        std::string s = std::to_string(v);
        return s.size() < 2 ? "0" + s : s;
    };
    const std::string today = std::to_string(year) + "-" + two_digits(month) + "-" + two_digits(utc.tm_mday);
    const std::string path = "/api/v0/usage/cost?month=" + std::to_string(month) +
                             "&year=" + std::to_string(year);

    std::vector<std::pair<std::string, std::string>> headers = {
        {"Authorization", "Bearer " + *web_token_},
        {"Accept", "application/json"},
        {"User-Agent", "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0.0.0 Safari/537.36"},
        {"x-app-version", "1.0.0"},
    };

    HttpClient::Response resp = client.request("GET", std::string(USAGE_BASE_URL) + path, headers, "");
    if (resp.status < 200 || resp.status >= 300) {
        throw Error(Error::Code::Api,
                    "usage API error (status=" + std::to_string(resp.status) + "): " + truncate_body(resp.body));
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, resp.body);
    if (err) {
        throw Error(Error::Code::Json,
                    "usage JSON parse failed: " + std::string(err.c_str()) +
                        "; body=" + truncate_body(resp.body));
    }

    auto today_cost = parse_today_cost(doc, today);
    if (!today_cost) {
        throw Error(Error::Code::Api,
                    "no daily cost found in response (endpoint may have changed); body=" + truncate_body(resp.body));
    }
    return today_cost;
}

} // namespace maiq
