#include "storage.hpp"

#include <esp_littlefs.h>
#include <esp_log.h>
#include <nvs_flash.h>

#include <fstream>
#include <sstream>

namespace hw {

static const char* TAG = "storage";
static const char* CONFIG_PATH = "/littlefs/config.json";

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
        .format_if_mount_failed = true,
        .dont_mount = false,
    };
    ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LittleFS register failed: %s", esp_err_to_name(ret));
        return false;
    }
    return true;
}

maiq::Config storage_load_config() {
    std::ifstream f(CONFIG_PATH);
    if (!f) {
        ESP_LOGW(TAG, "config file not found: %s", CONFIG_PATH);
        return maiq::Config{};
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    try {
        return maiq::Config::from_json_string(ss.str());
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "config parse failed: %s", e.what());
        return maiq::Config{};
    }
}

bool storage_save_config(const maiq::Config& config) {
    std::string s = config.to_json_string();
    std::ofstream f(CONFIG_PATH);
    if (!f) {
        ESP_LOGE(TAG, "cannot open config file for writing");
        return false;
    }
    f << s;
    return f.good();
}

} // namespace hw
