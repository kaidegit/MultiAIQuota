#include "maiq/types.hpp"

namespace maiq {

std::string to_string(Vendor v) {
    switch (v) {
        case Vendor::Kimi: return "kimi";
        case Vendor::Codex: return "codex";
        case Vendor::Volcengine: return "volcengine";
        case Vendor::DeepSeek: return "deepseek";
        case Vendor::NewApi: return "newapi";
        case Vendor::OpenAiPlatform: return "openai_platform";
    }
    return "unknown";
}

std::string to_string(QueryMode m) {
    switch (m) {
        case QueryMode::CodingPlan: return "coding_plan";
        case QueryMode::TokenPlan: return "token_plan";
        case QueryMode::Balance: return "balance";
    }
    return "unknown";
}

} // namespace maiq
