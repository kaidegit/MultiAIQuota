#pragma once

namespace hw {

// Initialize SNTP and wait for the system time to be synchronized.
// Sets the timezone to China Standard Time (CST-8) for localtime conversions.
// Should be called after Wi-Fi is connected.
// Returns true if the time was successfully synchronized.
bool time_sync();

} // namespace hw
