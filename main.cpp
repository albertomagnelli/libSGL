// Kleines Beispiel: OpenGL 4.6 Core Profile mit sgl und dem eigenen Loader
// aus GL.hpp / GL.cpp.
//
// Ablauf:
//   1. sgl initialisieren und ein Fenster mit Core-Profile-Kontext erzeugen
//   2. Kontext aktuell machen (erst danach dürfen GL-Funktionen geladen werden!)
//   3. GL::load() aufrufen – lädt alle Funktionszeiger über sgl::getProcAddress
//   4. Render-Schleife: Farbe animieren, Buffer leeren, Fenster aktualisieren
//
// Beenden mit ESC oder durch Schließen des Fensters.

#include "GL.hpp"

#include <sgl/sgl.hpp>

#include <cmath>
#include <cstdio>
#include <utility>

namespace {

void framebufferSizeCallback(ivec2 size) {
    GL::glViewport(0, 0, size.x, size.y);
}

} // namespace

int main() {
    sgl::setErrorCallback([](const sgl::Error& error) {
        std::fprintf(stderr, "sgl-Fehler %d: %s\n", static_cast<int>(error.code),
                     error.message.c_str());
    });

    if (auto result = sgl::init(); !result) {
        std::fprintf(stderr, "sgl konnte nicht initialisiert werden: %s\n",
                     result.error().message.c_str());
        return 1;
    }

    sgl::WindowHints hints;
    hints.width = 800;
    hints.height = 600;
    hints.title = "GL.hpp - OpenGL 4.6 Loader";
    hints.contextVersionMajor = 4;
    hints.contextVersionMinor = 6;
    hints.contextProfile = sgl::ContextProfile::Core;
    hints.contextForwardCompat = true;

    auto windowResult = sgl::Window::create(hints);
    if (!windowResult) {
        std::fprintf(stderr, "Fenster/Kontext konnte nicht erzeugt werden: %s\n",
                     windowResult.error().message.c_str());
        sgl::terminate();
        return 1;
    }
    std::unique_ptr<sgl::Window> window = std::move(*windowResult);

    window->makeContextCurrent();
    window->setSwapInterval(1); // VSync

    // Wichtig: erst jetzt, mit aktuellem Kontext, die Funktionen laden.
    const GL::LoadReport report = GL::load();
    std::printf("%zu/%zu OpenGL-Funktionen geladen%s\n", report.loaded, report.total,
                report.complete() ? " (vollständig)" : " (Treiber bietet nicht alles an)");

    if (GL::glGetString) {
        const auto* vendor = reinterpret_cast<const char*>(GL::glGetString(GL_VENDOR));
        const auto* renderer = reinterpret_cast<const char*>(GL::glGetString(GL_RENDERER));
        const auto* version = reinterpret_cast<const char*>(GL::glGetString(GL_VERSION));
        std::printf("Vendor:   %s\nRenderer: %s\nVersion:  %s\n", vendor, renderer, version);
    }

    // Ohne diese Basis-Funktionen ist der Kontext unbrauchbar.
    if (!GL::glClear || !GL::glClearColor || !GL::glViewport) {
        std::fprintf(stderr, "Grundlegende GL-Funktionen fehlen.\n");
        window.reset();
        sgl::terminate();
        return 1;
    }

    const ivec2 framebufferSize = window->getFramebufferSize();
    GL::glViewport(0, 0, framebufferSize.x, framebufferSize.y);
    window->setFramebufferSizeCallback(framebufferSizeCallback);

    while (!window->isCloseRequested()) {
        if (window->isKeyPressed(sgl::Key::Escape)) {
            window->setCloseRequested(true);
        }

        // Farbe sanft animieren, damit man sieht, dass die Schleife läuft.
        const auto t = static_cast<float>(sgl::getTime());
        GL::glClearColor(0.5f + 0.5f * std::sin(t),
                         0.5f + 0.5f * std::sin(t + 2.094f),
                         0.5f + 0.5f * std::sin(t + 4.189f),
                         1.0f);
        GL::glClear(GL_COLOR_BUFFER_BIT);

        window->swapBuffers();
        sgl::pollEvents();
    }

    window.reset();
    sgl::terminate();
    return 0;
}
