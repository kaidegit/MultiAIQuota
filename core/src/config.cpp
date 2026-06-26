#include "maiq/config.hpp"

#include "maiq/error.hpp"

#include <algorithm>
#include <cctype>

namespace maiq {

namespace {

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
    return s;
}

Vendor parse_vendor(const std::string& s) {
    const std::string l = lower(s);
    if (l == "kimi") return Vendor::Kimi;
    if (l == "codex") return Vendor::Codex;
    if (l == "volcengine") return Vendor::Volcengine;
    if (l == "deepseek") return Vendor::DeepSeek;
    if (l == "newapi" || l == "new_api") return Vendor::NewApi;
    if (l == "openai_platform") return Vendor::OpenAiPlatform;
    throw Error(Error::Code::Config, "unknown vendor: " + s);
}

QueryMode parse_mode(const std::string& s) {
    const std::string l = lower(s);
    if (l == "coding_plan") return QueryMode::CodingPlan;
    if (l == "token_plan") return QueryMode::TokenPlan;
    if (l == "balance") return QueryMode::Balance;
    throw Error(Error::Code::Config, "unknown mode: " + s);
}

} // namespace

ProviderConfig ProviderConfig::from_json(const JsonObjectConst& obj) {
    ProviderConfig cfg;
    cfg.name = obj["name"] | "";
    cfg.vendor = parse_vendor(obj["vendor"] | "");
    cfg.mode = parse_mode(obj["mode"] | "");
    if (obj["base_url"].is<const char*>()) {
        cfg.base_url = obj["base_url"].as<const char*>();
    }
    cfg.timeout_secs = obj["timeout_secs"] | 30;

    const std::string auth_type = lower(obj["auth_type"] | "");
    if (auth_type == "bearer") {
        BearerCredentials c;
        c.api_key = obj["api_key"] | "";
        cfg.credentials = std::move(c);
    } else if (auth_type == "volcengine") {
        VolcengineCredentials c;
        c.ak = obj["ak"] | "";
        c.sk = obj["sk"] | "";
        c.region = obj["region"] | "cn-beijing";
        cfg.credentials = std::move(c);
    } else if (auth_type == "newapi") {
        NewApiCredentials c;
        c.access_token = obj["access_token"] | "";
        if (obj["user_id"].is<const char*>()) {
            c.user_id = obj["user_id"].as<const char*>();
        }
        cfg.credentials = std::move(c);
    } else if (auth_type == "codex_oauth") {
        CodexOAuthCredentials c;
        c.access_token = obj["access_token"] | "";
        if (obj["account_id"].is<const char*>()) {
            c.account_id = obj["account_id"].as<const char*>();
        }
        cfg.credentials = std::move(c);
    } else {
        throw Error(Error::Code::Config, "unknown auth_type: " + auth_type);
    }
    return cfg;
}

void ProviderConfig::to_json(JsonObject& obj) const {
    obj["name"] = name;
    obj["vendor"] = to_string(vendor);
    obj["mode"] = to_string(mode);
    obj["timeout_secs"] = timeout_secs;
    if (base_url) obj["base_url"] = *base_url;

    std::visit([&](const auto& c) {
        using T = std::decay_t<decltype(c)>;
        if constexpr (std::is_same_v<T, BearerCredentials>) {
            obj["auth_type"] = "bearer";
            obj["api_key"] = c.api_key;
        } else if constexpr (std::is_same_v<T, VolcengineCredentials>) {
            obj["auth_type"] = "volcengine";
            obj["ak"] = c.ak;
            obj["sk"] = c.sk;
            obj["region"] = c.region;
        } else if constexpr (std::is_same_v<T, NewApiCredentials>) {
            obj["auth_type"] = "newapi";
            obj["access_token"] = c.access_token;
            if (c.user_id) obj["user_id"] = *c.user_id;
        } else if constexpr (std::is_same_v<T, CodexOAuthCredentials>) {
            obj["auth_type"] = "codex_oauth";
            obj["access_token"] = c.access_token;
            if (c.account_id) obj["account_id"] = *c.account_id;
        }
    }, credentials);
}

Config Config::from_json(const JsonDocument& doc) {
    Config cfg;
    JsonArrayConst arr = doc["accounts"].as<JsonArrayConst>();
    for (JsonObjectConst obj : arr) {
        cfg.accounts.push_back(ProviderConfig::from_json(obj));
    }
    return cfg;
}

void Config::to_json(JsonDocument& doc) const {
    JsonArray arr = doc["accounts"].to<JsonArray>();
    for (const auto& acc : accounts) {
        JsonObject obj = arr.add<JsonObject>();
        acc.to_json(obj);
    }
}

Config Config::from_json_string(const std::string& s) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, s);
    if (err) {
        throw Error(Error::Code::Config, std::string("config JSON parse failed: ") + err.c_str());
    }
    return from_json(doc);
}

std::string Config::to_json_string() const {
    JsonDocument doc;
    to_json(doc);
    std::string out;
    serializeJsonPretty(doc, out);
    return out;
}

std::string mask_key(const std::string& key) {
    if (key.size() <= 8) return "***";
    return key.substr(0, 4) + "..." + key.substr(key.size() - 4);
}

} // namespace maiq
