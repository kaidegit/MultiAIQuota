#include "maiq/providers/kimi.hpp"
#include "maiq/config.hpp"
#include "maiq/http_client.hpp"
#include "maiq/query.hpp"

#include <cassert>
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
            "limits": [{"window": {"timeUnit": "MINUTE"}, "detail": {"limit": "100", "used": "7", "remaining": "93"}}]
        })";

        MockHttpClient client(json);
        auto status = query_one(client, cfg);
        assert(status.is_valid);
        assert(status.entries.size() == 2);
        assert(status.entries[0].name == "weekly");
        assert(status.entries[0].remaining == 52.0);
        assert(status.entries[1].name == "minute-window");
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
        assert(status.entries[0].remaining == 49.59);
    }

    std::cout << "All tests passed.\n";
    return 0;
}
