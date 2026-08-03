#pragma once

#include <string>

namespace hw {

// Human-readable Wi-Fi states.
enum class WifiState {
    Init,
    Disconnected,
    Connecting,
    Connected,
    SmartConfig,
    Failed,
};

// Initialize Wi-Fi and try to connect. If no credentials are stored,
// start SmartConfig and wait for the phone app to send SSID/password.
// Blocks until connected or timed out.
bool wifi_ensure_connected();

// Connect to the given network and persist credentials in NVS.
bool wifi_connect(const std::string& ssid, const std::string& password);

// Save credentials to NVS without connecting.
void wifi_save_credentials(const std::string& ssid, const std::string& password);

// Load credentials from NVS (output-only parameters).
bool wifi_load_credentials(std::string& ssid, std::string& password);

// Clear stored credentials and disconnect.
void wifi_clear_credentials();

// Current connection state.
WifiState wifi_state();
const char* wifi_state_string();

// Optional callback invoked whenever the Wi-Fi state changes. Async
// transitions (connect/disconnect/IP events) are delivered from the ESP
// event loop task context; do not call LVGL or other task-bound code there
// without appropriate synchronization.
typedef void (*wifi_state_change_cb_t)(WifiState state, void* user_data);

// Register (or clear with nullptr) a state-change callback.
void wifi_set_state_change_cb(wifi_state_change_cb_t cb, void* user_data);

// SSID of the currently configured/stored network (empty if none).
const char* wifi_ssid();

// Current IP address string, or empty if not connected.
const char* wifi_ip();

} // namespace hw
