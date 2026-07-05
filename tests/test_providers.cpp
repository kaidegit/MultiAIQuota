#include "maiq/providers/codex.hpp"
#include "maiq/providers/kimi.hpp"
#include "maiq/config.hpp"
#include "maiq/http_client.hpp"
#include "maiq/query.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

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

    std::cout << "All tests passed.\n";
    return 0;
}
