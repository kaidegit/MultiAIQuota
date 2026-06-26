#pragma once

#include "types.hpp"

#include <ArduinoJson.h>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace maiq {

struct BearerCredentials {
    std::string api_key;
};

struct VolcengineCredentials {
    std::string ak;
    std::string sk;
    std::string region;
};

struct NewApiCredentials {
    std::string access_token;
    std::optional<std::string> user_id;
};

struct CodexOAuthCredentials {
    std::string access_token;
    std::optional<std::string> account_id;
};

using Credentials = std::variant<BearerCredentials,
                                 VolcengineCredentials,
                                 NewApiCredentials,
                                 CodexOAuthCredentials>;

struct ProviderConfig {
    std::string name;
    Vendor vendor;
    QueryMode mode;
    Credentials credentials;
    std::optional<std::string> base_url;
    uint64_t timeout_secs = 30;

    static ProviderConfig from_json(const JsonObjectConst& obj);
    void to_json(JsonObject& obj) const;
};

struct Config {
    std::vector<ProviderConfig> accounts;

    static Config from_json(const JsonDocument& doc);
    void to_json(JsonDocument& doc) const;
    static Config from_json_string(const std::string& s);
    std::string to_json_string() const;
};

// Mask a secret for display (first 4 + last 4 chars).
std::string mask_key(const std::string& key);

} // namespace maiq
