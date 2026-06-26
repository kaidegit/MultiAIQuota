#include "maiq/http_client.hpp"

namespace maiq {

JsonDocument HttpClient::request_json(const std::string& method, const std::string& url,
                                      const std::vector<std::pair<std::string, std::string>>& headers,
                                      const std::string& body,
                                      const std::string& vendor_label) {
    Response resp = request(method, url, headers, body);
    if (resp.status < 200 || resp.status >= 300) {
        throw Error(Error::Code::Api,
                    vendor_label + " API error (status=" + std::to_string(resp.status) + "): " + resp.body);
    }
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, resp.body);
    if (err) {
        throw Error(Error::Code::Json,
                    vendor_label + " JSON parse failed: " + std::string(err.c_str()));
    }
    return doc;
}

} // namespace maiq
