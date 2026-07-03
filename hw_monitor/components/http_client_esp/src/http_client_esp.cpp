#include "http_client_esp.hpp"

#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_log.h>

namespace maiq {

namespace {

static const char* TAG = "http_client_esp";

static esp_err_t http_event_handler(esp_http_client_event_t* evt) {
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        auto* body = static_cast<std::string*>(evt->user_data);
        if (body && evt->data && evt->data_len > 0) {
            body->append(static_cast<const char*>(evt->data), evt->data_len);
        }
    }
    return ESP_OK;
}

class HttpClientEsp : public HttpClient {
public:
    using HttpClient::HttpClient;

    Response request(const std::string& method, const std::string& url,
                     const std::vector<std::pair<std::string, std::string>>& headers,
                     const std::string& body) override {
        std::string response_body;

        esp_http_client_config_t config = {};
        config.url = url.c_str();
        config.method = method == "POST" ? HTTP_METHOD_POST : HTTP_METHOD_GET;
        config.timeout_ms = static_cast<int>(timeout_ms_);
        config.crt_bundle_attach = esp_crt_bundle_attach;
        config.skip_cert_common_name_check = false;
        config.event_handler = http_event_handler;
        config.user_data = &response_body;

        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (!client) {
            throw Error(Error::Code::Http, "esp_http_client_init failed");
        }

        for (const auto& [k, v] : headers) {
            esp_http_client_set_header(client, k.c_str(), v.c_str());
        }

        if (!body.empty()) {
            esp_http_client_set_post_field(client, body.c_str(), static_cast<int>(body.size()));
        }

        esp_err_t err = esp_http_client_perform(client);
        if (err != ESP_OK) {
            esp_http_client_cleanup(client);
            throw Error(Error::Code::Http, std::string("HTTP request failed: ") + esp_err_to_name(err));
        }

        int status = esp_http_client_get_status_code(client);
        int64_t content_len = esp_http_client_get_content_length(client);
        ESP_LOGI(TAG, "%s %s -> status=%d, content_length=%lld, received=%zu",
                 method.c_str(), url.c_str(), status, content_len, response_body.size());
        if (!response_body.empty()) {
            ESP_LOGI(TAG, "response: %.*s", static_cast<int>(response_body.size()), response_body.data());
        }

        esp_http_client_cleanup(client);
        return {status, response_body};
    }
};

} // namespace

std::unique_ptr<HttpClient> create_http_client_esp(uint32_t timeout_ms) {
    return std::make_unique<HttpClientEsp>(timeout_ms);
}

} // namespace maiq
