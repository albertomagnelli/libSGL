// Rahmenloses Fenster mit eigener Titelleiste und Verschieben per Maus.
//
// Tasten:
//   D    Dekoration ein/aus
//   F    immer im Vordergrund
//   M    maximieren/wiederherstellen
//   ESC  beenden
//
// Maus: linke Taste auf der oberen Leiste ziehen.

#include "ExampleCommon.hpp"

#include <cstdio>

int main() {
    sgl::WindowHints hints;
    hints.decorated = false;
    hints.resizable = true;

    auto window = example::CreateWindow("sgl – Frameless", 900, 600, hints);
    if (!window) {
        return 1;
    }

    bool dragging = false;
    ivec2 dragWindowStart{0, 0};
    vec2 dragCursorStart{0.0f, 0.0f};
    vec2 cursorPosition = window->getCursorPos();

    example::PrintKeyHelp("D Dekoration, F Floating, M Maximieren, ESC beenden; "
                          "obere Leiste mit linker Maustaste ziehen");

    window->setCursorPosCallback([&](vec2 position) {
        cursorPosition = position;
    });

    window->setMouseButtonCallback(
        [&](sgl::MouseButton button, sgl::KeyAction action, sgl::Modifiers) {
            if (button != sgl::MouseButton::Left) {
                return;
            }
            if (action == sgl::KeyAction::Press && cursorPosition.y < 40.0f) {
                dragging = true;
                dragWindowStart = window->getPosition();
                dragCursorStart = cursorPosition;
            } else if (action == sgl::KeyAction::Release) {
                dragging = false;
            }
        });

    window->setKeyCallback([&](sgl::Key key, i32, sgl::KeyAction action, sgl::Modifiers) {
        if (action != sgl::KeyAction::Press) {
            return;
        }
        switch (key) {
        case sgl::Key::Escape:
            window->setCloseRequested(true);
            break;
        case sgl::Key::D:
            window->setDecorated(!window->isDecorated());
            SLOGI("Dekoration: %s\n", window->isDecorated() ? "an" : "aus");
            break;
        case sgl::Key::F:
            window->setFloating(!window->isFloating());
            SLOGI("Floating: %s\n", window->isFloating() ? "an" : "aus");
            break;
        case sgl::Key::M:
            window->isMaximized() ? window->restore() : window->maximize();
            break;
        default:
            break;
        }
    });

    while (!window->isCloseRequested()) {
        if (dragging) {
            const vec2 delta = cursorPosition - dragCursorStart;
            const ivec2 target(static_cast<i32>(static_cast<f32>(dragWindowStart.x) + delta.x),
                               static_cast<i32>(static_cast<f32>(dragWindowStart.y) + delta.y));
            window->setPosition(target);
        }

        const ivec2 framebuffer = window->getFramebufferSize();
        example::ClearBackground(0.08f, 0.10f, 0.13f);

        // Titelleiste
        example::DrawRectTopLeft(ivec2(0, 0), ivec2(framebuffer.x, 38), framebuffer, 0.16f,
                                 0.20f, 0.28f);
        // "Fensterknopf"
        example::DrawRectTopLeft(ivec2(10, 10), ivec2(18, 18), framebuffer, 1.0f, 0.55f, 0.1f);

        // Cursor-Punkt
        const ivec2 position(static_cast<i32>(cursorPosition.x), static_cast<i32>(cursorPosition.y));
        example::DrawRectTopLeft(ivec2(position.x - 3, position.y - 3), ivec2(6, 6), framebuffer,
                                 0.9f, 0.95f, 1.0f);

        window->swapBuffers();
        sgl::PollEvents();
    }

    window.reset();
    sgl::Terminate();
    return 0;
}
