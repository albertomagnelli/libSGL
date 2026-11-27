#include "sgl/Init.hpp"

#include <string>
#include <utility>

#include "backend/Backend.hpp"
#include "common/Internal.hpp"

namespace sgl {

std::expected<void, Error> Init(const InitOptions& options) {
    if (detail::IsInitialized()) {
        return {};
    }

    Error error;
    if (!detail::InitPlatform(options, error)) {
        if (!error.isError()) {
            error = detail::MakeError(ErrorCode::PlatformError, "Initialisierung fehlgeschlagen.");
        }
        return std::unexpected(std::move(error));
    }
    return {};
}

void Terminate() {
    detail::TerminatePlatform();
}

void SetInitHint(InitHint hint, i64 value) {
    if (detail::IsInitialized()) {
        ReportError(ErrorCode::InvalidValue, "initHint muss vor sgl::Init() aufgerufen werden.");
        return;
    }

    auto& hints = detail::GetInitHints();
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
        ReportError(ErrorCode::InvalidEnum, "Unbekannter Init-Hint.");
        break;
    }
}

void SetInitHint(InitHint hint, bool value) {
    SetInitHint(hint, static_cast<i64>(value ? 1 : 0));
}

Version GetVersion() {
    return Version{0, 1, 0};
}

std::string GetVersionString() {
    return "sgl 0.1.0";
}

Platform GetPlatform() {
    auto* backend = detail::GetBackend();
    return backend ? backend->getPlatform() : Platform::Null;
}

bool IsPlatformSupported(Platform platform) {
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
