#pragma once

#include <string>

namespace hw {

// Initialize the in-memory web asset store: decompress the firmware-embedded
// www.tar.gz (gzip) into PSRAM and build a path -> (data, size) index.
// Called once at startup, before the web server starts.
bool www_store_init();

// Read a whole file by its path (e.g. "/index.html", "/assets/x.js").
// Returns false if the file is not present in the embedded archive.
bool www_store_read(const char* path, std::string& out);

} // namespace hw
