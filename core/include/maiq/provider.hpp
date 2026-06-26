#pragma once

#include "config.hpp"
#include "http_client.hpp"
#include "types.hpp"

#include <memory>
#include <string>

namespace maiq {

class Provider {
public:
    virtual ~Provider() = default;
    virtual const std::string& name() const = 0;
    virtual Vendor vendor() const = 0;
    virtual QueryMode mode() const = 0;
    virtual AccountStatus query(HttpClient& client) const = 0;
};

std::unique_ptr<Provider> build_provider(const ProviderConfig& config);

} // namespace maiq
