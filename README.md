# sgl – eine moderne C++23-Alternative zu GLFW 3.4

`sgl` ist eine eigenständige, plattformübergreifende Bibliothek für Fenster,
OpenGL-Kontexte, Eingabe, Monitore und Joysticks. Die API ist modernes C++23
mit RAII, `std::expected`, `std::function`, `std::span` und `std::string_view`.
Es gibt **keine** GLFW-kompatible C-Schnittstelle und keine Laufzeitabhängigkeit
von GLFW.

Der OpenGL-Loader in `GL.hpp` / `GL.cpp` verwendet `sgl::getProcAddress()` und
lädt einen OpenGL-4.6-Core-Kontext.

## Funktionsumfang

- Fensterverwaltung: Erstellen, Größe, Position, Titel, Sichtbarkeit,
  Dekoration, Vollbild, Icons, Opazität, Größenlimits, Seitenverhältnis
- OpenGL-Kontexte: GLX/EGL (X11), EGL (Wayland), WGL (Win32), NSOpenGL (macOS),
  OSMesa (optional, Null-Plattform)
- Eingabe: Tastatur, Maus, Scrollen, Zeicheneingabe, IME, Rohmausbewegung
  (X11/Win32), Sticky Keys/Buttons, Cursor-Modi
- Monitore: Aufzählung, Video-Modi, Workarea, physische Größe, DPI-Skalierung,
  Gamma-Rampen, Hotplug-Benachrichtigung
- Zwischenablage und Drag & Drop
- Joysticks/Gamepads inkl. Hotplug, Achsen, Buttons, Hats, GUIDs und
  Gamepad-Mapping (Linux evdev, Windows XInput, macOS IOKit HID)
- Zeit, Ereignisschleife, `postEmptyEvent`, Fehler-Callbacks
- Native Zugriffe für X11, Wayland, Win32, Cocoa, EGL und OSMesa

## Abhängigkeiten

| Abhängigkeit | Verwendung | Status im Referenzsystem |
|---|---|---|
| GLM | `Types.hpp` | Systempaket |
| X11, XRandR, Xinerama, Xcursor, Xi, Xext | X11-Backend | Systempaket |
| GLX / libGL | X11-Kontexte | Systempaket |
| Wayland-Client, Wayland-EGL, Wayland-Cursor, xkbcommon | Wayland-Backend | Systempaket |
| EGL | Wayland- und X11-EGL-Kontexte | Systempaket |
| wayland-protocols + wayland-scanner | Protokollcode | Systempaket |
| OSMesa | optionales OSMesa-Backend | optional |
| Catch2 3.7.1 | Tests | unter `third_party/catch2` vendort |

## Bauen

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/GLLoader
```

Optionen:

```cmake
-DSGL_BACKEND_X11=ON
-DSGL_BACKEND_WAYLAND=ON
-DSGL_BACKEND_WIN32=ON
-DSGL_BACKEND_COCOA=ON
-DSGL_BACKEND_OSMESA=ON
-DSGL_BUILD_TESTS=ON
-DSGL_BUILD_EXAMPLES=ON
```

## Minimalbeispiel

```cpp
#include <sgl/sgl.hpp>

int main() {
    if (auto result = sgl::init(); !result) {
        return 1;
    }

    sgl::WindowHints hints;
    hints.width = 1280;
    hints.height = 720;
    hints.title = "sgl";
    hints.contextVersionMajor = 4;
    hints.contextVersionMinor = 6;
    hints.contextProfile = sgl::ContextProfile::Core;
    hints.contextForwardCompat = true;

    auto windowResult = sgl::Window::create(hints);
    if (!windowResult) {
        sgl::terminate();
        return 1;
    }
    auto window = std::move(*windowResult);

    window->makeContextCurrent();
    window->setSwapInterval(1);
    window->setKeyCallback([](sgl::Key key, sgl::i32, sgl::KeyAction action, sgl::Modifiers) {
        if (key == sgl::Key::Escape && action == sgl::KeyAction::Press) {
            // ...
        }
    });

    while (!window->isCloseRequested()) {
        window->swapBuffers();
        sgl::pollEvents();
    }

    window.reset();
    sgl::terminate();
}
```

## Namenskonventionen

- Membervariablen: `m_`
- Getter: `getX()`, boolesche Getter: `isX()`
- Setter: `setX()`
- Aktionen: Verb (`swapBuffers()`, `pollEvents()`)
- Reine Datentypen (`WindowHints`, `VideoMode`, `GamepadState`, `Image`,
  `GammaRamp`) haben öffentliche Felder.

## Plattformhinweise

- **X11** ist die Referenzplattform und wird lokal vollständig gebaut und
  getestet.
- **Wayland** wird gebaut; Laufzeittests benötigen einen Compositor
  (z. B. Weston). Pointer-Lock, Rohmausbewegung und Fensterpositionierung sind
  durch Wayland nicht verfügbar und werden als `FeatureUnavailable` gemeldet.
- **Windows** und **macOS** sind code-complete, konnten in dieser Umgebung
  aber nicht kompiliert oder getestet werden.
- **macOS** unterstützt maximal OpenGL 4.1. Ein 4.6-Kontext schlägt dort
  erwartungsgemäß mit `VersionUnavailable` fehl.
- **EGL** wird für Wayland und optional für X11 verwendet; X11 kann auch GLX,
  Win32 nutzt WGL, macOS NSOpenGL.
- **OSMesa** wird nur gebaut, wenn `libosmesa` per pkg-config gefunden wird.

## Tests

Die Catch2-Tests decken Fehlerbehandlung, Hints, Zeit, Null-Plattform,
Initialisierungsfehler und – bei gesetztem `DISPLAY` – X11-Fenster und
Monitore ab.

```bash
ctest --test-dir build --output-on-failure
```

## Lizenzhinweise

Catch2 steht unter BSL-1.0, GLM unter MIT. Die Lizenztexte liegen unter
`third_party/catch2/LICENSE.txt` bzw. werden über das Systempaket bezogen.
