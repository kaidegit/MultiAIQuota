#include "web_server.hpp"

#include <ArduinoJson.h>
#include <esp_http_server.h>
#include <esp_log.h>

#include <atomic>

#include "maiq/config.hpp"
#include "maiq/query.hpp"
#include "http_client_esp.hpp"
#include "storage.hpp"
#include "wifi.hpp"
#include "display.hpp"
#include "board.hpp"

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

static std::atomic<bool> g_config_saved{false};

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

static std::string config_to_masked_json(const maiq::Config& cfg) {
    JsonDocument doc;
    JsonArray arr = doc["accounts"].to<JsonArray>();
    for (const auto& acc : cfg.accounts) {
        JsonObject obj = arr.add<JsonObject>();
        obj["name"] = acc.name;
        obj["vendor"] = maiq::to_string(acc.vendor);
        obj["mode"] = maiq::to_string(acc.mode);
        obj["timeout_secs"] = acc.timeout_secs;
        if (acc.base_url) obj["base_url"] = *acc.base_url;

        std::visit([&](const auto& c) {
            using T = std::decay_t<decltype(c)>;
            if constexpr (std::is_same_v<T, maiq::BearerCredentials>) {
                obj["auth_type"] = "bearer";
                obj["api_key"] = maiq::mask_key(c.api_key);
            } else if constexpr (std::is_same_v<T, maiq::VolcengineCredentials>) {
                obj["auth_type"] = "volcengine";
                obj["ak"] = maiq::mask_key(c.ak);
                obj["sk"] = maiq::mask_key(c.sk);
                obj["region"] = c.region;
            } else if constexpr (std::is_same_v<T, maiq::NewApiCredentials>) {
                obj["auth_type"] = "newapi";
                obj["access_token"] = maiq::mask_key(c.access_token);
                if (c.user_id) obj["user_id"] = *c.user_id;
            } else if constexpr (std::is_same_v<T, maiq::CodexOAuthCredentials>) {
                obj["auth_type"] = "codex_oauth";
                obj["access_token"] = maiq::mask_key(c.access_token);
                if (c.account_id) obj["account_id"] = *c.account_id;
            }
        }, acc.credentials);
    }
    std::string s;
    serializeJson(doc, s);
    return s;
}

static void merge_config_secrets(maiq::Config& incoming, const maiq::Config& existing) {
    for (size_t i = 0; i < incoming.accounts.size() && i < existing.accounts.size(); ++i) {
        auto& in_acc = incoming.accounts[i];
        const auto& ex_acc = existing.accounts[i];
        if (in_acc.credentials.index() != ex_acc.credentials.index()) continue;

        std::visit([&](auto& in_c) {
            using T = std::decay_t<decltype(in_c)>;
            if (!std::holds_alternative<T>(ex_acc.credentials)) return;
            const auto& ex_c = std::get<T>(ex_acc.credentials);
            if constexpr (std::is_same_v<T, maiq::BearerCredentials>) {
                if (in_c.api_key.empty()) in_c.api_key = ex_c.api_key;
            } else if constexpr (std::is_same_v<T, maiq::VolcengineCredentials>) {
                if (in_c.ak.empty()) in_c.ak = ex_c.ak;
                if (in_c.sk.empty()) in_c.sk = ex_c.sk;
                if (in_c.region.empty()) in_c.region = ex_c.region;
            } else if constexpr (std::is_same_v<T, maiq::NewApiCredentials>) {
                if (in_c.access_token.empty()) in_c.access_token = ex_c.access_token;
                if (!in_c.user_id.has_value() && ex_c.user_id.has_value()) {
                    in_c.user_id = ex_c.user_id;
                }
            } else if constexpr (std::is_same_v<T, maiq::CodexOAuthCredentials>) {
                if (in_c.access_token.empty()) in_c.access_token = ex_c.access_token;
                if (!in_c.account_id.has_value() && ex_c.account_id.has_value()) {
                    in_c.account_id = ex_c.account_id;
                }
            }
        }, in_acc.credentials);
    }
}

static esp_err_t config_get_handler(httpd_req_t* req) {
    auto cfg = storage_load_config();
    send_json(req, config_to_masked_json(cfg));
    return ESP_OK;
}

