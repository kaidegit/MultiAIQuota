#pragma once

#include <ArduinoJson.h>
#include <ctime>
#include <optional>
#include <string>

namespace maiq {

// Portable UTC time conversion (newlib on ESP32 may not provide timegm).
inline time_t utc_time_from_tm(const std::tm& tm) {
#ifdef ESP_PLATFORM
    static constexpr int days_before_month[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    int year = tm.tm_year + 1900;
    int month = tm.tm_mon;
    int day = tm.tm_mday - 1;
    long long y = year - 1;
    long long days = y * 365 + y / 4 - y / 100 + y / 400
                   + days_before_month[month] + day;
    if (month > 1 && ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))) {
        ++days;
    }
    return days * 86400LL + tm.tm_hour * 3600 + tm.tm_min * 60 + tm.tm_sec;
#else
    return timegm(const_cast<std::tm*>(&tm));
#endif
}

// Helpers to read numbers that may be strings or numbers in JSON.
inline std::optional<double> json_number(JsonVariantConst v) {
    if (v.is<double>()) return v.as<double>();
    if (v.is<const char*>()) {
        try {
            return std::stod(v.as<const char*>());
        } catch (...) {
            return std::nullopt;
        }
    }
    if (v.is<std::string>()) {
        try {
            return std::stod(v.as<std::string>());
        } catch (...) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

inline std::optional<time_t> json_reset_time(JsonVariantConst v) {
    if (v.isNull()) return std::nullopt;
    if (v.is<const char*>()) {
        const char* s = v.as<const char*>();
        std::tm tm = {};
        const char* p = strptime(s, "%Y-%m-%dT%H:%M:%S", &tm);
        if (p != nullptr) {
            // Skip optional fractional seconds and timezone suffix (Z or +/-HHMM)
            if (*p == '.') {
                ++p;
                while (*p >= '0' && *p <= '9') ++p;
            }
            if (*p == 'Z' || *p == '+' || *p == '-') {
                return utc_time_from_tm(tm);
            }
        }
    }
    return std::nullopt;
}

} // namespace maiq
