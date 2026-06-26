#include "maiq/config.hpp"
#include "maiq/platform/http_client_pc.hpp"
#include "maiq/query.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

namespace {

std::string read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

bool find_config(std::string& out) {
    const std::vector<std::string> paths = {
        "tests/live_config.json",
        "../tests/live_config.json",
        "../../tests/live_config.json",
    };
    for (const auto& p : paths) {
        out = read_file(p);
        if (!out.empty()) return true;
    }
    return false;
}

} // namespace

int main() {
    std::string cfg_text;
    if (!find_config(cfg_text)) {
        std::cout << "SKIP: tests/live_config.json not found\n";
        return 0;
    }

    maiq::Config cfg;
    try {
        cfg = maiq::Config::from_json_string(cfg_text);
    } catch (const std::exception& e) {
        std::cerr << "FAIL: parse live_config.json: " << e.what() << "\n";
        return 1;
    }

    if (cfg.accounts.empty()) {
        std::cout << "SKIP: no accounts in live_config.json\n";
        return 0;
    }

    auto client = maiq::create_http_client(30000);
    auto statuses = maiq::query_all_statuses(*client, cfg.accounts);

    bool all_ok = true;
    for (const auto& s : statuses) {
        std::cout << s.account_name << " (" << maiq::to_string(s.vendor) << "): "
                  << (s.is_valid ? "OK" : "FAIL") << "\n";
        if (!s.is_valid) {
            std::cerr << "  error: " << *s.invalid_message << "\n";
            all_ok = false;
            continue;
        }
        for (const auto& e : s.entries) {
            std::cout << "  " << e.name << " used=" << (e.used ? std::to_string(*e.used) : "-")
                      << " remaining=" << (e.remaining ? std::to_string(*e.remaining) : "-")
                      << " total=" << (e.total ? std::to_string(*e.total) : "-")
                      << " " << e.unit << "\n";
        }
    }

    return all_ok ? 0 : 1;
}
