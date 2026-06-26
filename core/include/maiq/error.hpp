#pragma once

#include <stdexcept>
#include <string>

namespace maiq {

class Error : public std::runtime_error {
public:
    enum class Code {
        Http,
        Json,
        Api,
        MissingField,
        InvalidCredentials,
        UnsupportedMode,
        Config,
        Signing,
        Other,
    };

    Error(Code code, std::string message)
        : std::runtime_error(message), code_(code), message_(std::move(message)) {}

    Code code() const noexcept { return code_; }
    const std::string& message() const noexcept { return message_; }

private:
    Code code_;
    std::string message_;
};

} // namespace maiq
