#include "init.hpp"

namespace maiq::cli::commands {

const char* EXAMPLE_CONFIG = R"({
  "accounts": [
    {
      "name": "my-kimi-code",
      "vendor": "kimi",
      "mode": "coding_plan",
      "auth_type": "bearer",
      "api_key": "sk-kimi-xxxxxxxx"
    },
    {
      "name": "my-deepseek",
      "vendor": "deepseek",
      "mode": "balance",
      "auth_type": "bearer",
      "api_key": "sk-xxxxxxxx"
    }
  ]
})";

std::string run_init() {
    return EXAMPLE_CONFIG;
}

} // namespace maiq::cli::commands
