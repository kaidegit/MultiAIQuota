#include "cli.hpp"
#include "config_loader.hpp"
#include "commands/init.hpp"
#include "commands/list.hpp"
#include "commands/query.hpp"

#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    try {
        auto args = maiq::cli::parse_args(argc, argv);

        switch (args.command) {
            case maiq::cli::Command::Init: {
                std::cout << maiq::cli::commands::run_init() << "\n";
                break;
            }
            case maiq::cli::Command::List: {
                auto config = maiq::cli::load_config(args.config_path);
                std::cout << maiq::cli::commands::run_list(config) << "\n";
                break;
            }
            case maiq::cli::Command::Query: {
                auto config = maiq::cli::load_config(args.config_path);
                std::cout << maiq::cli::commands::run_query(config, args) << "\n";
                break;
            }
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
