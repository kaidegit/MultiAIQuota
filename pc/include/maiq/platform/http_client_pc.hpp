#pragma once

#include "maiq/http_client.hpp"

#include <cstdint>
#include <memory>

namespace maiq {

// Create a PC HttpClient backed by cpp-httplib (with OpenSSL).
std::unique_ptr<HttpClient> create_http_client(uint32_t timeout_ms = 30000);

} // namespace maiq
