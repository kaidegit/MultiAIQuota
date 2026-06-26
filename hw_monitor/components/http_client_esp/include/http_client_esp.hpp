#pragma once

#include "maiq/http_client.hpp"

#include <cstdint>
#include <memory>

namespace maiq {

// Create an ESP32 HttpClient backed by esp_http_client.
std::unique_ptr<HttpClient> create_http_client_esp(uint32_t timeout_ms = 30000);

} // namespace maiq
