#pragma once

namespace hw {

// Start the built-in HTTP server. Serves the Svelte frontend from LittleFS
// and exposes RESTful API endpoints for Wi-Fi, config, and querying.
void web_server_start();

// Returns true once after the configuration has been saved through the web UI,
// then resets the flag. Call periodically from the main loop to refresh the
// display with the latest configuration.
bool web_server_take_config_saved_flag();

} // namespace hw
