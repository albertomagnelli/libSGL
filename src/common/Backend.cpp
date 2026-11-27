#include "backend/Backend.hpp"
#include "backend/BackendFactory.hpp"
#include "common/Internal.hpp"

#include <cstdlib>
#include <memory>
#include <vector>

namespace sgl::detail {
namespace {

std::unique_ptr<Backend> g_backend;
Window* g_currentWindow = nullptr;
InitHintsState g_initHints;
bool g_initialized = false;

bool EnvSet(const char* name) {
    const char* value = std::getenv(name);
    return value != nullptr && value[0] != '\0';
}

} // namespace

InitHintsState& GetInitHints() {
    return g_initHints;
}

bool IsInitialized() {
    return g_initialized;
}

Backend* GetBackend() {
    return g_backend.get();
}

void SetBackend(std::unique_ptr<Backend> backend) {
    g_backend = std::move(backend);
}

Window* GetCurrentWindow() {
    return g_currentWindow;
}

void SetCurrentWindow(Window* window) {
    g_currentWindow = window;
}

std::unique_ptr<Backend> CreatePlatformBackend(Platform platform, Error& error) {
    if (platform == Platform::Any) {
        std::vector<Platform> candidates;
#if defined(SGL_HAS_WAYLAND) && SGL_HAS_WAYLAND
        if (EnvSet("WAYLAND_DISPLAY")) {
            candidates.push_back(Platform::Wayland);
        }
#endif
#if defined(SGL_HAS_X11) && SGL_HAS_X11
        if (EnvSet("DISPLAY")) {
            candidates.push_back(Platform::X11);
        }
#endif
#if defined(SGL_HAS_WIN32) && SGL_HAS_WIN32
        candidates.push_back(Platform::Win32);
#endif
#if defined(SGL_HAS_COCOA) && SGL_HAS_COCOA
        candidates.push_back(Platform::Cocoa);
#endif
        candidates.push_back(Platform::Null);

        Error lastError;
        for (Platform candidate : candidates) {
            Error candidateError;
            if (auto backend = CreatePlatformBackend(candidate, candidateError)) {
                return backend;
            }
            lastError = std::move(candidateError);
        }
        error = lastError.isError()
                    ? std::move(lastError)
                    : MakeError(ErrorCode::PlatformUnavailable,
                                "Keine Plattform konnte initialisiert werden.");
        return nullptr;
    }

    switch (platform) {
    case Platform::X11:
#if defined(SGL_HAS_X11) && SGL_HAS_X11
        return CreateX11Backend(error);
#else
        break;
#endif
    case Platform::Wayland:
#if defined(SGL_HAS_WAYLAND) && SGL_HAS_WAYLAND
        return CreateWaylandBackend(error);
#else
        break;
#endif
    case Platform::Win32:
#if defined(SGL_HAS_WIN32) && SGL_HAS_WIN32
        return CreateWin32Backend(error);
#else
        break;
#endif
    case Platform::Cocoa:
#if defined(SGL_HAS_COCOA) && SGL_HAS_COCOA
        return CreateCocoaBackend(error);
#else
        break;
#endif
    case Platform::Null:
        return CreateNullBackend(error);
    case Platform::Any:
        break;
    }

    error = MakeError(ErrorCode::PlatformUnavailable,
                      "Die angeforderte Plattform ist in diesem Build nicht verfuegbar.");
    return nullptr;
}

bool InitPlatform(const InitOptions& options, Error& error) {
    if (g_initialized) {
        return true;
    }

    auto backend = CreatePlatformBackend(g_initHints.platform, error);
    if (!backend) {
        return false;
    }
    if (!backend->init(options, error)) {
        return false;
    }

    g_backend = std::move(backend);
    g_initialized = true;
    ResetTimeBase();
    return true;
}

void TerminatePlatform() {
    if (g_backend) {
        g_backend->terminate();
        g_backend.reset();
    }
    g_currentWindow = nullptr;
    g_initialized = false;
}

} // namespace sgl::detail
