#pragma once

#include <cmath>
#include <memory>
#include <string>
#include <string_view>

#include <sgl/sgl.hpp>

namespace fontdemo {

inline void errorCallback(const sgl::Error& error) {
    SLOGE("sgl-Fehler %d: %s", static_cast<int>(error.code), error.message.c_str());
}

inline bool initialize() {
    sgl::setErrorCallback(errorCallback);
    if (auto result = sgl::init(); !result) {
        SLOGE("sgl::init() fehlgeschlagen: %s", result.error().message.c_str());
        return false;
    }
    sgl::Logger::setMaxLogLevel(sgl::Logger::LogLevel::WARNING);
    return true;
}

inline std::unique_ptr<sgl::Window> createWindow(int width, int height, std::string_view title,
                                                 int samples = 0, bool visible = true) {
    sgl::WindowHints hints;
    hints.width = width;
    hints.height = height;
    hints.title = std::string(title);
    hints.samples = samples;
    hints.visible = visible;
    hints.contextVersionMajor = 4;
    hints.contextVersionMinor = 6;
    hints.contextProfile = sgl::ContextProfile::Core;
    hints.contextForwardCompat = true;

    auto result = sgl::Window::create(hints);
    if (!result) {
        SLOGE("Fenster konnte nicht erzeugt werden: %s", result.error().message.c_str());
        return nullptr;
    }

    std::unique_ptr<sgl::Window> window = std::move(*result);
    window->makeContextCurrent();
    window->setSwapInterval(visible ? 1 : 0);
    GL::load();
    if (GL::glCreateProgram == nullptr || GL::glCreateShader == nullptr) {
        SLOGE("OpenGL-Funktionen konnten nicht geladen werden.");
        return nullptr;
    }
    return window;
}

inline vec4 hsvToRgb(f32 hue, f32 saturation, f32 value) {
    hue = std::fmod(hue, 360.0f);
    if (hue < 0.0f) {
        hue += 360.0f;
    }
    const f32 c = value * saturation;
    const f32 x = c * (1.0f - std::abs(std::fmod(hue / 60.0f, 2.0f) - 1.0f));
    const f32 m = value - c;

    f32 r = 0.0f;
    f32 g = 0.0f;
    f32 b = 0.0f;
    if (hue < 60.0f) {
        r = c; g = x;
    } else if (hue < 120.0f) {
        r = x; g = c;
    } else if (hue < 180.0f) {
        g = c; b = x;
    } else if (hue < 240.0f) {
        g = x; b = c;
    } else if (hue < 300.0f) {
        r = x; b = c;
    } else {
        r = c; b = x;
    }
    return vec4(r + m, g + m, b + m, 1.0f);
}

} // namespace fontdemo
