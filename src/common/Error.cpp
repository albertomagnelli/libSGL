#include "sgl/Error.hpp"

#include <mutex>
#include <utility>

namespace sgl {
namespace {

thread_local Error t_lastError;
std::mutex g_callbackMutex;
ErrorCallback g_errorCallback;

} // namespace

void setErrorCallback(ErrorCallback callback) {
    std::lock_guard lock(g_callbackMutex);
    g_errorCallback = std::move(callback);
}

Error getError() {
    Error error = std::move(t_lastError);
    t_lastError = {};
    return error;
}

void clearError() {
    t_lastError = {};
}

void reportError(ErrorCode code, std::string message) {
    Error error{code, std::move(message)};
    t_lastError = error;

    ErrorCallback callback;
    {
        std::lock_guard lock(g_callbackMutex);
        callback = g_errorCallback;
    }
    if (callback) {
        callback(error);
    }
}

} // namespace sgl
