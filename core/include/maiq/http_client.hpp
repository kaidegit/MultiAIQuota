#pragma once

#include "error.hpp"
#include "json.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace maiq {

// Cross-platform synchronous HTTP client abstraction.
// PC implements this with cpp-httplib; ESP32 implements with esp_http_client.
class HttpClient {
public:
    struct Response {
        int status = 0;
        std::string body;
    };

    explicit HttpClient(uint32_t timeout_ms = 30000) : timeout_ms_(timeout_ms) {}
    virtual ~HttpClient() = default;

    // Synchronous blocking request. Must honor timeout_ms().
    virtual Response request(const std::string& method, const std::string& url,
                             const std::vector<std::pair<std::string, std::string>>& headers,
                             const std::string& body) = 0;

    JsonDocument request_json(const std::string& method, const std::string& url,
                              const std::vector<std::pair<std::string, std::string>>& headers,
                              const std::string& body,
                              const std::string& vendor_label);

    uint32_t timeout_ms() const noexcept { return timeout_ms_; }
    void set_timeout_ms(uint32_t ms) noexcept { timeout_ms_ = ms; }

protected:
    uint32_t timeout_ms_;
};

} // namespace maiq
