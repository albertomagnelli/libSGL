// Eingabe-Callbacks: Tastatur, Text, Maus, Scrollen, Fokus, Cursor.
//
// ESC beendet das Programm. Die Events werden auf der Konsole ausgegeben.

#include "ExampleCommon.hpp"

#include <cstdio>

namespace {

const char* actionName(sgl::KeyAction action) {
    switch (action) {
    case sgl::KeyAction::Press: return "press";
    case sgl::KeyAction::Release: return "release";
    case sgl::KeyAction::Repeat: return "repeat";
    }
    return "?";
}

} // namespace

int main() {
    auto window = example::createWindow("sgl – Input", 900, 600);
    if (!window) {
        return 1;
    }

    vec2 cursorPosition = window->getCursorPos();

    window->setKeyCallback([&](sgl::Key key, i32 scancode, sgl::KeyAction action,
                               sgl::Modifiers modifiers) {
        if (key == sgl::Key::Escape && action == sgl::KeyAction::Press) {
            window->setCloseRequested(true);
        }
        const std::string name = window->getKeyName(key, scancode);
        SLOGI("key %-12s scancode=%d action=%-7s mods=0x%x\n", name.c_str(),
                    static_cast<int>(scancode), actionName(action),
                    static_cast<unsigned>(modifiers));
    });

    window->setCharCallback([](u32 codepoint) {
        SLOGI("char U+%04X\n", static_cast<unsigned>(codepoint));
    });

    window->setMouseButtonCallback(
        [](sgl::MouseButton button, sgl::KeyAction action, sgl::Modifiers modifiers) {
            SLOGI("mouse button=%d action=%-7s mods=0x%x\n", static_cast<int>(button),
                        actionName(action), static_cast<unsigned>(modifiers));
        });

    window->setCursorPosCallback([&cursorPosition](vec2 position) {
        cursorPosition = position;
    });

    window->setScrollCallback([](vec2 offset) {
        SLOGI("scroll x=%.2f y=%.2f\n", static_cast<double>(offset.x),
                    static_cast<double>(offset.y));
    });

    window->setFocusCallback([](bool focused) {
        SLOGI("focus %s\n", focused ? "true" : "false");
    });

    window->setCursorEnterCallback([](bool entered) {
        SLOGI("cursor %s\n", entered ? "entered" : "left");
    });

    window->setContentScaleCallback([](vec2 scale) {
        SLOGI("content scale %.2f x %.2f\n", static_cast<double>(scale.x),
                    static_cast<double>(scale.y));
    });

    example::printKeyHelp("Eingabe-Events werden auf der Konsole ausgegeben; ESC beendet.");

    while (!window->isCloseRequested()) {
        const ivec2 framebuffer = window->getFramebufferSize();
        example::clearBackground(0.05f, 0.06f, 0.08f);

        const ivec2 position(static_cast<i32>(cursorPosition.x), static_cast<i32>(cursorPosition.y));
        example::drawRectTopLeft(ivec2(position.x - 10, position.y - 10), ivec2(20, 20),
                                 framebuffer, 0.2f, 0.8f, 1.0f);
        example::drawRectTopLeft(ivec2(position.x - 1, 0), ivec2(2, framebuffer.y), framebuffer,
                                 0.3f, 0.35f, 0.45f);
        example::drawRectTopLeft(ivec2(0, position.y - 1), ivec2(framebuffer.x, 2), framebuffer,
                                 0.3f, 0.35f, 0.45f);

        window->swapBuffers();
        sgl::pollEvents();
    }

    window.reset();
    sgl::terminate();
    return 0;
}
