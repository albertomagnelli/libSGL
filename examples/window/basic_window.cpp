// Grundlegendes Fenster mit OpenGL-Clear-Schleife.
//
// Tasten: ESC beendet das Programm.

#include "ExampleCommon.hpp"

#include <cmath>

int main() {
    auto window = example::createWindow("sgl – Basic Window", 960, 540);
    if (!window) {
        return 1;
    }

    example::printGLInfo();
    example::closeOnEscape(*window);

    while (!window->isCloseRequested()) {
        const auto t = static_cast<float>(sgl::getTime());
        example::clearBackground(0.5f + 0.5f * std::sin(t),
                                 0.5f + 0.5f * std::sin(t + 2.094f),
                                 0.5f + 0.5f * std::sin(t + 4.189f));
        window->swapBuffers();
        sgl::pollEvents();
    }

    window.reset();
    sgl::terminate();
    return 0;
}
