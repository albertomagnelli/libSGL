// Cursor-Beispiel: Standard-Cursor, eigener RGBA-Cursor und Cursor-Modi.
//
// Tasten:
//   1–6  Standard-Cursor wechseln
//   7    eigener Cursor
//   0    Standard-Cursor zurücksetzen
//   C    Cursor-Modus wechseln (Normal → Hidden → Disabled)
//   ESC  beenden

#include "ExampleCommon.hpp"

#include <array>
#include <cstdio>
#include <memory>
#include <vector>

namespace {

std::unique_ptr<sgl::Cursor> createCustomCursor() {
    constexpr int size = 32;
    std::vector<u8> pixels(static_cast<std::size_t>(size) * size * 4, 0);

    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const std::size_t index = (static_cast<std::size_t>(y) * size + x) * 4;
            const bool horizontal = y >= 14 && y <= 17 && x >= 4 && x <= 27;
            const bool vertical = x >= 14 && x <= 17 && y >= 4 && y <= 27;
            const bool center = x >= 12 && x <= 19 && y >= 12 && y <= 19;
            if (center) {
                pixels[index + 0] = 255;
                pixels[index + 1] = 160;
                pixels[index + 2] = 0;
                pixels[index + 3] = 255;
            } else if (horizontal || vertical) {
                pixels[index + 0] = 255;
                pixels[index + 1] = 255;
                pixels[index + 2] = 255;
                pixels[index + 3] = 255;
            }
        }
    }

    sgl::WindowIcon image;
    image.size = ivec2(size, size);
    image.rgba8 = std::move(pixels);
    auto cursor = sgl::Cursor::create(image, ivec2(size / 2, size / 2));
    if (!cursor) {
        SLOGE("Eigener Cursor fehlgeschlagen: %s\n",
                     cursor.error().message.c_str());
        return nullptr;
    }
    return std::move(*cursor);
}

const char* modeName(sgl::CursorMode mode) {
    switch (mode) {
    case sgl::CursorMode::Normal: return "Normal";
    case sgl::CursorMode::Hidden: return "Hidden";
    case sgl::CursorMode::Disabled: return "Disabled";
    case sgl::CursorMode::Captured: return "Captured";
    }
    return "?";
}

} // namespace

int main() {
    auto window = example::createWindow("sgl – Cursor", 800, 600);
    if (!window) {
        return 1;
    }

    std::array<std::unique_ptr<sgl::Cursor>, 6> standard;
    const std::array<sgl::StandardCursor, 6> shapes = {
        sgl::StandardCursor::Arrow,   sgl::StandardCursor::IBeam,
        sgl::StandardCursor::Crosshair, sgl::StandardCursor::Hand,
        sgl::StandardCursor::HResize, sgl::StandardCursor::VResize,
    };
    for (std::size_t i = 0; i < shapes.size(); ++i) {
        auto cursor = sgl::Cursor::createStandard(shapes[i]);
        if (!cursor) {
            SLOGE("Standard-Cursor %zu fehlgeschlagen: %s\n", i,
                         cursor.error().message.c_str());
            return 1;
        }
        standard[i] = std::move(*cursor);
    }
    auto custom = createCustomCursor();

    int selected = 0;
    sgl::CursorMode mode = sgl::CursorMode::Normal;
    vec2 cursorPosition = window->getCursorPos();

    window->setCursor(standard[0].get());
    window->setCursorMode(mode);
    example::printKeyHelp("1–6 Standard-Cursor, 7 eigener Cursor, 0 zurücksetzen, C Modus, ESC "
                          "beenden");

    window->setCursorPosCallback([&cursorPosition](vec2 position) {
        cursorPosition = position;
    });

    window->setKeyCallback([&](sgl::Key key, i32, sgl::KeyAction action, sgl::Modifiers) {
        if (action != sgl::KeyAction::Press) {
            return;
        }
        if (key == sgl::Key::Escape) {
            window->setCloseRequested(true);
        } else if (key >= sgl::Key::Num1 && key <= sgl::Key::Num6) {
            selected = static_cast<int>(key) - static_cast<int>(sgl::Key::Num1);
            window->setCursor(standard[static_cast<std::size_t>(selected)].get());
            SLOGI("Standard-Cursor %d\n", selected + 1);
        } else if (key == sgl::Key::Num7 && custom) {
            selected = 6;
            window->setCursor(custom.get());
            SLOGI("Eigener Cursor\n");
        } else if (key == sgl::Key::Num0) {
            selected = 0;
            window->setCursor(static_cast<sgl::Cursor*>(nullptr));
            SLOGI("Standard-Cursor des Systems\n");
        } else if (key == sgl::Key::C) {
            if (mode == sgl::CursorMode::Normal) {
                mode = sgl::CursorMode::Hidden;
            } else if (mode == sgl::CursorMode::Hidden) {
                mode = sgl::CursorMode::Disabled;
            } else {
                mode = sgl::CursorMode::Normal;
            }
            window->setCursorMode(mode);
            SLOGI("Cursor-Modus: %s\n", modeName(mode));
        }
    });

    while (!window->isCloseRequested()) {
        const ivec2 framebuffer = window->getFramebufferSize();
        example::clearBackground(0.10f, 0.12f, 0.16f);

        const ivec2 position(static_cast<i32>(cursorPosition.x), static_cast<i32>(cursorPosition.y));
        if (position.x >= 0 && position.y >= 0 && position.x < framebuffer.x &&
            position.y < framebuffer.y) {
            example::drawRectTopLeft(ivec2(position.x - 6, position.y - 6), ivec2(12, 12),
                                     framebuffer, 1.0f, 0.55f, 0.1f);
        }

        window->swapBuffers();
        sgl::pollEvents();
    }

    window.reset();
    sgl::terminate();
    return 0;
}
