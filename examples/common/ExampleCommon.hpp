#pragma once

#include <cstdio>
#include <memory>
#include <string>
#include <string_view>

#include <sgl/GL.hpp>
#include <sgl/sgl.hpp>

namespace example {

inline void ErrorCallback(const sgl::Error& error) {
    SLOGE("sgl-Fehler %d: %s\n", static_cast<int>(error.code),
                 error.message.c_str());
}

/// Initialisiert sgl, erzeugt ein Fenster und lädt die OpenGL-Funktionen.
/// Falls kein 4.6-Kontext verfügbar ist, wird auf 3.3 zurückgefallen.
inline std::unique_ptr<sgl::Window> CreateWindow(std::string_view title, int width, int height,
                                                 sgl::WindowHints hints = {}) {
    sgl::SetErrorCallback(ErrorCallback);

    if (auto result = sgl::Init(); !result) {
        SLOGE("sgl::Init() fehlgeschlagen: %s\n", result.error().message.c_str());
        return nullptr;
    }

    hints.width = width;
    hints.height = height;
    hints.title = std::string(title);
    hints.contextVersionMajor = 4;
    hints.contextVersionMinor = 6;
    hints.contextProfile = sgl::ContextProfile::Core;
    hints.contextForwardCompat = true;

    auto result = sgl::Window::Create(hints);
    if (!result) {
        // Ältere Treiber / macOS: konservativerer Kontext.
        sgl::WindowHints fallback = hints;
        fallback.contextVersionMajor = 3;
        fallback.contextVersionMinor = 3;
        result = sgl::Window::Create(fallback);
    }
    if (!result) {
        // Manche GLX-Server (Xvfb/llvmpipe) bieten keinen MSAA-Framebuffer.
        sgl::WindowHints fallback = hints;
        fallback.contextVersionMajor = 3;
        fallback.contextVersionMinor = 3;
        fallback.samples = 0;
        result = sgl::Window::Create(fallback);
    }
    if (!result) {
        // Letzter Versuch: klassischer Kompatibilitätskontext ohne MSAA.
        sgl::WindowHints fallback = hints;
        fallback.contextVersionMajor = 2;
        fallback.contextVersionMinor = 1;
        fallback.contextProfile = sgl::ContextProfile::Any;
        fallback.contextForwardCompat = false;
        fallback.samples = 0;
        result = sgl::Window::Create(fallback);
    }
    if (!result) {
        SLOGE("Fenster konnte nicht erzeugt werden: %s\n",
                     result.error().message.c_str());
        sgl::Terminate();
        return nullptr;
    }

    auto window = std::move(*result);
    window->makeContextCurrent();
    window->setSwapInterval(1);

    const gl::LoadReport report = gl::Load();
    if (!report.complete()) {
        SLOGE("%zu/%zu OpenGL-Funktionen geladen.\n", report.loaded, report.total);
    }

    const ivec2 framebuffer = window->getFramebufferSize();
    gl::glViewport(0, 0, framebuffer.x, framebuffer.y);
    window->setFramebufferSizeCallback([](ivec2 size) {
        gl::glViewport(0, 0, size.x, size.y);
    });

    return window;
}

/// Beendet die Schleife bei ESC.
inline void CloseOnEscape(sgl::Window& window) {
    window.setKeyCallback([&window](sgl::Key key, i32, sgl::KeyAction action,
                                   sgl::Modifiers) {
        if (key == sgl::Key::Escape && action == sgl::KeyAction::Press) {
            window.setCloseRequested(true);
        }
    });
}

inline void PrintGlInfo() {
    if (!gl::glGetString) {
        return;
    }
    const auto* vendor = reinterpret_cast<const char*>(gl::glGetString(GL_VENDOR));
    const auto* renderer = reinterpret_cast<const char*>(gl::glGetString(GL_RENDERER));
    const auto* version = reinterpret_cast<const char*>(gl::glGetString(GL_VERSION));
    SLOGI("Vendor:   %s\nRenderer: %s\nVersion:  %s\n", vendor ? vendor : "?",
                renderer ? renderer : "?", version ? version : "?");
}

/// Zeichnet einen farbigen Hintergrund.
inline void ClearBackground(float red, float green, float blue) {
    gl::glClearColor(red, green, blue, 1.0f);
    gl::glClear(GL_COLOR_BUFFER_BIT);
}

/// Zeichnet ein Rechteck per Scissor-Test (ohne Shader).
/// Koordinaten: Ursprung oben links, y nach unten – wie bei sgl-Cursorpos.
inline void DrawRectTopLeft(ivec2 topLeft, ivec2 size, ivec2 framebufferSize, float red,
                            float green, float blue) {
    gl::glEnable(GL_SCISSOR_TEST);
    gl::glScissor(topLeft.x, framebufferSize.y - topLeft.y - size.y, size.x, size.y);
    gl::glClearColor(red, green, blue, 1.0f);
    gl::glClear(GL_COLOR_BUFFER_BIT);
    gl::glDisable(GL_SCISSOR_TEST);
}

inline void PrintAttribs(const sgl::Window& window) {
    SLOGI("visible=%d decorated=%d resizable=%d floating=%d maximized=%d iconified=%d "
                "focused=%d hovered=%d opacity=%.2f\n",
                window.isVisible() ? 1 : 0, window.isDecorated() ? 1 : 0,
                window.isResizable() ? 1 : 0, window.isFloating() ? 1 : 0,
                window.isMaximized() ? 1 : 0, window.isIconified() ? 1 : 0,
                window.isFocused() ? 1 : 0, window.isHovered() ? 1 : 0,
                static_cast<double>(window.getOpacity()));
}

inline void PrintKeyHelp(std::string_view text) {
    SLOGI("%s\n", std::string(text).c_str());
}

} // namespace example
