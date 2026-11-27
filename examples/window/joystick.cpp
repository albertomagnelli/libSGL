// Joystick/Gamepad-Test: alle Achsen, Buttons und Hats visualisieren.
//
// Tasten:
//   J / → / ←   Joystick wechseln
//   1–9, 0      Joystick 0–9 direkt wählen
//   P           vollständigen Zustand auf der Konsole ausgeben
//   ESC         beenden
//
// Der obere Bereich zeigt die Rohdaten des Joysticks (alle Achsen, Buttons,
// Hats). Y-Achsen (Index 1 und 4) werden senkrecht dargestellt, alle anderen
// waagerecht. Es gilt die GLFW/SDL-Konvention: negativ = oben, positiv = unten.
// Unten rechts wird – falls vorhanden – die Gamepad-Zuordnung mit allen 6
// Achsen und 15 Buttons dargestellt; LY und RY sind senkrecht.

#include "ExampleCommon.hpp"

#include <algorithm>
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr int kButtonCell = 26;
constexpr int kButtonGap = 6;
constexpr int kGamepadCell = 30;

bool hatHas(sgl::JoystickHat value, sgl::JoystickHat flag) {
    return (static_cast<int>(value) & static_cast<int>(flag)) != 0;
}

std::string hatName(sgl::JoystickHat hat) {
    switch (hat) {
    case sgl::JoystickHat::Centered: return "Centered";
    case sgl::JoystickHat::Up: return "Up";
    case sgl::JoystickHat::Right: return "Right";
    case sgl::JoystickHat::Down: return "Down";
    case sgl::JoystickHat::Left: return "Left";
    case sgl::JoystickHat::RightUp: return "RightUp";
    case sgl::JoystickHat::RightDown: return "RightDown";
    case sgl::JoystickHat::LeftDown: return "LeftDown";
    case sgl::JoystickHat::LeftUp: return "LeftUp";
    }
    return "?";
}

const char* gamepadButtonName(sgl::GamepadButton button) {
    switch (button) {
    case sgl::GamepadButton::A: return "A";
    case sgl::GamepadButton::B: return "B";
    case sgl::GamepadButton::X: return "X";
    case sgl::GamepadButton::Y: return "Y";
    case sgl::GamepadButton::LeftBumper: return "LB";
    case sgl::GamepadButton::RightBumper: return "RB";
    case sgl::GamepadButton::Back: return "Back";
    case sgl::GamepadButton::Start: return "Start";
    case sgl::GamepadButton::Guide: return "Guide";
    case sgl::GamepadButton::LeftThumb: return "LS";
    case sgl::GamepadButton::RightThumb: return "RS";
    case sgl::GamepadButton::DPadUp: return "DPadUp";
    case sgl::GamepadButton::DPadRight: return "DPadRight";
    case sgl::GamepadButton::DPadDown: return "DPadDown";
    case sgl::GamepadButton::DPadLeft: return "DPadLeft";
    case sgl::GamepadButton::Count: break;
    }
    return "?";
}

const char* axisName(sgl::GamepadAxis axis) {
    switch (axis) {
    case sgl::GamepadAxis::LeftX: return "LX";
    case sgl::GamepadAxis::LeftY: return "LY";
    case sgl::GamepadAxis::RightX: return "RX";
    case sgl::GamepadAxis::RightY: return "RY";
    case sgl::GamepadAxis::LeftTrigger: return "LT";
    case sgl::GamepadAxis::RightTrigger: return "RT";
    case sgl::GamepadAxis::Count: break;
    }
    return "?";
}

void drawAxisBar(ivec2 origin, ivec2 size, ivec2 framebuffer, f32 value, float red, float green,
                 float blue) {
    example::drawRectTopLeft(origin, size, framebuffer, 0.13f, 0.15f, 0.20f);

    const int centerX = origin.x + size.x / 2;
    example::drawRectTopLeft(ivec2(centerX - 1, origin.y), ivec2(2, size.y), framebuffer, 0.40f,
                             0.44f, 0.52f);

    const f32 clamped = std::clamp(value, -1.0f, 1.0f);
    const int travel = std::max(size.x / 2 - 7, 1);
    const int markerX = centerX + static_cast<int>(clamped * static_cast<f32>(travel));
    example::drawRectTopLeft(ivec2(markerX - 4, origin.y - 2), ivec2(8, size.y + 4), framebuffer,
                             red, green, blue);
}

