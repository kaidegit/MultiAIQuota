#include "config_loader.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unistd.h>
#include <sys/stat.h>

namespace maiq::cli {

namespace {

std::string read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        throw std::runtime_error("cannot open config file: " + path);
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string home_config_path() {
    const char* home = std::getenv("HOME");
    if (!home) return "";
    return std::string(home) + "/.config/multi-ai-quota/config.json";
}

bool file_exists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

} // namespace

Config load_config(const std::optional<std::string>& explicit_path) {
    std::string path;
    if (explicit_path) {
        path = *explicit_path;
    } else if (const char* env = std::getenv("MAIQ_CONFIG"); env && *env) {
        path = env;
    } else if (file_exists("./maiq.json")) {
        path = "./maiq.json";
    } else {
        path = home_config_path();
    }

    if (path.empty() || !file_exists(path)) {
        throw std::runtime_error("config file not found: " + path);
    }

    return Config::from_json_string(read_file(path));
}

} // namespace maiq::cli
