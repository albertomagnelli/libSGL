// Vollbild-Umschaltung: F11 wechselt zwischen Fenster und Vollbild.
// ESC beendet das Programm.

#include "ExampleCommon.hpp"

#include <cstdio>
#include <optional>

int main() {
    auto window = example::createWindow("sgl – Fullscreen", 960, 540);
    if (!window) {
        return 1;
    }

    bool fullscreen = false;
    ivec2 windowedPosition = window->getPosition();
    ivec2 windowedSize = window->getSize();
    sgl::Monitor fullscreenMonitor;

    example::printKeyHelp("F11 Vollbild, ESC beenden");

    window->setKeyCallback([&](sgl::Key key, i32, sgl::KeyAction action, sgl::Modifiers) {
        if (action != sgl::KeyAction::Press) {
            return;
        }
        if (key == sgl::Key::Escape) {
            window->setCloseRequested(true);
            return;
        }
        if (key != sgl::Key::F11) {
            return;
        }

        if (!fullscreen) {
            windowedPosition = window->getPosition();
            windowedSize = window->getSize();

            std::optional<sgl::Monitor> current = window->getMonitor();
            sgl::Monitor monitor = current ? *current : sgl::getPrimaryMonitor();
            if (!monitor.isValid()) {
                monitor = sgl::getPrimaryMonitor();
            }
            if (!monitor.isValid()) {
                SLOGE("Kein Monitor für Vollbild gefunden.\n");
                return;
            }

            sgl::VideoMode mode = monitor.getVideoMode();
            fullscreenMonitor = monitor;
            if (mode.size.x > 0 && mode.size.y > 0) {
                window->setMonitor(monitor, monitor.getPosition(), mode.size, mode.refreshRate);
            } else {
                window->setMonitor(monitor);
            }
            fullscreen = true;
            SLOGI("Vollbild auf '%s' (%dx%d @ %d Hz)\n", monitor.getName().c_str(),
                        mode.size.x, mode.size.y, mode.refreshRate);
        } else {
            window->setMonitor(sgl::Monitor{});
            window->setSize(windowedSize);
            if (sgl::getPlatform() != sgl::Platform::Wayland) {
                window->setPosition(windowedPosition);
            }
            fullscreen = false;
            SLOGI("Fenstermodus wiederhergestellt: %dx%d bei %d,%d\n", windowedSize.x,
                  windowedSize.y, windowedPosition.x, windowedPosition.y);
        }
    });

    while (!window->isCloseRequested()) {
        const auto t = static_cast<float>(sgl::getTime());
        example::clearBackground(fullscreen ? 0.10f : 0.16f,
                                 fullscreen ? 0.20f : 0.20f,
                                 fullscreen ? 0.16f : 0.26f);
        (void)t;
        window->swapBuffers();
        sgl::pollEvents();
    }

    window.reset();
    sgl::terminate();
    return 0;
}