void drawAxisBarVertical(ivec2 origin, ivec2 size, ivec2 framebuffer, f32 value, float red,
                         float green, float blue) {
    example::drawRectTopLeft(origin, size, framebuffer, 0.13f, 0.15f, 0.20f);

    const int centerY = origin.y + size.y / 2;
    example::drawRectTopLeft(ivec2(origin.x, centerY - 1), ivec2(size.x, 2), framebuffer, 0.40f,
                             0.44f, 0.52f);

    const f32 clamped = std::clamp(value, -1.0f, 1.0f);
    const int travel = std::max(size.y / 2 - 7, 1);
    // GLFW/SDL-Konvention: negativ = oben, positiv = unten.
    const int markerY = centerY + static_cast<int>(clamped * static_cast<f32>(travel));
    example::drawRectTopLeft(ivec2(origin.x - 2, markerY - 4), ivec2(size.x + 4, 8), framebuffer,
                             red, green, blue);
}

void drawButton(ivec2 origin, int cell, ivec2 framebuffer, bool pressed, float red, float green,
                float blue) {
    example::drawRectTopLeft(origin, ivec2(cell, cell), framebuffer, 0.34f, 0.38f, 0.46f);
    example::drawRectTopLeft(ivec2(origin.x + 2, origin.y + 2), ivec2(cell - 4, cell - 4),
                             framebuffer, pressed ? red : 0.10f, pressed ? green : 0.12f,
                             pressed ? blue : 0.16f);
}

void drawHat(ivec2 origin, int cell, ivec2 framebuffer, sgl::JoystickHat hat) {
    example::drawRectTopLeft(ivec2(origin.x, origin.y), ivec2(cell * 3, cell * 3), framebuffer,
                             0.10f, 0.12f, 0.16f);
    example::drawRectTopLeft(ivec2(origin.x + cell, origin.y + cell), ivec2(cell, cell),
                             framebuffer, 0.30f, 0.34f, 0.42f);

    const bool up = hatHas(hat, sgl::JoystickHat::Up);
    const bool right = hatHas(hat, sgl::JoystickHat::Right);
    const bool down = hatHas(hat, sgl::JoystickHat::Down);
    const bool left = hatHas(hat, sgl::JoystickHat::Left);

    if (up) {
        example::drawRectTopLeft(ivec2(origin.x + cell, origin.y), ivec2(cell, cell), framebuffer,
                                 1.0f, 0.55f, 0.1f);
    }
    if (right) {
        example::drawRectTopLeft(ivec2(origin.x + cell * 2, origin.y + cell), ivec2(cell, cell),
                                 framebuffer, 1.0f, 0.55f, 0.1f);
    }
    if (down) {
        example::drawRectTopLeft(ivec2(origin.x + cell, origin.y + cell * 2), ivec2(cell, cell),
                                 framebuffer, 1.0f, 0.55f, 0.1f);
    }
    if (left) {
        example::drawRectTopLeft(ivec2(origin.x, origin.y + cell), ivec2(cell, cell), framebuffer,
                                 1.0f, 0.55f, 0.1f);
    }
}

i32 firstPresent() {
    for (i32 jid = 0; jid < 16; ++jid) {
        if (sgl::isJoystickPresent(jid)) {
            return jid;
        }
    }
    return -1;
}

i32 nextPresent(i32 start, int direction) {
    for (int step = 1; step <= 16; ++step) {
        const i32 jid = (start + direction * step + 16) % 16;
        if (sgl::isJoystickPresent(jid)) {
            return jid;
        }
    }
    return -1;
}

