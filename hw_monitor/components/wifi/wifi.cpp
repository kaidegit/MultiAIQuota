#include "wifi.hpp"

#include <cstring>
#include <nvs_flash.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_smartconfig.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>

namespace hw {

static const char* TAG = "wifi";
static const char* NVS_NS = "wifi_cred";
static const char* KEY_SSID = "ssid";
static const char* KEY_PASS = "pass";

static EventGroupHandle_t s_event_group = nullptr;
static constexpr int CONNECTED_BIT = BIT0;
static constexpr int FAIL_BIT = BIT1;
static constexpr int SMARTCONFIG_GOT_CRED_BIT = BIT2;
static constexpr int SMARTCONFIG_DONE_BIT = BIT3;

static WifiState s_state = WifiState::Init;
static char s_ssid[33] = {};
static char s_password[65] = {};
static char s_ip[16] = {};
static bool s_credentials_loaded = false;
static bool s_smartconfig_started = false;
static bool s_initial_attempt = false;

static const char* state_to_string(WifiState state) {
    switch (state) {
        case WifiState::Init: return "init";
        case WifiState::Disconnected: return "disconnected";
        case WifiState::Connecting: return "connecting";
        case WifiState::Connected: return "connected";
        case WifiState::SmartConfig: return "smartconfig";
        case WifiState::Failed: return "failed";
    }
    return "unknown";
}

static void set_state(WifiState state) {
    s_state = state;
    ESP_LOGI(TAG, "state -> %s", state_to_string(state));
}

const char* wifi_state_string() {
    return state_to_string(s_state);
}

static bool nvs_save_credentials(const char* ssid, const char* pass) {
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(ret));
        return false;
    }
    ret = nvs_set_str(h, KEY_SSID, ssid);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_set_str(ssid) failed: %s", esp_err_to_name(ret));
        nvs_close(h);
        return false;
    }
    ret = nvs_set_str(h, KEY_PASS, pass);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_set_str(pass) failed: %s", esp_err_to_name(ret));
        nvs_close(h);
        return false;
    }
    ret = nvs_commit(h);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_commit failed: %s", esp_err_to_name(ret));
        nvs_close(h);
        return false;
    }
    nvs_close(h);
    return true;
}

static bool nvs_load_credentials(char* ssid, size_t ssid_len,
                                 char* pass, size_t pass_len) {
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return false;
    size_t l1 = ssid_len, l2 = pass_len;
    if (nvs_get_str(h, KEY_SSID, ssid, &l1) != ESP_OK ||
        nvs_get_str(h, KEY_PASS, pass, &l2) != ESP_OK) {
        nvs_close(h);
        return false;
    }
    nvs_close(h);
    return true;
}

static void nvs_clear_credentials() {
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_erase_key(h, KEY_SSID);
        nvs_erase_key(h, KEY_PASS);
        nvs_commit(h);
        nvs_close(h);
    }
}

static void apply_config_and_connect() {
    wifi_config_t cfg = {};
    // Use the full field size (32/64 bytes). For max-length credentials the
    // driver uses the whole field; shorter values are still null-terminated.
    std::strncpy(reinterpret_cast<char*>(cfg.sta.ssid), s_ssid, sizeof(cfg.sta.ssid));
    std::strncpy(reinterpret_cast<char*>(cfg.sta.password), s_password, sizeof(cfg.sta.password));
    cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    esp_err_t ret = esp_wifi_set_config(WIFI_IF_STA, &cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_config failed: %s", esp_err_to_name(ret));
    }
    set_state(WifiState::Connecting);
    ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_connect failed: %s", esp_err_to_name(ret));
    }
}

static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data) {
    (void)arg;
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        if (s_credentials_loaded) {
            apply_config_and_connect();
        }
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        auto* evt = static_cast<wifi_event_sta_disconnected_t*>(event_data);
        ESP_LOGW(TAG, "disconnected, reason=%d", evt->reason);
        set_state(WifiState::Disconnected);
        if (s_smartconfig_started) {
            // During SmartConfig the station may disconnect/scan repeatedly;
            // do not treat those as fatal failures.
        } else if (s_initial_attempt) {
            // Initial credential attempt failed; signal failure so the caller
            // can retry or fall back to SmartConfig. Do not auto-reconnect here.
            xEventGroupSetBits(s_event_group, FAIL_BIT);
        } else {
            // Runtime disconnect: try to reconnect.
            set_state(WifiState::Connecting);
            esp_wifi_connect();
        }
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        set_state(WifiState::Connecting);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        auto* event = static_cast<ip_event_got_ip_t*>(event_data);
        std::snprintf(s_ip, sizeof(s_ip), IPSTR, IP2STR(&event->ip_info.ip));
        set_state(WifiState::Connected);
        ESP_LOGI(TAG, "Wi-Fi connected, web server: http://%s/", s_ip);
        xEventGroupSetBits(s_event_group, CONNECTED_BIT);
    } else if (event_base == SC_EVENT) {
        if (event_id == SC_EVENT_GOT_SSID_PSWD) {
            auto* evt = static_cast<smartconfig_event_got_ssid_pswd_t*>(event_data);
            std::strncpy(s_ssid, reinterpret_cast<const char*>(evt->ssid), sizeof(s_ssid) - 1);
            s_ssid[sizeof(s_ssid) - 1] = '\0';
            std::strncpy(s_password, reinterpret_cast<const char*>(evt->password), sizeof(s_password) - 1);
            s_password[sizeof(s_password) - 1] = '\0';
            s_credentials_loaded = true;
            nvs_save_credentials(s_ssid, s_password);
            ESP_LOGI(TAG, "SmartConfig got SSID: %s", s_ssid);

            apply_config_and_connect();
            xEventGroupSetBits(s_event_group, SMARTCONFIG_GOT_CRED_BIT);
        } else if (event_id == SC_EVENT_SEND_ACK_DONE) {
            xEventGroupSetBits(s_event_group, SMARTCONFIG_DONE_BIT);
            esp_smartconfig_stop();
            s_smartconfig_started = false;
        }
    }
}

