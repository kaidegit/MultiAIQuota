#include "time_sync.hpp"

#include <esp_log.h>
#include <esp_netif_sntp.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <ctime>

namespace hw {

static const char* TAG = "time_sync";

bool time_sync() {
    // Use China Standard Time (UTC+8) for localtime conversions.
    setenv("TZ", "CST-8", 1);
    tzset();

    ESP_LOGI(TAG, "starting SNTP sync");
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("ntp.aliyun.com");
    config.sync_cb = [](struct timeval*) {
        ESP_LOGI(TAG, "time synchronized from SNTP");
    };
    esp_netif_sntp_init(&config);

    int retry = 0;
    constexpr int retry_count = 15;
    while (esp_netif_sntp_sync_wait(2000 / portTICK_PERIOD_MS) == ESP_ERR_TIMEOUT &&
           ++retry < retry_count) {
        ESP_LOGI(TAG, "waiting for time sync... (%d/%d)", retry, retry_count);
    }

    // The successful sync_wait() in the loop consumed the semaphore, so we
    // decide timeout by checking if we exited before exhausting retries.
    bool synced = (retry < retry_count);
    if (!synced) {
        ESP_LOGW(TAG, "time sync timed out");
    } else {
        time_t now = std::time(nullptr);
        struct tm tm_info;
        localtime_r(&now, &tm_info);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_info);
        ESP_LOGI(TAG, "current local time: %s", buf);
    }

    esp_netif_sntp_deinit();
    return synced;
}

} // namespace hw
