#pragma once

namespace hw {

// Start the built-in HTTP server. Serves the Svelte frontend from LittleFS
// and exposes RESTful API endpoints for Wi-Fi, config, and querying.
void web_server_start();

} // namespace hw
