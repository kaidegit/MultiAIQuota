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
    },
    {
      "name": "my-codex",
      "vendor": "codex",
      "mode": "coding_plan",
      "auth_type": "codex_oauth",
      "access_token": "从 ~/.codex/auth.json 的 tokens.access_token 复制",
      "account_id": "从 ~/.codex/auth.json 的 tokens.account_id 复制"
    }
  ]
})";

std::string run_init() {
    return EXAMPLE_CONFIG;
}

} // namespace maiq::cli::commands
