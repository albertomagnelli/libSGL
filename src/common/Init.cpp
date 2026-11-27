#include "sgl/Init.hpp"

#include <string>
#include <utility>

#include "backend/Backend.hpp"
#include "common/Internal.hpp"

namespace sgl {

std::expected<void, Error> init(const InitOptions& options) {
    if (detail::isInitialized()) {
        return {};
    }

    Error error;
    if (!detail::initPlatform(options, error)) {
        if (!error.isError()) {
            error = detail::makeError(ErrorCode::PlatformError, "Initialisierung fehlgeschlagen.");
        }
        return std::unexpected(std::move(error));
    }
    return {};
}

void terminate() {
    detail::terminatePlatform();
}

void initHint(InitHint hint, i64 value) {
    if (detail::isInitialized()) {
        reportError(ErrorCode::InvalidValue, "initHint muss vor sgl::init() aufgerufen werden.");
        return;
    }

    auto& hints = detail::getInitHints();
    switch (hint) {
    case InitHint::Platform:
        hints.platform = static_cast<Platform>(value);
        break;
    case InitHint::JoystickHatButtons:
        hints.joystickHatButtons = value != 0;
        break;
    case InitHint::CocoaChdirResources:
        hints.cocoaChdirResources = value != 0;
        break;
    case InitHint::CocoaMenubar:
        hints.cocoaMenubar = value != 0;
        break;
    case InitHint::WaylandLibdecor:
        hints.waylandLibdecor = value != 0;
        break;
    case InitHint::AnglePlatformType:
        hints.anglePlatformType = static_cast<AnglePlatformType>(value);
        break;
    default:
        reportError(ErrorCode::InvalidEnum, "Unbekannter Init-Hint.");
        break;
    }
}

void initHint(InitHint hint, bool value) {
    initHint(hint, static_cast<i64>(value ? 1 : 0));
}

Version getVersion() {
    return Version{0, 1, 0};
}

std::string getVersionString() {
    return "sgl 0.1.0";
}

Platform getPlatform() {
    auto* backend = detail::getBackend();
    return backend ? backend->getPlatform() : Platform::Null;
}

bool isPlatformSupported(Platform platform) {
    switch (platform) {
    case Platform::Any:
        return true;
    case Platform::Null:
        return true;
    case Platform::X11:
#if defined(SGL_HAS_X11) && SGL_HAS_X11
        return true;
#else
        return false;
#endif
    case Platform::Wayland:
#if defined(SGL_HAS_WAYLAND) && SGL_HAS_WAYLAND
        return true;
#else
        return false;
#endif
    case Platform::Win32:
#if defined(SGL_HAS_WIN32) && SGL_HAS_WIN32
        return true;
#else
        return false;
#endif
    case Platform::Cocoa:
#if defined(SGL_HAS_COCOA) && SGL_HAS_COCOA
        return true;
#else
        return false;
#endif
    }
    return false;
}

} // namespace sgl