static bool wait_for_ip(TickType_t timeout_ticks) {
    EventBits_t bits = xEventGroupWaitBits(s_event_group,
                                           CONNECTED_BIT | FAIL_BIT,
                                           pdTRUE, pdFALSE, timeout_ticks);
    return (bits & CONNECTED_BIT) != 0;
}

static bool wait_for_connected(TickType_t timeout_ticks) {
    EventBits_t bits = xEventGroupWaitBits(s_event_group,
                                           CONNECTED_BIT,
                                           pdTRUE, pdFALSE, timeout_ticks);
    return (bits & CONNECTED_BIT) != 0;
}

static bool start_smartconfig(TickType_t timeout_ticks) {
    s_smartconfig_started = true;
    set_state(WifiState::SmartConfig);
    ESP_LOGI(TAG, "Starting SmartConfig (ESPTouch), timeout %lu s",
             static_cast<unsigned long>(timeout_ticks / configTICK_RATE_HZ));

    xEventGroupClearBits(s_event_group,
                         CONNECTED_BIT | FAIL_BIT | SMARTCONFIG_GOT_CRED_BIT | SMARTCONFIG_DONE_BIT);

    ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_ESPTOUCH));
    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_smartconfig_start(&cfg));

    bool ok = wait_for_connected(timeout_ticks);
    if (!ok && s_smartconfig_started) {
        esp_smartconfig_stop();
        s_smartconfig_started = false;
    }
    return ok;
}

bool wifi_ensure_connected() {
    if (s_state == WifiState::Connected) return true;

    s_event_group = xEventGroupCreate();
    if (!s_event_group) return false;

    // Load credentials before starting Wi-Fi so the STA_START handler can
    // initiate a single, well-timed connection attempt.
    bool have_stored = nvs_load_credentials(s_ssid, sizeof(s_ssid),
                                            s_password, sizeof(s_password));
    if (have_stored) {
        s_credentials_loaded = true;
        ESP_LOGI(TAG, "Loaded stored SSID: %s", s_ssid);
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        nullptr,
                                                        nullptr));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        nullptr,
                                                        nullptr));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(SC_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        nullptr,
                                                        nullptr));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_MIN_MODEM));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Try stored credentials first. STA_START will call apply_config_and_connect().
    if (s_credentials_loaded) {
        s_initial_attempt = true;
        constexpr int kMaxRetries = 2;
        for (int retry = 0; retry <= kMaxRetries; ++retry) {
            if (retry > 0) {
                ESP_LOGI(TAG, "retrying stored credentials (%d/%d)", retry, kMaxRetries);
                xEventGroupClearBits(s_event_group, CONNECTED_BIT | FAIL_BIT);
                apply_config_and_connect();
            }
            if (wait_for_ip(pdMS_TO_TICKS(15000))) {
                s_initial_attempt = false;
                return true;
            }
        }
        s_initial_attempt = false;
        ESP_LOGW(TAG, "Stored credentials failed, falling back to SmartConfig");
    }

    // Make sure any pending connect/disconnect is cleared before SmartConfig.
    esp_wifi_disconnect();
    return start_smartconfig(pdMS_TO_TICKS(120000));
}

bool wifi_connect(const std::string& ssid, const std::string& password) {
    std::strncpy(s_ssid, ssid.c_str(), sizeof(s_ssid) - 1);
    std::strncpy(s_password, password.c_str(), sizeof(s_password) - 1);
    s_credentials_loaded = true;
    nvs_save_credentials(s_ssid, s_password);

    if (s_state == WifiState::Init) {
        // Not initialized yet; let wifi_ensure_connected handle it.
        return wifi_ensure_connected();
    }

    esp_wifi_disconnect();
    apply_config_and_connect();
    return wait_for_ip(pdMS_TO_TICKS(30000));
}

void wifi_save_credentials(const std::string& ssid, const std::string& password) {
    nvs_save_credentials(ssid.c_str(), password.c_str());
}

bool wifi_load_credentials(std::string& ssid, std::string& password) {
    char ss[33] = {}, pw[65] = {};
    if (!nvs_load_credentials(ss, sizeof(ss), pw, sizeof(pw))) return false;
    ssid = ss;
    password = pw;
    return true;
}

void wifi_clear_credentials() {
    nvs_clear_credentials();
    s_credentials_loaded = false;
    s_ssid[0] = '\0';
    s_password[0] = '\0';
    s_ip[0] = '\0';
    if (s_state != WifiState::Init) {
        esp_wifi_disconnect();
    }
    set_state(WifiState::Disconnected);
}

WifiState wifi_state() {
    return s_state;
}

const char* wifi_ssid() {
    return s_ssid;
}

const char* wifi_ip() {
    return s_ip;
}

} // namespace hw
