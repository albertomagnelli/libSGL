#include "sgl/Events.hpp"

#include "backend/Backend.hpp"

namespace sgl {
namespace {

detail::Backend* requireBackend() {
    auto* backend = detail::getBackend();
    if (!backend) {
        reportError(ErrorCode::NotInitialized, "sgl ist nicht initialisiert.");
    }
    return backend;
}

} // namespace

void pollEvents() {
    if (auto* backend = requireBackend()) {
        backend->pollEvents();
    }
}

void waitEvents() {
    if (auto* backend = requireBackend()) {
        backend->waitEvents(-1.0);
    }
}

void waitEventsTimeout(f64 seconds) {
    if (auto* backend = requireBackend()) {
        backend->waitEvents(seconds);
    }
}

void postEmptyEvent() {
    if (auto* backend = requireBackend()) {
        backend->postEmptyEvent();
    }
}

} // namespace sgl