static esp_err_t config_post_handler(httpd_req_t* req) {
    auto body = read_req_body(req);
    if (!body) {
        send_json_error(req, "empty body");
        return ESP_OK;
    }
    try {
        auto existing = storage_load_config();
        auto cfg = maiq::Config::from_json_string(*body);
        merge_config_secrets(cfg, existing);
        if (storage_save_config(cfg)) {
            g_config_saved.store(true);
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

static void bmp_header(uint8_t* out, uint32_t width, uint32_t height, uint32_t row_bytes) {
    const uint32_t image_size = row_bytes * height;
    const uint32_t file_size = 54 + image_size;
    std::memset(out, 0, 54);
    out[0] = 'B';
    out[1] = 'M';
    out[2] = static_cast<uint8_t>(file_size);
    out[3] = static_cast<uint8_t>(file_size >> 8);
    out[4] = static_cast<uint8_t>(file_size >> 16);
    out[5] = static_cast<uint8_t>(file_size >> 24);
    out[10] = 54;
    out[14] = 40;
    out[18] = static_cast<uint8_t>(width);
    out[19] = static_cast<uint8_t>(width >> 8);
    out[20] = static_cast<uint8_t>(width >> 16);
    out[21] = static_cast<uint8_t>(width >> 24);
    out[22] = static_cast<uint8_t>(height);
    out[23] = static_cast<uint8_t>(height >> 8);
    out[24] = static_cast<uint8_t>(height >> 16);
    out[25] = static_cast<uint8_t>(height >> 24);
    out[26] = 1;
    out[28] = 24;
    out[34] = static_cast<uint8_t>(image_size);
    out[35] = static_cast<uint8_t>(image_size >> 8);
    out[36] = static_cast<uint8_t>(image_size >> 16);
    out[37] = static_cast<uint8_t>(image_size >> 24);
}

static esp_err_t screenshot_handler(httpd_req_t* req) {
    constexpr uint32_t width = xueersi::LCD_WIDTH;
    constexpr uint32_t height = xueersi::LCD_HEIGHT;
    constexpr uint32_t row_bytes = width * 3;

    const uint16_t* buf = hw::display_screenshot_lock();

    uint8_t header[54];
    bmp_header(header, width, height, row_bytes);

    httpd_resp_set_status(req, "200 OK");
    httpd_resp_set_type(req, "image/bmp");

    esp_err_t err = httpd_resp_send_chunk(req, reinterpret_cast<const char*>(header), sizeof(header));

    uint8_t row[row_bytes];
    for (int y = static_cast<int>(height) - 1; err == ESP_OK && y >= 0; --y) {
        for (int x = 0; x < static_cast<int>(width); ++x) {
            const uint16_t swapped = buf[y * width + x];
            const uint16_t c = __builtin_bswap16(swapped);
            const uint8_t r5 = (c >> 11) & 0x1F;
            const uint8_t g6 = (c >> 5) & 0x3F;
            const uint8_t b5 = c & 0x1F;
            const uint8_t r = static_cast<uint8_t>((r5 << 3) | (r5 >> 2));
            const uint8_t g = static_cast<uint8_t>((g6 << 2) | (g6 >> 4));
            const uint8_t b = static_cast<uint8_t>((b5 << 3) | (b5 >> 2));
            row[x * 3 + 0] = b;
            row[x * 3 + 1] = g;
            row[x * 3 + 2] = r;
        }
        err = httpd_resp_send_chunk(req, reinterpret_cast<const char*>(row), row_bytes);
    }

    hw::display_screenshot_unlock();

    if (err == ESP_OK) {
        err = httpd_resp_send_chunk(req, nullptr, 0);
    }
    return err;
}

} // namespace

bool web_server_take_config_saved_flag() {
    return g_config_saved.exchange(false);
}

void web_server_start() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192; // query endpoint needs extra stack
    config.max_uri_handlers = 16; // we now have more than the default 8 handlers
    config.uri_match_fn = httpd_uri_match_wildcard;

    httpd_handle_t server = nullptr;
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "failed to start server");
        return;
    }

    httpd_uri_t routes[] = {
        {"/api/health",        HTTP_GET,  health_handler,        nullptr},
        {"/api/screenshot",    HTTP_GET,  screenshot_handler,    nullptr},
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
