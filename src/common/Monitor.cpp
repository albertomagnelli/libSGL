#include "sgl/Monitor.hpp"

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

bool Monitor::isConnected() const {
    auto* backend = requireBackend();
    return backend && backend->isMonitorConnected(*this);
}

ivec2 Monitor::getPosition() const {
    auto* backend = requireBackend();
    return backend ? backend->getMonitorPosition(*this) : ivec2{0, 0};
}

Monitor::Workarea Monitor::getWorkarea() const {
    auto* backend = requireBackend();
    return backend ? backend->getMonitorWorkarea(*this) : Workarea{};
}

ivec2 Monitor::getPhysicalSize() const {
    auto* backend = requireBackend();
    return backend ? backend->getMonitorPhysicalSize(*this) : ivec2{0, 0};
}

vec2 Monitor::getContentScale() const {
    auto* backend = requireBackend();
    return backend ? backend->getMonitorContentScale(*this) : vec2{1.0f, 1.0f};
}

std::string Monitor::getName() const {
    auto* backend = requireBackend();
    return backend ? backend->getMonitorName(*this) : std::string{};
}

std::vector<VideoMode> Monitor::getVideoModes() const {
    auto* backend = requireBackend();
    return backend ? backend->getVideoModes(*this) : std::vector<VideoMode>{};
}

VideoMode Monitor::getVideoMode() const {
    auto* backend = requireBackend();
    return backend ? backend->getVideoMode(*this) : VideoMode{};
}

void Monitor::setGamma(f32 gamma) const {
    if (auto* backend = requireBackend()) {
        backend->setGamma(*this, gamma);
    }
}

GammaRamp Monitor::getGammaRamp() const {
    auto* backend = requireBackend();
    return backend ? backend->getGammaRamp(*this) : GammaRamp{};
}

void Monitor::setGammaRamp(const GammaRamp& ramp) const {
    if (auto* backend = requireBackend()) {
        backend->setGammaRamp(*this, ramp);
    }
}

NativeHandle Monitor::getNativeHandle() const {
    auto* backend = requireBackend();
    return backend ? backend->getMonitorNativeHandle(*this) : NativeHandle{};
}

std::vector<Monitor> getMonitors() {
    auto* backend = requireBackend();
    return backend ? backend->getMonitors() : std::vector<Monitor>{};
}

Monitor getPrimaryMonitor() {
    auto* backend = requireBackend();
    return backend ? backend->getPrimaryMonitor() : Monitor{};
}

void setMonitorCallback(MonitorCallback callback) {
    if (auto* backend = requireBackend()) {
        backend->setMonitorCallback(std::move(callback));
    }
}

} // namespace sgl
