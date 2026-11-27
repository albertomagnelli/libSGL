// Fenster-Icon zur Laufzeit setzen und wechseln.
//
// Tasten: I Icon-Set wechseln, R neu einfärben, ESC beenden.

#include "ExampleCommon.hpp"

#include <cstdio>
#include <random>
#include <vector>

namespace {

sgl::WindowIcon makeIcon(int size, u8 red, u8 green, u8 blue) {
    sgl::WindowIcon image;
    image.size = ivec2(size, size);
    image.rgba8.assign(static_cast<std::size_t>(size) * size * 4, 0);

    const float center = static_cast<float>(size - 1) * 0.5f;
    const float radius = static_cast<float>(size) * 0.46f;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const std::size_t index = (static_cast<std::size_t>(y) * size + x) * 4;
            const float dx = static_cast<float>(x) - center;
            const float dy = static_cast<float>(y) - center;
            const float distance = dx * dx + dy * dy;
            const bool inside = distance <= radius * radius;
            const bool border = distance >= (radius - 2.0f) * (radius - 2.0f);
            const bool diagonal = (x + y) / 4 % 2 == 0;

            if (inside) {
                image.rgba8[index + 0] = border ? 20 : (diagonal ? red : static_cast<u8>(red / 2));
                image.rgba8[index + 1] =
                    border ? 20 : (diagonal ? green : static_cast<u8>(green / 2));
                image.rgba8[index + 2] =
                    border ? 20 : (diagonal ? blue : static_cast<u8>(blue / 2));
                image.rgba8[index + 3] = 255;
            }
        }
    }
    return image;
}

std::vector<sgl::WindowIcon> makeIconSet(u8 red, u8 green, u8 blue) {
    std::vector<sgl::WindowIcon> icons;
    icons.push_back(makeIcon(16, red, green, blue));
    icons.push_back(makeIcon(32, red, green, blue));
    icons.push_back(makeIcon(64, red, green, blue));
    return icons;
}

} // namespace

int main() {
    auto window = example::createWindow("sgl – Icon", 800, 600);
    if (!window) {
        return 1;
    }

    std::mt19937 random{12345};
    auto randomColor = [&random]() {
        return static_cast<u8>(64 + random() % 192);
    };

    std::vector<sgl::WindowIcon> icons = makeIconSet(230, 120, 30);
    window->setIcon(icons);
    SLOGI("Icon gesetzt (%zu Größen). I wechselt das Set, R färbt neu.\n", icons.size());

    bool alternate = false;
    window->setKeyCallback([&](sgl::Key key, i32, sgl::KeyAction action, sgl::Modifiers) {
        if (action != sgl::KeyAction::Press) {
            return;
        }
        if (key == sgl::Key::Escape) {
            window->setCloseRequested(true);
        } else if (key == sgl::Key::I) {
            alternate = !alternate;
            icons = alternate ? makeIconSet(40, 170, 255) : makeIconSet(230, 120, 30);
            window->setIcon(icons);
            SLOGI("Icon-Set: %s\n", alternate ? "blau" : "orange");
        } else if (key == sgl::Key::R) {
            icons = makeIconSet(randomColor(), randomColor(), randomColor());
            window->setIcon(icons);
            SLOGI("Icon neu eingefärbt.\n");
        }
    });

    while (!window->isCloseRequested()) {
        const ivec2 framebuffer = window->getFramebufferSize();
        example::clearBackground(0.07f, 0.09f, 0.12f);
        example::drawRectTopLeft(ivec2((framebuffer.x - 64) / 2, (framebuffer.y - 64) / 2),
                                 ivec2(64, 64), framebuffer, alternate ? 0.16f : 0.90f,
                                 alternate ? 0.66f : 0.47f, alternate ? 1.0f : 0.12f);
        window->swapBuffers();
        sgl::pollEvents();
    }

    window.reset();
    sgl::terminate();
    return 0;
}
