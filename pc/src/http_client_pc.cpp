#include "maiq/platform/http_client_pc.hpp"

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.h>

namespace maiq {

namespace {

class HttpClientPc : public HttpClient {
public:
    using HttpClient::HttpClient;

    Response request(const std::string& method, const std::string& url,
                     const std::vector<std::pair<std::string, std::string>>& headers,
                     const std::string& body) override {
        // Parse URL into host and path.
        std::string scheme, host;
        int port = 0;
        std::string path;

        size_t scheme_end = url.find("://");
        size_t host_start = (scheme_end == std::string::npos) ? 0 : scheme_end + 3;
        scheme = (scheme_end == std::string::npos) ? "https" : url.substr(0, scheme_end);

        size_t path_start = url.find('/', host_start);
        if (path_start == std::string::npos) {
            host = url.substr(host_start);
            path = "/";
        } else {
            host = url.substr(host_start, path_start - host_start);
            path = url.substr(path_start);
        }

        // Split host:port
        std::string hostname = host;
        bool is_https = (scheme == "https");
        size_t colon = host.rfind(':');
        if (colon != std::string::npos) {
            // handle IPv6 bracket? simplified.
            try {
                port = std::stoi(host.substr(colon + 1));
                hostname = host.substr(0, colon);
            } catch (...) {
                // ignore, default port
            }
        }
        if (port == 0) port = is_https ? 443 : 80;

        std::string client_url = scheme + "://" + hostname + ":" + std::to_string(port);
        httplib::Client cli(client_url);
        if (!cli.is_valid()) {
            throw Error(Error::Code::Http, "failed to create http client for " + client_url);
        }
        cli.set_connection_timeout(timeout_ms_ / 1000, (timeout_ms_ % 1000) * 1000);
        cli.set_read_timeout(timeout_ms_ / 1000, (timeout_ms_ % 1000) * 1000);
        cli.set_write_timeout(timeout_ms_ / 1000, (timeout_ms_ % 1000) * 1000);
        cli.enable_server_certificate_verification(true);

        httplib::Headers hdrs;
        for (const auto& [k, v] : headers) hdrs.emplace(k, v);

        httplib::Result res;
        std::string upper_method = method;
        for (auto& c : upper_method) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

        if (upper_method == "GET") {
            res = cli.Get(path, hdrs);
        } else if (upper_method == "POST") {
            res = cli.Post(path, hdrs, body, "application/json");
        } else {
            throw Error(Error::Code::Other, "unsupported HTTP method: " + method);
        }

        if (!res) {
            auto err = res.error();
            throw Error(Error::Code::Http, httplib::to_string(err));
        }
        return {res->status, res->body};
    }
};

} // namespace

std::unique_ptr<HttpClient> create_http_client(uint32_t timeout_ms) {
    return std::make_unique<HttpClientPc>(timeout_ms);
}

} // namespace maiq
