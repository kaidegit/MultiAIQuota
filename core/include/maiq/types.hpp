#pragma once

#include <cstdint>
#include <ctime>
#include <optional>
#include <string>
#include <vector>

namespace maiq {

enum class Vendor {
    Kimi,
    Codex,
    Volcengine,
    DeepSeek,
    NewApi,
    OpenAiPlatform,
};

enum class QueryMode {
    CodingPlan,
    TokenPlan,
    Balance,
};

std::string to_string(Vendor v);
std::string to_string(QueryMode m);

struct QuotaEntry {
    std::string name;
    std::optional<double> used;
    std::optional<double> remaining;
    std::optional<double> total;
    std::string unit;
    std::optional<time_t> reset_at;

    QuotaEntry() = default;
    QuotaEntry(std::string name, std::string unit)
        : name(std::move(name)), unit(std::move(unit)) {}

    QuotaEntry& with_used(double v) { used = v; return *this; }
    QuotaEntry& with_remaining(double v) { remaining = v; return *this; }
    QuotaEntry& with_total(double v) { total = v; return *this; }
    QuotaEntry& with_reset_at(time_t t) { reset_at = t; return *this; }
};

struct AccountStatus {
    Vendor vendor;
    std::string account_name;
    QueryMode mode;
    bool is_valid = true;
    std::optional<std::string> invalid_message;
    std::vector<QuotaEntry> entries;

    AccountStatus() = default;
    AccountStatus(Vendor vendor, std::string account_name, QueryMode mode,
                  std::vector<QuotaEntry> entries)
        : vendor(vendor), account_name(std::move(account_name)), mode(mode),
          entries(std::move(entries)) {}

    static AccountStatus invalid(Vendor vendor, std::string account_name,
                                  QueryMode mode, std::string message) {
        AccountStatus s(vendor, std::move(account_name), mode, {});
        s.is_valid = false;
        s.invalid_message = std::move(message);
        return s;
    }
};

} // namespace maiq
