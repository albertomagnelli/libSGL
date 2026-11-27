// Grundlegendes Fenster mit OpenGL-Clear-Schleife.
//
// Tasten: ESC beendet das Programm.

#include "ExampleCommon.hpp"

#include <cmath>

int main() {
    auto window = example::CreateWindow("sgl – Basic Window", 960, 540);
    if (!window) {
        return 1;
    }

    example::PrintGlInfo();
    example::CloseOnEscape(*window);

    while (!window->isCloseRequested()) {
        const auto t = static_cast<float>(sgl::GetTime());
        example::ClearBackground(0.5f + 0.5f * std::sin(t),
                                 0.5f + 0.5f * std::sin(t + 2.094f),
                                 0.5f + 0.5f * std::sin(t + 4.189f));
        window->swapBuffers();
        sgl::PollEvents();
    }

    window.reset();
    sgl::Terminate();
    return 0;
}
