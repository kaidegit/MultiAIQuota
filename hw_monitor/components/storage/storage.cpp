#include "storage.hpp"

#include <esp_littlefs.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <nvs.h>

namespace hw {

static const char* TAG = "storage";
static const char* NVS_NS = "maiq";
static const char* NVS_KEY_CONFIG = "config";

bool storage_init() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed: %s", esp_err_to_name(ret));
        return false;
    }

    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/littlefs",
        .partition_label = "littlefs",
        .partition = nullptr,
        .format_if_mount_failed = true,
        .read_only = false,
        .dont_mount = false,
        .grow_on_mount = false,
    };
    ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LittleFS register failed: %s", esp_err_to_name(ret));
        return false;
    }
    return true;
}

maiq::Config storage_load_config() {
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NS, NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "NVS open failed: %s", esp_err_to_name(ret));
        return maiq::Config{};
    }

    size_t len = 0;
    ret = nvs_get_blob(handle, NVS_KEY_CONFIG, nullptr, &len);
    if (ret != ESP_OK || len == 0) {
        nvs_close(handle);
        if (ret != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "NVS get config failed: %s", esp_err_to_name(ret));
        }
        return maiq::Config{};
    }

    std::string s(len, '\0');
    ret = nvs_get_blob(handle, NVS_KEY_CONFIG, s.data(), &len);
    nvs_close(handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS read config failed: %s", esp_err_to_name(ret));
        return maiq::Config{};
    }
    s.resize(len);

    try {
        return maiq::Config::from_json_string(s);
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "config parse failed: %s", e.what());
        return maiq::Config{};
    }
}

bool storage_save_config(const maiq::Config& config) {
    std::string s = config.to_json_string();
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NS, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS open failed: %s", esp_err_to_name(ret));
        return false;
    }

    ret = nvs_set_blob(handle, NVS_KEY_CONFIG, s.data(), s.size());
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS set blob failed: %s", esp_err_to_name(ret));
        nvs_close(handle);
        return false;
    }

    ret = nvs_commit(handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS commit failed: %s", esp_err_to_name(ret));
    }
    nvs_close(handle);
    return ret == ESP_OK;
}

} // namespace hw
