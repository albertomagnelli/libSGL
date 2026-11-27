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

bool envSet(const char* name) {
    const char* value = std::getenv(name);
    return value != nullptr && value[0] != '\0';
}

} // namespace

InitHintsState& getInitHints() {
    return g_initHints;
}

bool isInitialized() {
    return g_initialized;
}

Backend* getBackend() {
    return g_backend.get();
}

void setBackend(std::unique_ptr<Backend> backend) {
    g_backend = std::move(backend);
}

Window* getCurrentWindow() {
    return g_currentWindow;
}

void setCurrentWindow(Window* window) {
    g_currentWindow = window;
}

std::unique_ptr<Backend> createPlatformBackend(Platform platform, Error& error) {
    if (platform == Platform::Any) {
        std::vector<Platform> candidates;
#if defined(SGL_HAS_WAYLAND) && SGL_HAS_WAYLAND
        if (envSet("WAYLAND_DISPLAY")) {
            candidates.push_back(Platform::Wayland);
        }
#endif
#if defined(SGL_HAS_X11) && SGL_HAS_X11
        if (envSet("DISPLAY")) {
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
            if (auto backend = createPlatformBackend(candidate, candidateError)) {
                return backend;
            }
            lastError = std::move(candidateError);
        }
        error = lastError.isError()
                    ? std::move(lastError)
                    : makeError(ErrorCode::PlatformUnavailable,
                                "Keine Plattform konnte initialisiert werden.");
        return nullptr;
    }

    switch (platform) {
    case Platform::X11:
#if defined(SGL_HAS_X11) && SGL_HAS_X11
        return createX11Backend(error);
#else
        break;
#endif
    case Platform::Wayland:
#if defined(SGL_HAS_WAYLAND) && SGL_HAS_WAYLAND
        return createWaylandBackend(error);
#else
        break;
#endif
    case Platform::Win32:
#if defined(SGL_HAS_WIN32) && SGL_HAS_WIN32
        return createWin32Backend(error);
#else
        break;
#endif
    case Platform::Cocoa:
#if defined(SGL_HAS_COCOA) && SGL_HAS_COCOA
        return createCocoaBackend(error);
#else
        break;
#endif
    case Platform::Null:
        return createNullBackend(error);
    case Platform::Any:
        break;
    }

    error = makeError(ErrorCode::PlatformUnavailable,
                      "Die angeforderte Plattform ist in diesem Build nicht verfuegbar.");
    return nullptr;
}

bool initPlatform(const InitOptions& options, Error& error) {
    if (g_initialized) {
        return true;
    }

    auto backend = createPlatformBackend(g_initHints.platform, error);
    if (!backend) {
        return false;
    }
    if (!backend->init(options, error)) {
        return false;
    }

    g_backend = std::move(backend);
    g_initialized = true;
    resetTimeBase();
    return true;
}

void terminatePlatform() {
    if (g_backend) {
        g_backend->terminate();
        g_backend.reset();
    }
    g_currentWindow = nullptr;
    g_initialized = false;
}

} // namespace sgl::detail
