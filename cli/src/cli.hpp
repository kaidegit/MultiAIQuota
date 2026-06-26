#pragma once

#include <optional>
#include <string>
#include <vector>

namespace maiq::cli {

enum class Command {
    Query,
    List,
    Init,
};

enum class OutputFormat {
    Table,
    Json,
};

struct Args {
    Command command = Command::Query;
    std::optional<std::string> config_path;
    std::optional<std::string> vendor_filter;
    std::optional<std::string> account_filter;
    OutputFormat format = OutputFormat::Table;
};

Args parse_args(int argc, char** argv);
void print_usage(const char* program);

} // namespace maiq::cli
