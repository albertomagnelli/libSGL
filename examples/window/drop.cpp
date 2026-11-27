// Drag & Drop: Dateien aus dem Dateimanager auf das Fenster ziehen.
// ESC beendet das Programm.

#include "ExampleCommon.hpp"

#include <cstdio>

int main() {
    auto window = example::createWindow("sgl – Drag & Drop", 800, 500);
    if (!window) {
        return 1;
    }

    example::printKeyHelp("Dateien auf das Fenster ziehen; ESC beendet.");

    window->setDropCallback([](std::span<const std::string> paths) {
        SLOGI("Drop mit %zu Pfad(en):\n", paths.size());
        for (const std::string& path : paths) {
            SLOGI("  %s\n", path.c_str());
        }
    });

    window->setKeyCallback([&](sgl::Key key, i32, sgl::KeyAction action, sgl::Modifiers) {
        if (key == sgl::Key::Escape && action == sgl::KeyAction::Press) {
            window->setCloseRequested(true);
        }
    });

    while (!window->isCloseRequested()) {
        const ivec2 framebuffer = window->getFramebufferSize();
        example::clearBackground(0.06f, 0.09f, 0.08f);
        example::drawRectTopLeft(ivec2(40, 40), ivec2(framebuffer.x - 80, framebuffer.y - 80),
                                 framebuffer, 0.10f, 0.18f, 0.14f);
        window->swapBuffers();
        sgl::pollEvents();
    }

    window.reset();
    sgl::terminate();
    return 0;
}
