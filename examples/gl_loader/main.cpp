// Kleines Beispiel: OpenGL 4.6 Core Profile mit sgl und dem eigenen Loader
// aus sgl/GL.hpp / src/gl/GL.cpp.
//
// Ablauf:
//   1. sgl initialisieren und ein Fenster mit Core-Profile-Kontext erzeugen
//   2. Kontext aktuell machen (erst danach dürfen GL-Funktionen geladen werden!)
//   3. gl::Load() aufrufen – lädt alle Funktionszeiger über sgl::GetProcAddress
//   4. Render-Schleife: Farbe animieren, Buffer leeren, Fenster aktualisieren
//
// Beenden mit ESC oder durch Schließen des Fensters.

#include <sgl/GL.hpp>

#include <sgl/sgl.hpp>

#include <cmath>
#include <cstdio>
#include <utility>

namespace {

void FramebufferSizeCallback(ivec2 size) {
    gl::glViewport(0, 0, size.x, size.y);
}

} // namespace

int main() {
    sgl::SetErrorCallback([](const sgl::Error& error) {
        SLOGE("sgl-Fehler %d: %s\n", static_cast<int>(error.code),
                     error.message.c_str());
    });

    if (auto result = sgl::Init(); !result) {
        SLOGE("sgl konnte nicht initialisiert werden: %s\n",
                     result.error().message.c_str());
        return 1;
    }

    sgl::WindowHints hints;
    hints.width = 800;
    hints.height = 600;
    hints.title = "sgl - OpenGL 4.6 Loader";
    hints.contextVersionMajor = 4;
    hints.contextVersionMinor = 6;
    hints.contextProfile = sgl::ContextProfile::Core;
    hints.contextForwardCompat = true;

    auto windowResult = sgl::Window::Create(hints);
    if (!windowResult) {
        SLOGE("Fenster/Kontext konnte nicht erzeugt werden: %s\n",
                     windowResult.error().message.c_str());
        sgl::Terminate();
        return 1;
    }
    std::unique_ptr<sgl::Window> window = std::move(*windowResult);

    window->makeContextCurrent();
    window->setSwapInterval(1); // VSync

    // Wichtig: erst jetzt, mit aktuellem Kontext, die Funktionen laden.
    const gl::LoadReport report = gl::Load();
    SLOGI("%zu/%zu OpenGL-Funktionen geladen%s\n", report.loaded, report.total,
                report.complete() ? " (vollständig)" : " (Treiber bietet nicht alles an)");

    if (gl::glGetString) {
        const auto* vendor = reinterpret_cast<const char*>(gl::glGetString(GL_VENDOR));
        const auto* renderer = reinterpret_cast<const char*>(gl::glGetString(GL_RENDERER));
        const auto* version = reinterpret_cast<const char*>(gl::glGetString(GL_VERSION));
        SLOGI("Vendor:   %s\nRenderer: %s\nVersion:  %s\n", vendor, renderer, version);
    }

    // Ohne diese Basis-Funktionen ist der Kontext unbrauchbar.
    if (!gl::glClear || !gl::glClearColor || !gl::glViewport) {
        SLOGE("Grundlegende GL-Funktionen fehlen.\n");
        window.reset();
        sgl::Terminate();
        return 1;
    }

    const ivec2 framebufferSize = window->getFramebufferSize();
    gl::glViewport(0, 0, framebufferSize.x, framebufferSize.y);
    window->setFramebufferSizeCallback(FramebufferSizeCallback);

    while (!window->isCloseRequested()) {
        if (window->isKeyPressed(sgl::Key::Escape)) {
            window->setCloseRequested(true);
        }

        // Farbe sanft animieren, damit man sieht, dass die Schleife läuft.
        const auto t = static_cast<float>(sgl::GetTime());
        gl::glClearColor(0.5f + 0.5f * std::sin(t),
                         0.5f + 0.5f * std::sin(t + 2.094f),
                         0.5f + 0.5f * std::sin(t + 4.189f),
                         1.0f);
        gl::glClear(GL_COLOR_BUFFER_BIT);

        window->swapBuffers();
        sgl::PollEvents();
    }

    window.reset();
    sgl::Terminate();
    return 0;
}
