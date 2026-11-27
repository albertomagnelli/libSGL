# Plan: sgl – plattformunabhängige GLFW-3.4-Alternative in C++23

## Ziel

Eigenständige C++23-Bibliothek `sgl` als Ersatz für GLFW 3.4 – **ohne Vulkan**.
Keine externe GLFW-Abhängigkeit, moderne C++-API, Typen aus `include/sgl/Types.hpp`.

## Umfang

- GLFW 3.4: 124 Funktionen, davon 6 Vulkan → **118 Funktionen**.
- Backends: X11/GLX, Wayland/EGL, Win32/WGL, Cocoa/NSOpenGL, EGL, OSMesa.
- Joystick/Gamepad inkl. Hotplug und SDL-Mappings.
- Tests mit Catch2 (vendored unter `third_party/catch2`).
- Kein GLFW-Kompatibilitäts-Shim.

## API-Konventionen

- Membermethoden: `camelCase` (`swapBuffers()`, `getSize()`).
- Freie Funktionen und statische Methoden: `PascalCase` (`Init()`, `Window::Create()`).
- Typen/Klassen: `PascalCase`; Namespaces: `lower_case`.
- Membervariablen: `m_camelCase`.
- Getter (nicht bool): `getX()`; Getter (bool): `isX()`; Setter: `setX()`.
- Reine Datentypen (`WindowHints`, `VideoMode`, `GamepadState`, `Image`, `GammaRamp`): öffentliche Felder.

## Architektur

```
include/sgl/          öffentliche API
src/common/           Fehler, Init, Events, Zeit, Joystick, Window-Basis
src/backend/          Backend-Schnittstelle
src/backend/x11/      Xlib, XRandR, XInput2, Xcursor, GLX
src/backend/wayland/  libwayland, xdg-shell, xkbcommon, EGL
src/backend/win32/    Win32, WGL, Raw Input, XInput
src/backend/cocoa/    Cocoa, NSOpenGL, IOKit
src/backend/egl/      EGL-Kontexte
src/backend/osmesa/   OSMesa-Kontexte
third_party/catch2/   Catch2 (vendored)
tests/                Catch2-Tests
```

## Phasen

| Phase | Inhalt | Abnahme |
|---|---|---|
| P0 | CMake, öffentliche Header, Backend-Interface, Null-Backend, Catch2, Types-Fix | Build + Tests grün |
| P1 | X11 + GLX: Fenster, GL-4.6-Core, Events, Eingabe, Timer, getProcAddress | main.cpp läuft ohne GLFW |
| P2 | X11 vollständig: Monitore, Vollbild, Clipboard, Cursor, Icons, Opacity, Attribs, Drop, Gamma, Native Access | Tests |
| P3 | Joystick/Gamepad Linux (evdev, Hotplug, Mappings) | Tests |
| P4 | Wayland (xdg-shell, EGL, xkbcommon, Clipboard, xdg-output) | Build + manueller Test |
| P5 | Windows (Win32/WGL, DPI, Raw Input, XInput) | Build + manueller Test |
| P6 | macOS (Cocoa/NSOpenGL, max. OpenGL 4.1) | Build + manueller Test |
| P7 | EGL (GL/GLES) und OSMesa | Kontext-Tests |
| P8 | Paritäts-Audit, Doku, Sanitizer, Demo | Checkliste |

## Abhängigkeiten

| Abhängigkeit | Status | Maßnahme |
|---|---|---|
| Catch2 | fehlt | `third_party/catch2` vendoren |
| GLM | vorhanden | `find_package(glm CONFIG REQUIRED)` |
| libdecor-dev | fehlt | installieren, sonst xdg-decoration-Fallback |
| OSMesa | fehlt | installieren, sonst Backend deaktiviert |
| weston / Xvfb | fehlt | für Laufzeittests installieren |

## Definition of Done

- Kein `libglfw` mehr im Build.
- 118 Nicht-Vulkan-Funktionen implementiert oder sauber als plattformunavailable gemeldet.
- Catch2-Tests grün auf Linux/X11.
- `main.cpp` läuft mit OpenGL 4.6 über `sgl`.
- Windows/macOS-Code vollständig, ohne CI manuell zu verifizieren.

## Umsetzungsstatus

| Phase | Status |
|---|---|
| P0 – Gerüst, Header, Backend-Interface, Null-Backend, CMake, Catch2 | umgesetzt |
| P1 – X11/GLX-Fenster, GL-4.6-Kontext, Events, Eingabe, Timer | umgesetzt |
| P2 – X11-Monitore, Vollbild, Clipboard, Cursor, Icons, Opacity, Gamma, Drag & Drop | umgesetzt |
| P3 – Linux-Joystick/Gamepad über evdev mit Hotplug | umgesetzt |
| P4 – Wayland/xdg-shell/EGL/xkbcommon/Clipboard | umgesetzt, Laufzeittest offen |
| P5 – Win32/WGL/XInput | gebaut und getestet in CI |
| P6 – Cocoa/NSOpenGL/IOKit-HID | gebaut und getestet in CI |
| P7 – EGL (Wayland) und OSMesa | EGL umgesetzt; OSMesa optional |
| P8 – Paritätsaudit, Doku, Tests | umgesetzt, siehe `docs/GLFW_PARITY.md` |

### Verifikation

- Linux/X11: `cmake --build` und `ctest` grün.
- `sgl_example_gl_loader` erzeugt einen OpenGL-4.6-Core-Kontext und lädt 656/656 Funktionen.
- `ldd` enthält kein `libglfw`.
- Wayland wird gebaut, aber ohne laufenden Compositor nicht zur Laufzeit getestet.
- Windows/macOS werden in der CI gebaut und getestet.

### Bewusste Abweichungen

- macOS unterstützt nur OpenGL bis 4.1; 4.6 wird mit `VersionUnavailable` abgelehnt.
- Wayland bietet keine Fensterpositionierung und keinen Pointer-Lock.
- EGL wird für Wayland und optional für X11 verwendet; Win32 nutzt WGL, macOS NSOpenGL.
- Die SDL_GameControllerDB ist eingebettet; eigene Mappings können über
  `sgl::UpdateGamepadMappings()` ergänzt werden.
