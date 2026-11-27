#include "sgl/Events.hpp"

#include "backend/Backend.hpp"

namespace sgl {
namespace {

detail::Backend* RequireBackend() {
    auto* backend = detail::GetBackend();
    if (!backend) {
        ReportError(ErrorCode::NotInitialized, "sgl ist nicht initialisiert.");
    }
    return backend;
}

} // namespace

void PollEvents() {
    if (auto* backend = RequireBackend()) {
        backend->pollEvents();
    }
}

void WaitEvents() {
    if (auto* backend = RequireBackend()) {
        backend->waitEvents(-1.0);
    }
}

void WaitEventsTimeout(f64 seconds) {
    if (auto* backend = RequireBackend()) {
        backend->waitEvents(seconds);
    }
}

void PostEmptyEvent() {
    if (auto* backend = RequireBackend()) {
        backend->postEmptyEvent();
    }
}

} // namespace sgl
