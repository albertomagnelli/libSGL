# Fenster-Beispiele

Alle Beispiele werden mit `SGL_BUILD_EXAMPLES=ON` gebaut und liegen danach unter
`build/examples/window/`:

```bash
cmake -S . -B build -G Ninja -DSGL_BUILD_EXAMPLES=ON
cmake --build build
./build/examples/window/sgl_example_basic_window
```

| Beispiel | Ziel | Zeigt |
|---|---|---|
| `basic_window.cpp` | `sgl_example_basic_window` | Fenster, GL-Kontext, Clear-Schleife, ESC |
| `cursor.cpp` | `sgl_example_cursor` | Standard-Cursor, eigener RGBA-Cursor, Cursor-Modi |
| `frameless.cpp` | `sgl_example_frameless` | rahmenloses Fenster, eigene Titelleiste, Ziehen per Maus |
| `icon.cpp` | `sgl_example_icon` | `setIcon` mit mehreren RGBA-Größen, Icon-Wechsel |
| `fullscreen.cpp` | `sgl_example_fullscreen` | Vollbild-Umschaltung mit F11 und Monitorauswahl |
| `window_attributes.cpp` | `sgl_example_window_attributes` | Dekoration, Resizable, Floating, Maximieren, Minimieren, Opazität, Titel, Limits, Verstecken |
| `input.cpp` | `sgl_example_input` | Key-, Char-, Maus-, Scroll-, Fokus- und Cursor-Callbacks |
| `clipboard.cpp` | `sgl_example_clipboard` | `setClipboardString` / `getClipboardString` |
| `drop.cpp` | `sgl_example_drop` | Drag & Drop von Dateien |
| `monitors.cpp` | `sgl_example_monitors` | Monitore, Video-Modi, Workarea, Skalierung, Joysticks (Konsole) |
| `joystick.cpp` | `sgl_example_joystick` | Rohdaten aller Achsen, Buttons und Hats sowie Gamepad-Mapping; J/←/→ wechselt, 1–9/0 wählt direkt, P gibt den Zustand aus |

## Gemeinsamer Helfer

`examples/common/ExampleCommon.hpp` enthält:

- `example::createWindow(...)` – initialisiert sgl, erzeugt ein Fenster mit
  OpenGL-4.6-Core (Fallback 3.3) und lädt `GL::load()`.
- `example::closeOnEscape(...)`
- `example::printGLInfo()`
- `example::clearBackground(...)`
- `example::drawRectTopLeft(...)` – Rechteck per Scissor-Test, ohne Shader.
- `example::printAttribs(...)`

## Hinweise

- Die Beispiele nutzen den OpenGL-Loader aus `sgl/GL.hpp`; er ist Teil der
  `sgl`-Bibliothek.
- `fullscreen.cpp`, `frameless.cpp` und `window_attributes.cpp` funktionieren
  unter X11 am vollständigsten. Wayland erlaubt keine Fensterpositionierung;
  `frameless` verschiebt dort nicht.
- Unter macOS ist maximal OpenGL 4.1 verfügbar; der 4.6-Versuch fällt auf 3.3
  zurück. Für Beispiele, die zwingend 4.6 benötigen, ist das nicht relevant.
- `joystick.cpp` benötigt ein angeschlossenes Gamepad/Joystick; sonst zeigt es
  nur einen Hinweisrahmen. Es stellt alle Rohachsen, Rohbuttons und Hats dar.
  Y-Achsen (Rohindex 1 und 4 sowie Gamepad LY/RY) werden senkrecht visualisiert
  (negativ = oben), alle übrigen Achsen waagerecht. Unten rechts werden
  zusätzlich die 6 Gamepad-Achsen und 15 Gamepad-Buttons dargestellt, sofern
  ein Mapping vorhanden ist. Mit `P` lässt sich der vollständige Zustand
  inklusive Indizes auf der Konsole ausgeben.

  Die senkrechte Darstellung der Rohachsen 1 und 4 geht von der üblichen
  evdev-Reihenfolge `X, Y, Z, RX, RY, RZ` aus. Bei abweichender Reihenfolge
  zeigt `P` die tatsächlichen Indizes und Werte.
