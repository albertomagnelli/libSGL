// Konsolenbeispiel: Monitore, Video-Modi und Joysticks auflisten.

#include <sgl/sgl.hpp>

#include <cstdio>

int main() {
    sgl::SetErrorCallback([](const sgl::Error& error) {
        SLOGE("sgl-Fehler %d: %s\n", static_cast<int>(error.code),
                     error.message.c_str());
    });

    if (auto result = sgl::Init(); !result) {
        SLOGE("sgl::Init() fehlgeschlagen: %s\n", result.error().message.c_str());
        return 1;
    }

    sgl::PollEvents();

    SLOGI("Plattform: %s\n", sgl::GetPlatform() == sgl::Platform::X11     ? "X11"
                                   : sgl::GetPlatform() == sgl::Platform::Wayland ? "Wayland"
                                   : sgl::GetPlatform() == sgl::Platform::Win32   ? "Win32"
                                   : sgl::GetPlatform() == sgl::Platform::Cocoa   ? "Cocoa"
                                                                                  : "Null");

    const auto monitors = sgl::GetMonitors();
    const sgl::Monitor primary = sgl::GetPrimaryMonitor();

    SLOGI("%zu Monitor(e)", monitors.size());
    for (std::size_t i = 0; i < monitors.size(); ++i) {
        const sgl::Monitor& monitor = monitors[i];
        const sgl::VideoMode mode = monitor.getVideoMode();
        const sgl::Monitor::Workarea workarea = monitor.getWorkarea();
        const ivec2 physical = monitor.getPhysicalSize();
        const vec2 scale = monitor.getContentScale();
        const auto modes = monitor.getVideoModes();

        SLOGI("  [%zu] %s%s\n", i, monitor.getName().c_str(),
                    monitor == primary ? " (primär)" : "");
        SLOGI("       Position: %d,%d  Größe: %dx%d  Workarea: %d,%d %dx%d\n",
                    monitor.getPosition().x, monitor.getPosition().y, mode.size.x, mode.size.y,
                    workarea.position.x, workarea.position.y, workarea.size.x, workarea.size.y);
        SLOGI("       Physikalisch: %dx%d mm  Skalierung: %.2f x %.2f\n", physical.x,
                    physical.y, static_cast<double>(scale.x), static_cast<double>(scale.y));
        SLOGI("       Aktueller Modus: %dx%d @ %d Hz  (%zu Video-Modusse)\n", mode.size.x,
                    mode.size.y, mode.refreshRate, modes.size());
        SLOGI("       Verbunden: %s  Gamma-Einträge: 256\n",
                    monitor.isConnected() ? "ja" : "nein");
    }

    SLOGI("Joysticks/Gamepads:");
    bool anyJoystick = false;
    for (i32 index = 0; index < 16; ++index) {
        if (!sgl::IsJoystickPresent(index)) {
            continue;
        }
        anyJoystick = true;
        const auto axes = sgl::GetJoystickAxes(index);
        const auto buttons = sgl::GetJoystickButtons(index);
        const auto hats = sgl::GetJoystickHats(index);

        SLOGI("  [%d] %s\n", index, sgl::GetJoystickName(index).c_str());
        SLOGI("       GUID: %s\n", sgl::GetJoystickGuid(index).c_str());
        SLOGI("       Achsen: %zu  Buttons: %zu  Hats: %zu  Gamepad: %s\n", axes.size(),
                    buttons.size(), hats.size(), sgl::IsGamepad(index) ? "ja" : "nein");
        if (const auto state = sgl::GetGamepadState(index)) {
            SLOGI("       Links: %.2f,%.2f  Rechts: %.2f,%.2f  Trigger: %.2f,%.2f\n",
                        static_cast<double>(state->axes[static_cast<std::size_t>(
                            sgl::GamepadAxis::LeftX)]),
                        static_cast<double>(state->axes[static_cast<std::size_t>(
                            sgl::GamepadAxis::LeftY)]),
                        static_cast<double>(state->axes[static_cast<std::size_t>(
                            sgl::GamepadAxis::RightX)]),
                        static_cast<double>(state->axes[static_cast<std::size_t>(
                            sgl::GamepadAxis::RightY)]),
                        static_cast<double>(state->axes[static_cast<std::size_t>(
                            sgl::GamepadAxis::LeftTrigger)]),
                        static_cast<double>(state->axes[static_cast<std::size_t>(
                            sgl::GamepadAxis::RightTrigger)]));
        }
    }
    if (!anyJoystick) {
        SLOGI("  (keine gefunden)\n");
    }

    sgl::Terminate();
    return 0;
}
