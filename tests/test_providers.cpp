#include "maiq/providers/codex.hpp"
#include "maiq/providers/kimi.hpp"
#include "maiq/config.hpp"
#include "maiq/http_client.hpp"
#include "maiq/query.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace maiq {

class MockHttpClient : public HttpClient {
public:
    MockHttpClient(std::string response_body, int status = 200)
        : response_{status, std::move(response_body)} {}

    Response request(const std::string&, const std::string&,
                     const std::vector<std::pair<std::string, std::string>>&, const std::string&) override {
        return response_;
    }

private:
    Response response_;
};

// Serves different responses depending on a URL substring, so a single
// provider query that makes several requests (e.g. DeepSeek balance + usage)
// can be tested with realistic per-endpoint bodies.
class MockHttpClientByUrl : public HttpClient {
public:
    void add(std::string url_needle, std::string body, int status = 200) {
        routes_.push_back(Route{std::move(url_needle), status, std::move(body)});
    }

    Response request(const std::string&, const std::string& url,
                     const std::vector<std::pair<std::string, std::string>>&, const std::string&) override {
        for (const auto& r : routes_) {
            if (url.find(r.needle) != std::string::npos) return {r.status, r.body};
        }
        return {404, "no route matched"};
    }

private:
    struct Route {
        std::string needle;
        int status;
        std::string body;
    };
    std::vector<Route> routes_;
};

} // namespace maiq

