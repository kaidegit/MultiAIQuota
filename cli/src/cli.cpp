#include "cli.hpp"

#include <cstring>
#include <iostream>

namespace maiq::cli {

void print_usage(const char* program) {
    std::cout << "Usage: " << program << " [options] <command> [options]\n"
              << "Commands:\n"
              << "  query [options]    Query all configured accounts\n"
              << "  list               List configured accounts\n"
              << "  init               Print example configuration\n"
              << "Options:\n"
              << "  -c, --config <path>   Config file path\n"
              << "  -v, --vendor <name>   Filter by vendor (query only)\n"
              << "  -a, --account <name>  Filter by account name (query only)\n"
              << "  -f, --format <table|json> Output format (query only, default: table)\n";
}

Args parse_args(int argc, char** argv) {
    Args args;
    bool command_set = false;

    int i = 1;
    while (i < argc) {
        const char* arg = argv[i];
        if (arg[0] == '-') {
            if (std::strcmp(arg, "-c") == 0 || std::strcmp(arg, "--config") == 0) {
                if (++i >= argc) throw std::runtime_error("missing config path");
                args.config_path = argv[i];
            } else if (std::strcmp(arg, "-v") == 0 || std::strcmp(arg, "--vendor") == 0) {
                if (++i >= argc) throw std::runtime_error("missing vendor");
                args.vendor_filter = argv[i];
            } else if (std::strcmp(arg, "-a") == 0 || std::strcmp(arg, "--account") == 0) {
                if (++i >= argc) throw std::runtime_error("missing account");
                args.account_filter = argv[i];
            } else if (std::strcmp(arg, "-f") == 0 || std::strcmp(arg, "--format") == 0) {
                if (++i >= argc) throw std::runtime_error("missing format");
                std::string f = argv[i];
                if (f == "json") args.format = OutputFormat::Json;
                else if (f == "table") args.format = OutputFormat::Table;
                else throw std::runtime_error("invalid format: " + f);
            } else if (std::strcmp(arg, "-h") == 0 || std::strcmp(arg, "--help") == 0) {
                print_usage(argv[0]);
                std::exit(0);
            } else {
                throw std::runtime_error(std::string("unknown option: ") + arg);
            }
            ++i;
        } else {
            if (command_set) {
                throw std::runtime_error(std::string("unexpected argument: ") + arg);
            }
            if (std::strcmp(arg, "query") == 0) args.command = Command::Query;
            else if (std::strcmp(arg, "list") == 0) args.command = Command::List;
            else if (std::strcmp(arg, "init") == 0) args.command = Command::Init;
            else throw std::runtime_error(std::string("unknown command: ") + arg);
            command_set = true;
            ++i;
        }
    }

    if (!command_set) {
        print_usage(argv[0]);
        std::exit(0);
    }

    return args;
}

} // namespace maiq::cli
