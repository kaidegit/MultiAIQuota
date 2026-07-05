#pragma once

#include "maiq/config.hpp"

#include <optional>
#include <string>

namespace hw {

// Initialize NVS and LittleFS.
bool storage_init();

// Load account configuration from NVS. Returns empty config on error/no config.
maiq::Config storage_load_config();

// Save account configuration to NVS as a JSON blob.
bool storage_save_config(const maiq::Config& config);

} // namespace hw
