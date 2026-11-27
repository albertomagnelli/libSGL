#pragma once

#include <functional>
#include <string>
#include <utility>

#include "Types.hpp"

namespace sgl {

enum class ErrorCode : i32 {
    None = 0,
    NotInitialized = 0x00010001,
    NoCurrentContext = 0x00010002,
    InvalidEnum = 0x00010003,
    InvalidValue = 0x00010004,
    OutOfMemory = 0x00010005,
    ApiUnavailable = 0x00010006,
    VersionUnavailable = 0x00010007,
    PlatformError = 0x00010008,
    FormatUnavailable = 0x00010009,
    NoWindowContext = 0x0001000A,
    FeatureUnavailable = 0x0001000C,
    PlatformUnavailable = 0x0001000E,
};

struct Error {
    ErrorCode code = ErrorCode::None;
    std::string message;

    [[nodiscard]] bool isError() const { return code != ErrorCode::None; }
    explicit operator bool() const { return isError(); }
};

using ErrorCallback = std::function<void(const Error&)>;

void setErrorCallback(ErrorCallback callback);
Error getError();
void reportError(ErrorCode code, std::string message);
void clearError();

namespace detail {
inline Error makeError(ErrorCode code, std::string message) {
    return Error{code, std::move(message)};
}
} // namespace detail

} // namespace sgl
