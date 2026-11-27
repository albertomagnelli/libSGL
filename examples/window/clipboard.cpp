// Zwischenablage: C kopiert einen Text, V liest die Zwischenablage.
// ESC beendet das Programm.

#include "ExampleCommon.hpp"

#include <cstdio>
#include <string>

int main() {
    auto window = example::createWindow("sgl – Clipboard", 800, 500);
    if (!window) {
        return 1;
    }

    int counter = 0;
    example::printKeyHelp("C kopieren, V einfügen, ESC beenden");

    window->setKeyCallback([&](sgl::Key key, i32, sgl::KeyAction action, sgl::Modifiers) {
        if (action != sgl::KeyAction::Press) {
            return;
        }
        if (key == sgl::Key::Escape) {
            window->setCloseRequested(true);
        } else if (key == sgl::Key::C) {
            ++counter;
            const std::string text =
                "sgl Zwischenablage Nr. " + std::to_string(counter) + " – " +
                std::to_string(static_cast<int>(sgl::getTime())) + "s";
            window->setClipboardString(text);
            SLOGI("Kopiert: %s\n", text.c_str());
        } else if (key == sgl::Key::V) {
            const std::string text = window->getClipboardString();
            SLOGI("Eingefügt (%zu Zeichen): %s\n", text.size(), text.c_str());
        }
    });

    while (!window->isCloseRequested()) {
        example::clearBackground(0.08f, 0.09f, 0.13f);
        window->swapBuffers();
        sgl::pollEvents();
    }

    window.reset();
    sgl::terminate();
    return 0;
}
