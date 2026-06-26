#include "output.hpp"

#include "cli.hpp"

#include <ArduinoJson.h>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace maiq::cli {

namespace {

std::string fmt_optional(std::optional<double> v) {
    if (!v) return "-";
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << *v;
    return oss.str();
}

std::string fmt_reset(std::optional<time_t> t) {
    if (!t) return "-";
    char buf[64];
    std::tm tm;
    gmtime_r(&*t, &tm);
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S UTC", &tm);
    return buf;
}

std::string fmt_bool(bool b) {
    return b ? "YES" : "NO";
}

} // namespace

std::string render(OutputFormat format, const std::vector<AccountStatus>& statuses) {
    if (format == OutputFormat::Json) {
        JsonDocument doc;
        JsonArray arr = doc["accounts"].to<JsonArray>();
        for (const auto& s : statuses) {
            JsonObject acc = arr.add<JsonObject>();
            acc["account_name"] = s.account_name;
            acc["vendor"] = to_string(s.vendor);
            acc["mode"] = to_string(s.mode);
            acc["is_valid"] = s.is_valid;
            if (s.invalid_message) acc["invalid_message"] = *s.invalid_message;
            JsonArray entries = acc["entries"].to<JsonArray>();
            for (const auto& e : s.entries) {
                JsonObject entry = entries.add<JsonObject>();
                entry["name"] = e.name;
                if (e.used) entry["used"] = *e.used;
                if (e.remaining) entry["remaining"] = *e.remaining;
                if (e.total) entry["total"] = *e.total;
                entry["unit"] = e.unit;
                if (e.reset_at) entry["reset_at"] = fmt_reset(e.reset_at);
            }
        }
        std::string out;
        serializeJsonPretty(doc, out);
        return out;
    }

    // Table format (tab-separated so long names don't break alignment)
    std::ostringstream oss;
    oss << "Account\tVendor\tMode\tValid\tItem\tUsed\tRemaining\tTotal\tUnit\tReset At\n";

    for (const auto& s : statuses) {
        if (!s.is_valid) {
            oss << s.account_name << "\t"
                << to_string(s.vendor) << "\t"
                << to_string(s.mode) << "\t"
                << fmt_bool(false) << "\t"
                << (s.invalid_message ? *s.invalid_message : "unknown") << "\t"
                << "-\t-\t-\t-\t-\n";
            continue;
        }
        if (s.entries.empty()) {
            oss << s.account_name << "\t"
                << to_string(s.vendor) << "\t"
                << to_string(s.mode) << "\t"
                << fmt_bool(true) << "\t"
                << "(no entries)\t-\t-\t-\t-\t-\n";
            continue;
        }
        for (const auto& e : s.entries) {
            oss << s.account_name << "\t"
                << to_string(s.vendor) << "\t"
                << to_string(s.mode) << "\t"
                << fmt_bool(true) << "\t"
                << e.name << "\t"
                << fmt_optional(e.used) << "\t"
                << fmt_optional(e.remaining) << "\t"
                << fmt_optional(e.total) << "\t"
                << e.unit << "\t"
                << fmt_reset(e.reset_at) << "\n";
        }
    }
    return oss.str();
}

} // namespace maiq::cli
