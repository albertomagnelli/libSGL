// Fenster-Attribute und Aktionen.
//
// Tasten:
//   D  Dekoration        R  resizable        F  floating
//   M  maximieren        N  minimieren       O  Opazität
//   T  Titel             A  Aufmerksamkeit   L  Größenlimits
//   V  für 1 s verstecken                    P  Attribute ausgeben
//   ESC beenden

#include "ExampleCommon.hpp"

#include <cstdio>

int main() {
    auto window = example::createWindow("sgl – Window Attributes", 900, 600);
    if (!window) {
        return 1;
    }

    bool hidden = false;
    double hiddenSince = 0.0;
    bool limitsEnabled = false;
    bool titleAlternate = false;
    bool opacityLow = false;

    example::printKeyHelp("D R F M N O T A L V P, ESC beenden");
    example::printAttribs(*window);

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
            break;
        case sgl::Key::R:
            window->setResizable(!window->isResizable());
            break;
        case sgl::Key::F:
            window->setFloating(!window->isFloating());
            break;
        case sgl::Key::M:
            window->isMaximized() ? window->restore() : window->maximize();
            break;
        case sgl::Key::N:
            window->isIconified() ? window->restore() : window->iconify();
            break;
        case sgl::Key::O:
            opacityLow = !opacityLow;
            window->setOpacity(opacityLow ? 0.5f : 1.0f);
            break;
        case sgl::Key::T:
            titleAlternate = !titleAlternate;
            window->setTitle(titleAlternate ? "sgl – geänderter Titel" : "sgl – Window Attributes");
            break;
        case sgl::Key::A:
            window->requestAttention();
            SLOGI("Aufmerksamkeit angefordert.\n");
            break;
        case sgl::Key::L:
            limitsEnabled = !limitsEnabled;
            if (limitsEnabled) {
                window->setSizeLimits(ivec2(400, 300), ivec2(1200, 800));
            } else {
                window->setSizeLimits(ivec2(0, 0), ivec2(0, 0));
            }
            SLOGI("Größenlimits: %s\n", limitsEnabled ? "400x300 … 1200x800" : "aus");
            break;
        case sgl::Key::V:
            hidden = true;
            hiddenSince = sgl::getTime();
            window->hide();
            SLOGI("Fenster für 1 Sekunde versteckt.\n");
            break;
        case sgl::Key::P:
            example::printAttribs(*window);
            break;
        default:
            break;
        }
        example::printAttribs(*window);
    });

    while (!window->isCloseRequested()) {
        if (hidden && sgl::getTime() - hiddenSince >= 1.0) {
            window->show();
            hidden = false;
            SLOGI("Fenster wieder sichtbar.\n");
        }

        const ivec2 framebuffer = window->getFramebufferSize();
        example::clearBackground(0.07f, 0.08f, 0.11f);

        const float red = window->isFloating() ? 0.95f : 0.20f;
        const float green = window->isDecorated() ? 0.65f : 0.25f;
        const float blue = window->isResizable() ? 0.35f : 0.20f;
        example::drawRectTopLeft(ivec2(40, 40), ivec2(framebuffer.x - 80, 60), framebuffer, red,
                                 green, blue);

        window->swapBuffers();
        sgl::pollEvents();
    }

    window.reset();
    sgl::terminate();
    return 0;
}
