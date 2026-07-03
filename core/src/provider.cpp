#include "maiq/provider.hpp"

#include "maiq/providers/codex.hpp"
#include "maiq/providers/deepseek.hpp"
#include "maiq/providers/kimi.hpp"

namespace maiq {

std::unique_ptr<Provider> build_provider(const ProviderConfig& config) {
    switch (config.vendor) {
        case Vendor::Kimi:
            return std::make_unique<KimiProvider>(config);
        case Vendor::DeepSeek:
            return std::make_unique<DeepSeekProvider>(config);
        case Vendor::Codex:
            return std::make_unique<CodexProvider>(config);
        default:
            throw Error(Error::Code::UnsupportedMode,
                        to_string(config.vendor) + " provider not implemented yet");
    }
}

} // namespace maiq