int main() {
    using namespace maiq;

    // Kimi coding plan parse test
    {
        ProviderConfig cfg;
        cfg.name = "test-kimi";
        cfg.vendor = Vendor::Kimi;
        cfg.mode = QueryMode::CodingPlan;
        cfg.credentials = BearerCredentials{"sk-test"};
        cfg.timeout_secs = 10;

        const char* json = R"({
            "usage": {"limit": "100", "used": "48", "remaining": "52", "resetTime": "2026-06-30T04:00:00Z"},
            "limits": [{"window": {"timeUnit": "minute"}, "detail": {"limit": "100", "used": "7", "remaining": "93"}}]
        })";

        MockHttpClient client(json);
        auto status = query_one(client, cfg);
        assert(status.is_valid);
        assert(status.entries.size() == 2);
        assert(status.entries[0].name == "weekly");
        assert(status.entries[0].remaining == 52.0);
        assert(status.entries[1].name == "minute-window");
    }

    // Kimi coding plan parse test when exhausted usage omits 'remaining'
    {
        ProviderConfig cfg;
        cfg.name = "test-kimi-exhausted";
        cfg.vendor = Vendor::Kimi;
        cfg.mode = QueryMode::CodingPlan;
        cfg.credentials = BearerCredentials{"sk-test"};
        cfg.timeout_secs = 10;

        const char* json = R"({
            "usage": {"limit": "100", "used": "100", "resetTime": "2026-07-19T03:25:06.127488Z"},
            "limits": [{"window": {"duration": 300, "timeUnit": "TIME_UNIT_MINUTE"}, "detail": {"limit": "100", "remaining": "100", "resetTime": "2026-07-19T00:25:06.127488Z"}}]
        })";

        MockHttpClient client(json);
        auto status = query_one(client, cfg);
        assert(status.is_valid);
        assert(status.entries.size() == 2);
        assert(status.entries[0].name == "weekly");
        assert(status.entries[0].total == 100.0);
        assert(status.entries[0].used == 100.0);
        assert(status.entries[0].remaining == 0.0);
        assert(status.entries[1].name == "TIME_UNIT_MINUTE-window");
        assert(status.entries[1].remaining == 100.0);
    }

    // Kimi coding plan parse test without explicit 'used' field
    {
        ProviderConfig cfg;
        cfg.name = "test-kimi-no-used";
        cfg.vendor = Vendor::Kimi;
        cfg.mode = QueryMode::CodingPlan;
        cfg.credentials = BearerCredentials{"sk-test"};
        cfg.timeout_secs = 10;

        const char* json = R"({
            "usage": {"limit": "100", "remaining": "100", "resetTime": "2026-07-12T03:25:06.127488Z"},
            "limits": [{"window": {"duration": 300, "timeUnit": "TIME_UNIT_MINUTE"}, "detail": {"limit": "100", "remaining": "100", "resetTime": "2026-07-05T11:25:06.127488Z"}}]
        })";

        MockHttpClient client(json);
        auto status = query_one(client, cfg);
        assert(status.is_valid);
        assert(status.entries.size() == 2);
        assert(status.entries[0].name == "weekly");
        assert(status.entries[0].total == 100.0);
        assert(status.entries[0].remaining == 100.0);
        assert(status.entries[0].used == 0.0);
        assert(status.entries[1].name == "TIME_UNIT_MINUTE-window");
        assert(status.entries[1].used == 0.0);
    }

    // Kimi balance parse test
    {
        ProviderConfig cfg;
        cfg.name = "test-kimi-balance";
        cfg.vendor = Vendor::Kimi;
        cfg.mode = QueryMode::Balance;
        cfg.credentials = BearerCredentials{"sk-test"};

        const char* json = R"({"data": {"available_balance": 49.59, "voucher_balance": 46.59, "cash_balance": 3.00}})";

        MockHttpClient client(json);
        auto status = query_one(client, cfg);
        assert(status.is_valid);
        assert(status.entries.size() == 3);
        assert(std::abs(status.entries[0].remaining.value() - 49.59) < 1e-6);
    }

    // Codex usage parse test
    {
        ProviderConfig cfg;
        cfg.name = "test-codex";
        cfg.vendor = Vendor::Codex;
        cfg.mode = QueryMode::CodingPlan;
        cfg.credentials = CodexOAuthCredentials{"access-token", "account-1"};

        const char* json = R"({
            "plan_type": "pro",
            "rate_limit": {
                "allowed": true,
                "limit_reached": false,
                "primary_window": {"used_percent": 12, "limit_window_seconds": 18000, "reset_after_seconds": 180, "reset_at": 1751548800},
                "secondary_window": {"used_percent": 5, "limit_window_seconds": 604800, "reset_after_seconds": 0, "reset_at": 1751552400}
            },
            "credits": {"has_credits": true, "unlimited": false, "balance": "42"},
            "spend_control": {
                "reached": false,
                "individual_limit": {"limit": "25000", "used": "1234", "remaining": "23766", "used_percent": 5, "remaining_percent": 95, "reset_after_seconds": 0, "reset_at": 1751552400}
            },
            "additional_rate_limits": [
                {"limit_name": "codex-other", "metered_feature": "codex_other", "rate_limit": {"allowed": true, "limit_reached": false, "primary_window": {"used_percent": 30, "limit_window_seconds": 900, "reset_after_seconds": 0, "reset_at": 1751550600}}}
            ]
        })";

        MockHttpClient client(json);
        auto status = query_one(client, cfg);
        assert(status.is_valid);
        assert(status.entries.size() == 6);
        assert(status.entries[0].name == "plan-pro");
        assert(status.entries[1].name == "5h");
        assert(status.entries[1].used == 12.0);
        assert(status.entries[1].remaining == 88.0);
        assert(status.entries[1].reset_at == static_cast<time_t>(1751548800));
        assert(status.entries[2].name == "weekly");
        assert(status.entries[3].name == "credits");
        assert(status.entries[3].remaining == 42.0);
        assert(status.entries[4].name == "monthly-limit");
        assert(status.entries[4].used == 1234.0);
        assert(status.entries[4].remaining == 23766.0);
        assert(status.entries[4].total == 25000.0);
        assert(status.entries[5].name == "codex-other");
        assert(status.entries[5].used == 30.0);
    }

    // Codex unlimited credits parse test
    {
        ProviderConfig cfg;
        cfg.name = "test-codex-unlimited";
        cfg.vendor = Vendor::Codex;
        cfg.mode = QueryMode::CodingPlan;
        cfg.credentials = CodexOAuthCredentials{"access-token", "account-1"};

        const char* json = R"({
            "plan_type": "plus",
            "rate_limit": {"allowed": true, "limit_reached": false},
            "credits": {"has_credits": true, "unlimited": true}
        })";

        MockHttpClient client(json);
        auto status = query_one(client, cfg);
        assert(status.is_valid);
        assert(status.entries.size() == 2);
        assert(status.entries[0].name == "plan-plus");
        assert(status.entries[1].name == "credits-unlimited");
    }

    // DeepSeek balance + daily usage (web token) parse test
    {
        ProviderConfig cfg;
        cfg.name = "test-deepseek-usage";
        cfg.vendor = Vendor::DeepSeek;
        cfg.mode = QueryMode::Balance;
        BearerCredentials creds;
        creds.api_key = "sk-test";
        creds.web_token = "web-token-test";
        cfg.credentials = std::move(creds);

        std::time_t now = std::time(nullptr);
        std::tm utc{};
        gmtime_r(&now, &utc);
        char today[16];
        std::snprintf(today, sizeof(today), "%04d-%02d-%02d",
                      utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday);

        // Usage response: array of daily cost records, one of which is today.
        std::string usage_json = std::string(R"({"biz_data": [{"date": ")") + today +
                                R"(", "amount": "0.96"}]})";
        const char* balance_json = R"({
            "is_available": true,
            "balance_infos": [{"currency": "CNY", "total_balance": "42.02",
                               "granted_balance": "0.00", "topped_up_balance": "42.02"}]
        })";

        MockHttpClientByUrl client;
        client.add("/user/balance", balance_json);
        client.add("/api/v0/usage/cost", usage_json);

        auto status = query_one(client, cfg);
        assert(status.is_valid);
        assert(status.entries.size() == 2);
        assert(status.entries[0].name == "today-cost");
        assert(std::abs(status.entries[0].used.value() - 0.96) < 1e-6);
        assert(status.entries[1].name == "CNY");
        assert(std::abs(status.entries[1].remaining.value() - 42.02) < 1e-6);
    }

    // DeepSeek usage with days keyed by date string
    {
        ProviderConfig cfg;
        cfg.name = "test-deepseek-usage-keyed";
        cfg.vendor = Vendor::DeepSeek;
        cfg.mode = QueryMode::Balance;
        BearerCredentials creds;
        creds.api_key = "sk-test";
        creds.web_token = "web-token-test";
        cfg.credentials = std::move(creds);

        std::time_t now = std::time(nullptr);
        std::tm utc{};
        gmtime_r(&now, &utc);
        char today[16];
        std::snprintf(today, sizeof(today), "%04d-%02d-%02d",
                      utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday);

        std::string usage_json = std::string(R"({"biz_data": {"days": {")") + today +
                                R"(": {"total": "1.50"}}}})";
        const char* balance_json = R"({"is_available": true, "balance_infos": []})";

        MockHttpClientByUrl client;
        client.add("/user/balance", balance_json);
        client.add("/api/v0/usage/cost", usage_json);

        auto status = query_one(client, cfg);
        assert(status.is_valid);
        assert(status.entries.size() == 1);
        assert(status.entries[0].name == "today-cost");
        assert(std::abs(status.entries[0].used.value() - 1.50) < 1e-6);
    }

    // DeepSeek usage failure is non-fatal: account stays valid, today-cost
    // slot is left empty ("-") while the balance is still reported
    {
        ProviderConfig cfg;
        cfg.name = "test-deepseek-usage-fail";
        cfg.vendor = Vendor::DeepSeek;
        cfg.mode = QueryMode::Balance;
        BearerCredentials creds;
        creds.api_key = "sk-test";
        creds.web_token = "expired-token";
        cfg.credentials = std::move(creds);

        const char* balance_json = R"({"is_available": true, "balance_infos": [{"currency": "CNY", "total_balance": "42.02"}]})";

        MockHttpClientByUrl client;
        client.add("/user/balance", balance_json);
        client.add("/api/v0/usage/cost", "", 401);

        auto status = query_one(client, cfg);
        assert(status.is_valid);
        assert(status.entries.size() == 2);
        assert(status.entries[0].name == "today-cost");
        assert(!status.entries[0].used.has_value());  // displayed as "-"
        assert(status.entries[1].name == "CNY");
        assert(std::abs(status.entries[1].remaining.value() - 42.02) < 1e-6);
    }

    // DeepSeek without web token: balance reported, today-cost slot empty
    {
        ProviderConfig cfg;
        cfg.name = "test-deepseek-balance-only";
        cfg.vendor = Vendor::DeepSeek;
        cfg.mode = QueryMode::Balance;
        BearerCredentials creds;
        creds.api_key = "sk-test";
        cfg.credentials = std::move(creds);

        const char* balance_json = R"({"is_available": true, "balance_infos": [{"currency": "CNY", "total_balance": "8.88"}]})";

        MockHttpClientByUrl client;
        client.add("/user/balance", balance_json);

        auto status = query_one(client, cfg);
        assert(status.is_valid);
        assert(status.entries.size() == 2);
        assert(status.entries[0].name == "today-cost");
        assert(!status.entries[0].used.has_value());
        assert(status.entries[1].name == "CNY");
        assert(std::abs(status.entries[1].total.value() - 8.88) < 1e-6);
    }

    std::cout << "All tests passed.\n";
    return 0;
}