void printState(i32 jid) {
    if (jid < 0 || !sgl::isJoystickPresent(jid)) {
        SLOGI("Kein Joystick ausgewählt.");
        return;
    }

    const auto axes = sgl::getJoystickAxes(jid);
    const auto buttons = sgl::getJoystickButtons(jid);
    const auto hats = sgl::getJoystickHats(jid);

    std::ostringstream out;
    out << "Joystick " << jid << " '" << sgl::getJoystickName(jid) << "'  GUID="
        << sgl::getJoystickGUID(jid) << '\n';

    out << "  Achsen (" << axes.size() << "):\n";
    out << std::showpos << std::fixed << std::setprecision(3);
    for (std::size_t i = 0; i < axes.size(); ++i) {
        out << "    [" << std::setw(2) << i << "] " << axes[i];
        if ((i + 1) % 4 == 0) {
            out << '\n';
        } else {
            out << "  ";
        }
    }
    if (axes.size() % 4 != 0) {
        out << '\n';
    }
    out << std::noshowpos;
    out << "  (Y-Achsen: negativ = oben, positiv = unten)\n";

    out << "  Buttons (" << buttons.size() << "):\n";
    for (std::size_t i = 0; i < buttons.size(); ++i) {
        out << "    [" << std::setw(2) << i << "]=" << (buttons[i] != 0 ? 1 : 0);
        if ((i + 1) % 8 == 0) {
            out << '\n';
        } else {
            out << "  ";
        }
    }
    if (buttons.size() % 8 != 0) {
        out << '\n';
    }

    out << "  Hats (" << hats.size() << "):";
    for (std::size_t i = 0; i < hats.size(); ++i) {
        out << " [" << i << "]=" << hatName(hats[i]);
    }
    out << '\n';

    if (const auto gamepad = sgl::getGamepadState(jid)) {
        out << "  Gamepad '" << sgl::getGamepadName(jid) << "':\n";
        out << std::showpos << std::fixed << std::setprecision(3);
        for (int i = 0; i < static_cast<int>(sgl::GamepadAxis::Count); ++i) {
            out << "    " << std::setw(2) << axisName(static_cast<sgl::GamepadAxis>(i)) << " = "
                << gamepad->axes[static_cast<std::size_t>(i)];
            if ((i + 1) % 3 == 0) {
                out << '\n';
            } else {
                out << "  ";
            }
        }
        out << std::noshowpos << "    Buttons:";
        for (int i = 0; i < static_cast<int>(sgl::GamepadButton::Count); ++i) {
            if (gamepad->buttons[static_cast<std::size_t>(i)] != 0) {
                out << ' ' << gamepadButtonName(static_cast<sgl::GamepadButton>(i));
            }
        }
        out << '\n';
    } else {
        out << "  Kein Gamepad-Mapping.\n";
    }

    SLOGI << out.str();
}

} // namespace

