#include "maiq/providers/volcengine.hpp"

#include "maiq/error.hpp"

namespace maiq {

VolcengineProvider::VolcengineProvider(const ProviderConfig& config)
    : name_(config.name), mode_(config.mode) {}

AccountStatus VolcengineProvider::query(HttpClient&) const {
    throw Error(Error::Code::UnsupportedMode, "volcengine provider not implemented");
    return AccountStatus::invalid(Vendor::Volcengine, name_, mode_, "not implemented");
}

} // namespace maiq
