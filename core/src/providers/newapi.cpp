#include "maiq/providers/newapi.hpp"

#include "maiq/error.hpp"

namespace maiq {

NewApiProvider::NewApiProvider(const ProviderConfig& config) : name_(config.name) {}

AccountStatus NewApiProvider::query(HttpClient&) const {
    throw Error(Error::Code::UnsupportedMode, "newapi provider not implemented");
    return AccountStatus::invalid(Vendor::NewApi, name_, QueryMode::Balance, "not implemented");
}

} // namespace maiq
