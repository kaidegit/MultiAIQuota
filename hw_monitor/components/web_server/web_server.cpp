#include "web_server.hpp"

#include <ArduinoJson.h>
#include <esp_http_server.h>
#include <esp_log.h>

#include "maiq/config.hpp"
#include "maiq/query.hpp"
#include "http_client_esp.hpp"
#include "storage.hpp"
#include "wifi.hpp"

#include <cstring>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace hw {

namespace {

static const char* TAG = "web_server";
static const char* LITTLEFS_ROOT = "/littlefs";

static std::optional<std::string> read_req_body(httpd_req_t* req, size_t max_len = 8192) {
    std::string body;
    body.resize(max_len);
    int ret = httpd_req_recv(req, body.data(), max_len - 1);
    if (ret <= 0) return std::nullopt;
    body.resize(static_cast<size_t>(ret));
    return body;
}

static void send_json(httpd_req_t* req, const std::string& json, int status = 200) {
    httpd_resp_set_status(req, status == 200 ? "200 OK" : "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json.c_str());
}

static void send_json_ok(httpd_req_t* req) {
    send_json(req, R"({"success":true})");
}

static void send_json_error(httpd_req_t* req, const char* msg, int status = 400) {
    httpd_resp_set_status(req, status == 400 ? "400 Bad Request" : "500 Internal Server Error");
    httpd_resp_set_type(req, "application/json");
    char buf[256];
    std::snprintf(buf, sizeof(buf), R"({"success":false,"error":"%s"})", msg);
    httpd_resp_sendstr(req, buf);
}

static const char* mime_type(const std::string& path) {
    if (path.ends_with(".html")) return "text/html";
    if (path.ends_with(".js")) return "application/javascript";
    if (path.ends_with(".mjs")) return "application/javascript";
    if (path.ends_with(".css")) return "text/css";
    if (path.ends_with(".json")) return "application/json";
    if (path.ends_with(".png")) return "image/png";
    if (path.ends_with(".svg")) return "image/svg+xml";
    if (path.ends_with(".ico")) return "image/x-icon";
    if (path.ends_with(".woff2")) return "font/woff2";
    return "application/octet-stream";
}

static std::string uri_to_path(const std::string& uri) {
    if (uri == "/") return std::string(LITTLEFS_ROOT) + "/index.html";
    return std::string(LITTLEFS_ROOT) + uri;
}

static esp_err_t static_file_handler(httpd_req_t* req) {
    std::string path = uri_to_path(req->uri);
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        // SPA fallback: return index.html for any non-API route.
        path = std::string(LITTLEFS_ROOT) + "/index.html";
        f.open(path, std::ios::binary);
        if (!f) {
            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not found");
            return ESP_FAIL;
        }
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    std::string data = ss.str();

    httpd_resp_set_type(req, mime_type(path));
    httpd_resp_send(req, data.data(), data.size());
    return ESP_OK;
}

static esp_err_t config_get_handler(httpd_req_t* req) {
    auto cfg = storage_load_config();
    send_json(req, cfg.to_json_string());
    return ESP_OK;
}

static esp_err_t config_post_handler(httpd_req_t* req) {
    auto body = read_req_body(req);
    if (!body) {
        send_json_error(req, "empty body");
        return ESP_OK;
    }
    try {
        auto cfg = maiq::Config::from_json_string(*body);
        if (storage_save_config(cfg)) {
            send_json_ok(req);
        } else {
            send_json_error(req, "save failed", 500);
        }
    } catch (const std::exception& e) {
        send_json_error(req, e.what());
    }
    return ESP_OK;
}

static esp_err_t wifi_status_handler(httpd_req_t* req) {
    JsonDocument doc;
    doc["connected"] = (wifi_state() == hw::WifiState::Connected);
    doc["state"] = wifi_state_string();
    doc["ssid"] = wifi_ssid();
    doc["ip"] = wifi_ip();
    std::string s;
    serializeJson(doc, s);
    send_json(req, s);
    return ESP_OK;
}

static esp_err_t wifi_connect_handler(httpd_req_t* req) {
    auto body = read_req_body(req);
    if (!body) {
        send_json_error(req, "empty body");
        return ESP_OK;
    }
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, *body);
    if (err || !doc["ssid"].is<const char*>() || !doc["password"].is<const char*>()) {
        send_json_error(req, "invalid json: need ssid and password");
        return ESP_OK;
    }
    std::string ssid = doc["ssid"].as<const char*>();
    std::string pass = doc["password"].as<const char*>();

    bool ok = wifi_connect(ssid, pass);

    JsonDocument resp;
    resp["success"] = ok;
    resp["state"] = wifi_state_string();
    resp["ssid"] = wifi_ssid();
    resp["ip"] = wifi_ip();
    resp["message"] = ok ? "connected" : "connection failed";
    std::string s;
    serializeJson(resp, s);
    send_json(req, s, ok ? 200 : 400);
    return ESP_OK;
}

static esp_err_t wifi_clear_handler(httpd_req_t* req) {
    wifi_clear_credentials();
    send_json_ok(req);
    return ESP_OK;
}

static std::string statuses_to_json(const std::vector<maiq::AccountStatus>& statuses) {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (const auto& st : statuses) {
        JsonObject o = arr.add<JsonObject>();
        o["name"] = st.account_name;
        o["vendor"] = maiq::to_string(st.vendor);
        o["mode"] = maiq::to_string(st.mode);
        o["valid"] = st.is_valid;
        if (st.invalid_message.has_value()) o["error"] = *st.invalid_message;
        JsonArray items = o["items"].to<JsonArray>();
        for (const auto& it : st.entries) {
            JsonObject io = items.add<JsonObject>();
            io["name"] = it.name;
            if (it.used.has_value()) io["used"] = *it.used;
            if (it.remaining.has_value()) io["remaining"] = *it.remaining;
            if (it.total.has_value()) io["total"] = *it.total;
            io["unit"] = it.unit;
            if (it.reset_at.has_value()) io["reset_at"] = static_cast<long>(*it.reset_at);
        }
    }
    std::string s;
    serializeJson(doc, s);
    return s;
}

static esp_err_t query_handler(httpd_req_t* req) {
    auto cfg = storage_load_config();
    if (cfg.accounts.empty()) {
        send_json_error(req, "no accounts configured");
        return ESP_OK;
    }
    try {
        auto client = maiq::create_http_client_esp();
        auto statuses = maiq::query_all_statuses(*client, cfg.accounts);
        send_json(req, statuses_to_json(statuses));
    } catch (const std::exception& e) {
        send_json_error(req, e.what(), 500);
    }
    return ESP_OK;
}

static esp_err_t health_handler(httpd_req_t* req) {
    send_json(req, R"({"status":"ok"})");
    return ESP_OK;
}

} // namespace

void web_server_start() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192; // query endpoint needs extra stack
    config.uri_match_fn = httpd_uri_match_wildcard;

    httpd_handle_t server = nullptr;
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "failed to start server");
        return;
    }

    httpd_uri_t routes[] = {
        {"/api/health",        HTTP_GET,  health_handler,        nullptr},
        {"/api/config",        HTTP_GET,  config_get_handler,    nullptr},
        {"/api/config",        HTTP_POST, config_post_handler,   nullptr},
        {"/api/wifi/status",   HTTP_GET,  wifi_status_handler,   nullptr},
        {"/api/wifi/connect",  HTTP_POST, wifi_connect_handler,  nullptr},
        {"/api/wifi/clear",    HTTP_POST, wifi_clear_handler,    nullptr},
        {"/api/query",         HTTP_POST, query_handler,         nullptr},
        {"/*",                 HTTP_GET,  static_file_handler,   nullptr},
    };

    for (const auto& uri : routes) {
        httpd_register_uri_handler(server, &uri);
    }

    ESP_LOGI(TAG, "web server started");
}

} // namespace hw