int main() {
    auto window = example::createWindow("sgl – Joystick-Test", 1120, 820);
    if (!window) {
        return 1;
    }

    sgl::pollEvents();

    i32 selected = firstPresent();
    if (selected < 0) {
        selected = 0;
    }

    auto updateTitle = [&]() {
        if (sgl::isJoystickPresent(selected)) {
            const auto axes = sgl::getJoystickAxes(selected);
            const auto buttons = sgl::getJoystickButtons(selected);
            const auto hats = sgl::getJoystickHats(selected);
            window->setTitle("sgl – Joystick " + std::to_string(selected) + ": " +
                             sgl::getJoystickName(selected) + " (" + std::to_string(axes.size()) +
                             " Achsen, " + std::to_string(buttons.size()) + " Buttons, " +
                             std::to_string(hats.size()) + " Hats)");
        } else {
            window->setTitle("sgl – Joystick-Test (kein Gerät)");
        }
    };

    auto select = [&](i32 jid) {
        if (jid < 0) {
            jid = firstPresent();
        }
        if (jid >= 0) {
            selected = jid;
            updateTitle();
            SLOGI("Ausgewählt: Joystick %d '%s'\n", static_cast<int>(selected),
                        sgl::getJoystickName(selected).c_str());
            printState(selected);
        }
    };

    updateTitle();
    example::printKeyHelp(
        "J/←/→ Joystick wechseln, 1–9/0 direkt wählen, P Zustand ausgeben, ESC beenden.");
    printState(selected);

    sgl::setJoystickCallback([&](i32 jid, bool connected) {
        SLOGI("Hotplug: Joystick %d %s\n", static_cast<int>(jid),
                    connected ? "verbunden" : "getrennt");
        if (connected && !sgl::isJoystickPresent(selected)) {
            select(firstPresent());
        } else if (!connected && jid == selected) {
            select(firstPresent());
        } else {
            updateTitle();
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
        case sgl::Key::J:
            select(nextPresent(selected, 1));
            break;
        case sgl::Key::Right:
            select(nextPresent(selected, 1));
            break;
        case sgl::Key::Left:
            select(nextPresent(selected, -1));
            break;
        case sgl::Key::P:
            printState(selected);
            break;
        default:
            if (key >= sgl::Key::Num1 && key <= sgl::Key::Num9) {
                select(static_cast<i32>(key) - static_cast<i32>(sgl::Key::Num1));
            } else if (key == sgl::Key::Num0) {
                select(9);
            }
            break;
        }
    });

    while (!window->isCloseRequested()) {
        sgl::pollEvents();

        const ivec2 framebuffer = window->getFramebufferSize();
        example::clearBackground(0.05f, 0.06f, 0.09f);

        if (!sgl::isJoystickPresent(selected)) {
            example::drawRectTopLeft(ivec2(framebuffer.x / 2 - 260, framebuffer.y / 2 - 40),
                                     ivec2(520, 80), framebuffer, 0.16f, 0.08f, 0.08f);
            window->swapBuffers();
            continue;
        }

        const auto axes = sgl::getJoystickAxes(selected);
        const auto buttons = sgl::getJoystickButtons(selected);
        const auto hats = sgl::getJoystickHats(selected);
        const auto gamepad = sgl::getGamepadState(selected);

        // ------------------------------------------------------------------
        // Rohdaten: alle Achsen. Y-Achsen (Index 1 und 4) werden senkrecht
        // dargestellt, alle anderen waagerecht. Bis zu 12 Achsen.
        // ------------------------------------------------------------------
        constexpr int axisCellWidth = 160;
        constexpr int axisCellHeight = 100;
        const int displayAxes = std::min<int>(static_cast<int>(axes.size()), 12);
        const int rawRows = std::max(1, (displayAxes + 2) / 3);
        for (int i = 0; i < displayAxes; ++i) {
            const int column = i % 3;
            const int row = i / 3;
            const ivec2 cell(24 + column * axisCellWidth, 64 + row * axisCellHeight);
            const f32 value = axes[static_cast<std::size_t>(i)];
            const float red = value >= 0.0f ? 1.0f : 0.25f;
            const float green = value >= 0.0f ? 0.55f : 0.65f;
            const float blue = value >= 0.0f ? 0.10f : 1.0f;

            if (i == 1 || i == 4) {
                // Y-Achse: senkrechter Balken, positiv = oben.
                drawAxisBarVertical(ivec2(cell.x + 72, cell.y + 10), ivec2(16, 80), framebuffer,
                                    value, red, green, blue);
            } else {
                drawAxisBar(ivec2(cell.x + 15, cell.y + 42), ivec2(130, 16), framebuffer, value,
                            red, green, blue);
            }
        }

        // ------------------------------------------------------------------
        // Rohdaten: alle Buttons (bis zu 48)
        // ------------------------------------------------------------------
        constexpr int rawButtonColumns = 16;
        for (std::size_t i = 0; i < buttons.size() && i < 48; ++i) {
            const int column = static_cast<int>(i) % rawButtonColumns;
            const int row = static_cast<int>(i) / rawButtonColumns;
            const ivec2 origin(520 + column * (kButtonCell + kButtonGap),
                               64 + row * (kButtonCell + kButtonGap));
            drawButton(origin, kButtonCell, framebuffer, buttons[i] != 0, 0.2f, 0.9f, 0.4f);
        }

        // ------------------------------------------------------------------
        // Rohdaten: Hats
        // ------------------------------------------------------------------
        for (std::size_t i = 0; i < hats.size() && i < 4; ++i) {
            drawHat(ivec2(520 + static_cast<int>(i) * 92, 180), 20, framebuffer, hats[i]);
        }

        // ------------------------------------------------------------------
        // Gamepad-Zuordnung
        // ------------------------------------------------------------------
        const int gamepadY = 64 + rawRows * axisCellHeight + 24;
        if (gamepad) {
            // LX/RX waagerecht, LY/RY senkrecht, Trigger waagerecht.
            auto gamepadAxis = [&](sgl::GamepadAxis axis) {
                return gamepad->axes[static_cast<std::size_t>(axis)];
            };
            drawAxisBar(ivec2(24, gamepadY + 50), ivec2(150, 16), framebuffer,
                        gamepadAxis(sgl::GamepadAxis::LeftX), 0.2f, 0.65f, 1.0f);
            drawAxisBarVertical(ivec2(200, gamepadY), ivec2(16, 100), framebuffer,
                                gamepadAxis(sgl::GamepadAxis::LeftY), 0.2f, 0.65f, 1.0f);
            drawAxisBar(ivec2(240, gamepadY + 50), ivec2(150, 16), framebuffer,
                        gamepadAxis(sgl::GamepadAxis::RightX), 0.2f, 0.65f, 1.0f);
            drawAxisBarVertical(ivec2(416, gamepadY), ivec2(16, 100), framebuffer,
                                gamepadAxis(sgl::GamepadAxis::RightY), 0.2f, 0.65f, 1.0f);
            drawAxisBar(ivec2(24, gamepadY + 120), ivec2(150, 16), framebuffer,
                        gamepadAxis(sgl::GamepadAxis::LeftTrigger), 0.2f, 0.65f, 1.0f);
            drawAxisBar(ivec2(240, gamepadY + 120), ivec2(150, 16), framebuffer,
                        gamepadAxis(sgl::GamepadAxis::RightTrigger), 0.2f, 0.65f, 1.0f);

            const ivec2 base(600, gamepadY);
            auto pressed = [&](sgl::GamepadButton button) {
                return gamepad->buttons[static_cast<std::size_t>(button)] != 0;
            };
            auto draw = [&](sgl::GamepadButton button, ivec2 position, float red, float green,
                            float blue) {
                drawButton(position, kGamepadCell, framebuffer, pressed(button), red, green, blue);
            };

            // Schultertasten
            draw(sgl::GamepadButton::LeftBumper, ivec2(base.x, base.y), 0.9f, 0.8f, 0.2f);
            draw(sgl::GamepadButton::RightBumper, ivec2(base.x + 260, base.y), 0.9f, 0.8f, 0.2f);

            // Back / Guide / Start
            draw(sgl::GamepadButton::Back, ivec2(base.x, base.y + 50), 0.7f, 0.7f, 0.9f);
            draw(sgl::GamepadButton::Guide, ivec2(base.x + 110, base.y + 50), 0.8f, 0.8f, 0.8f);
            draw(sgl::GamepadButton::Start, ivec2(base.x + 220, base.y + 50), 0.7f, 0.7f, 0.9f);

            // D-Pad
            draw(sgl::GamepadButton::DPadUp, ivec2(base.x + 45, base.y + 100), 0.9f, 0.4f, 0.2f);
            draw(sgl::GamepadButton::DPadLeft, ivec2(base.x, base.y + 145), 0.9f, 0.4f, 0.2f);
            draw(sgl::GamepadButton::DPadRight, ivec2(base.x + 90, base.y + 145), 0.9f, 0.4f, 0.2f);
            draw(sgl::GamepadButton::DPadDown, ivec2(base.x + 45, base.y + 190), 0.9f, 0.4f, 0.2f);

            // Face-Buttons
            draw(sgl::GamepadButton::Y, ivec2(base.x + 270, base.y + 95), 0.9f, 0.9f, 0.2f);
            draw(sgl::GamepadButton::X, ivec2(base.x + 235, base.y + 130), 0.2f, 0.5f, 1.0f);
            draw(sgl::GamepadButton::B, ivec2(base.x + 305, base.y + 130), 1.0f, 0.3f, 0.3f);
            draw(sgl::GamepadButton::A, ivec2(base.x + 270, base.y + 165), 0.2f, 1.0f, 0.4f);

            // Stick-Klicks
            draw(sgl::GamepadButton::LeftThumb, ivec2(base.x + 45, base.y + 230), 0.4f, 0.9f, 0.9f);
            draw(sgl::GamepadButton::RightThumb, ivec2(base.x + 220, base.y + 230), 0.4f, 0.9f,
                 0.9f);
        }

        window->swapBuffers();
    }

    window.reset();
    sgl::terminate();
    return 0;
}
