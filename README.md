# sgl – eine moderne C++23-Alternative zu GLFW 3.4

[![CI](https://github.com/albertomagnelli/libSGL/actions/workflows/ci.yml/badge.svg)](https://github.com/albertomagnelli/libSGL/actions/workflows/ci.yml)

`sgl` ist eine eigenständige, plattformübergreifende Bibliothek für Fenster,
OpenGL-Kontexte, Eingabe, Monitore und Joysticks. Die API ist modernes C++23
mit RAII, `std::expected`, `std::function`, `std::span` und `std::string_view`.
Es gibt **keine** GLFW-kompatible C-Schnittstelle und keine Laufzeitabhängigkeit
von GLFW.

Der OpenGL-Loader in `include/sgl/GL.hpp` / `src/gl/GL.cpp` verwendet `sgl::GetProcAddress()` und
lädt einen OpenGL-4.6-Core-Kontext.

## Namenskonventionen

- Klassenmember-Methoden: `camelCase`, z. B. `Window::getSize()`, `Material::setAlbedo()`
- Freie Funktionen: `PascalCase`, z. B. `sgl::Init()`, `sgl::GetTime()`
- Statische Klassenmethoden: `PascalCase`, z. B. `Window::Create()`, `ImageIO::Load()`
- Typen und Klassen: `PascalCase`, z. B. `Window`, `ImageFormat`
- Namespaces: `lower_case`, z. B. `sgl`, `sgl::gl`
- Membervariablen: `m_camelCase`
- Getter: `getX()` bzw. `isX()`/`hasX()`; Setter: `setX()`
- Reine Datentypen und Aggregates (`WindowHints`, `VideoMode`, `GammaRamp`,
  `GamepadState`, `Color`, `Rectangle`, `TextureOptions`, `FramebufferOptions`,
  `NoiseConfig`, `FontMetrics`, `Glyph`, `ModelNode`, `ModelSkin`,
  `AnimationSampler`, `AnimationChannel`, `AnimationFrame`, `Animation`,
  `GlyphMesh`) haben öffentliche Felder ohne Präfix. Klassen mit eigenem
  Zustand (z. B. `Image`, `Mesh`, `Model`) kapseln ihn hinter Getter/Settern.

Die Regeln sind in `.clang-tidy` hinterlegt (`readability-identifier-naming`)
und lassen sich damit prüfen. `.clang-format` beschreibt den Formatierstil.
Vendorte Fremdbibliotheken (`cgltf_*`, `ufbx_*`, `tinyobj*`, `mapbox*`,
`ma_*`, `qoa_*`, `jar_xm_*`) behalten ihre Originalnamen.

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
  Gamepad-Mapping auf Basis der eingebetteten SDL_GameControllerDB
  (Linux evdev, Windows XInput, macOS IOKit HID)
- Bildverarbeitung: PNG/JPG/BMP/TGA laden und speichern, Spiegeln, Drehen,
  Skalieren, Formatkonvertierung (`sgl::Image`); Datei-I/O über `sgl::ImageIO`
- 2D-Sprites: Textur, Atlas-Ausschnitt, Tint, Rotation und pixelgenaue
  Kollision (`sgl::Sprite`); animierte Atlanten mit benannten Animationen,
  Tempo und Kollision pro Rahmen (`sgl::AnimatedSprite`)
- Kollisionsformen: Deckungsmasken aus dem Alphakanal mit Rotation und
  pixelgenauen Überschneidungstests (`sgl::CollisionShape`)
- Framebuffer: Farb-/Tiefe-/Stencil-Anhänge, MRT, MSAA und Blit/Resolve
  (`sgl::Framebuffer`)
- Audio: WAV/FLAC/MP3/OGG/QOA dekodieren, WAV/QOA schreiben, Effekte mit
  mehreren Stimmen, gestreamte Musik und eigene PCM-Ströme; XM-Module werden
  für `Music` nach PCM gerendert (`sgl::Wave`, `sgl::Sound`, `sgl::Music`,
  `sgl::AudioStream`)
- Schriften: Bitmap-/SDF-Glyphenatlas, eingebettete DejaVu-Standardschrift,
  UTF-8, Kerning und Textvermessung (`sgl::Font`)
- Mesh-Schriften: 2D- und 3D-Glyphennetze aus TrueType-Dateien (`sgl::Font2d`,
  `sgl::Font3d`)
- Shader: GLSL-Programme aus Dateien/Quelltext, Includes, Stufen, Uniform-Cache,
  ScopedBind und Fehlerdiagnose (`sgl::Shader`)
- Texturen: `sgl::Texture2d` und `sgl::CubeMap` über Direct State Access
- Farben: RGB/HSV/HSL/HWB/CMYK, Hex, Mischen, Helligkeit und Alpha-Operationen
  (`sgl::Color`)
- Geometrie: `sgl::Rectangle` mit Kollision, Schnittmengen und Rotation
- PBR-Materialien: Albedo, Metallic, Roughness, AO, Normal, Height und Emissive
  als `sgl::Image`-Maps, inkl. 1x1-Bildern für konstante Werte (`sgl::Material`)
- Modelle: glTF/GLB, FBX und OBJ laden mit Meshes, Materialien, Nodes und
  (bei glTF/FBX) Skins und Animationen ohne OpenGL-Bindung (`sgl::Model`,
  `sgl::ModelIO`, `sgl::ModelAnimationIO`). OBJ unterstuetzt MTL-Materialien,
  Texturmaps, Smoothing-Gruppen und Triangulierung; Animationen gibt es dort
  nicht.
- Noise: Perlin-/Simplex-Rauschtexturen per Compute-Shader, 1D/2D/3D
  (`sgl::Noise`)
- Zeit, Ereignisschleife, `postEmptyEvent`, Fehler-Callbacks
- Leveleditor: Java-25-/JavaFX-25-Anwendung mit FXML, CDI (Weld SE),
  andockbaren und schwebenden Panels, Projektverwaltung, Kachel-/Objekt-Layer,
  Entities mit Attributen, Undo/Redo, Kachelanimationen, Tiled-Import/Export
  (TMJ/TMX, Templates, Wang-Metadaten) und Playtest-Vorschau
  (`tools/leveleditor`, siehe [Editor-README](tools/leveleditor/README.md))
- Native Zugriffe für X11, Wayland, Win32, Cocoa, EGL und OSMesa

## Abhängigkeiten

| Abhängigkeit | Verwendung | Status im Referenzsystem |
|---|---|---|
| GLM | `include/sgl/Types.hpp` | Systempaket |
| X11, XRandR, Xinerama, Xcursor, Xi, Xext | X11-Backend | Systempaket |
| GLX / libGL | X11-Kontexte | Systempaket |
| Wayland-Client, Wayland-EGL, Wayland-Cursor, xkbcommon | Wayland-Backend | Systempaket |
| EGL | Wayland- und X11-EGL-Kontexte | Systempaket |
| wayland-protocols + wayland-scanner | Protokollcode | Systempaket |
| OSMesa | optionales OSMesa-Backend | optional |
| Catch2 3.7.1 | Tests | unter `third_party/catch2` vendort |
| SDL_GameControllerDB | Gamepad-Mapping | unter `third_party/gamecontrollerdb` vendort (zlib) |
| stb_image, stb_image_write, stb_truetype | Bild- und Schriftladen | unter `third_party/stb` vendort (Public Domain/MIT) |
| ttf2mesh | Mesh-Schriften | unter `third_party/ttf2mesh` vendort (MIT), direkt in `sgl` kompiliert |
| miniaudio 0.11 | Audio-Backend (WAV/FLAC/MP3) | unter `third_party/audio` vendort, direkt in `sgl` kompiliert |
| stb_vorbis 1.24 | Ogg-Vorbis für miniaudio | unter `third_party/audio` vendort (Public Domain), direkt in `sgl` kompiliert |
| qoa.h | QOA-Decoder/Encoder | unter `third_party/audio` vendort (MIT), direkt in `sgl` kompiliert |
| jar_xm.h | XM-Modul-Player | unter `third_party/audio` vendort (WTFPL v2), direkt in `sgl` kompiliert |
| cgltf 1.15 | glTF-/GLB-Loader | unter `third_party/model` vendort (MIT), direkt in `sgl` kompiliert |
| ufbx | FBX-Loader | unter `third_party/model` vendort (MIT), direkt in `sgl` kompiliert |
| tinyobjloader 2.0.0rc13 | OBJ-/MTL-Loader | unter `third_party/model` vendort (MIT), direkt in `sgl` kompiliert |
| mapbox/earcut.hpp | robuste Polygon-Triangulierung fuer OBJ | unter `third_party/model/mapbox` vendort (ISC) |
| DejaVu Sans (Teilmenge) | eingebettete Standardschrift | Daten in `src/gl/DefaultFont.cpp` |
| JavaFX 25 | UI des Leveleditors | Maven-Abhängigkeit (GPLv2+CE) |
| Weld SE 6 | CDI-Container des Leveleditors | Maven-Abhängigkeit (Apache 2) |
| Jackson 2 | JSON/TMJ im Leveleditor | Maven-Abhängigkeit (Apache 2) |
| JUnit 5 | Java-Tests | Maven-Abhängigkeit (EPL 2) |

## Bauen

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/examples/gl_loader/sgl_example_gl_loader
```

Der Leveleditor wird separat mit Maven gebaut (siehe unten).

Optionen:

```cmake
-DSGL_BACKEND_X11=ON
-DSGL_BACKEND_WAYLAND=ON
-DSGL_BACKEND_WIN32=ON
-DSGL_BACKEND_COCOA=ON
-DSGL_BACKEND_OSMESA=ON
-DSGL_BUILD_TESTS=ON
-DSGL_BUILD_EXAMPLES=ON
-DSGL_BUILD_SHARED=OFF        # sgl als Shared Library bauen
-DSGL_INSTALL=ON              # Install-/find_package-Regeln erzeugen
-DSGL_SANITIZE=""             # z. B. "address,undefined" (GCC/Clang)
-DSGL_HEADLESS_RENDER_TESTS=OFF  # Headless-Render-Smoke-Tests in CTest
```

Die Bibliotheksversion steht zentral in `project(sgl VERSION …)` und wird als
`sgl/Version.hpp` erzeugt (`SGL_VERSION_MAJOR`, `SGL_VERSION_MINOR`,
`SGL_VERSION_PATCH`, `SGL_VERSION_STRING`). `sgl::GetVersion()` und
`sgl::GetVersionString()` verwenden diese Werte.

## Installation und Einbindung

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix /usr/local   # oder ein eigenes Prefix
```

Danach lässt sich `sgl` in einem anderen Projekt über `find_package` verwenden:

```cmake
find_package(sgl 0.1 CONFIG REQUIRED)
target_link_libraries(mein_programm PRIVATE sgl::sgl)
```

Alternativ genügt `add_subdirectory(pfad/zu/sgl)` und
`target_link_libraries(mein_programm PRIVATE sgl::sgl)`.

## Fenster-Beispiele

Unter `examples/window/` liegen ausführbare Beispiele für Cursor, rahmenlose
Fenster, Icons, Vollbild-Umschaltung, Fensterattribute, Eingabe, Zwischenablage,
Drag & Drop, Monitore und Joysticks:

```bash
cmake -S . -B build -G Ninja -DSGL_BUILD_EXAMPLES=ON
cmake --build build
./build/examples/window/sgl_example_cursor
./build/examples/window/sgl_example_frameless
./build/examples/window/sgl_example_fullscreen
```

Die vollständige Liste und Tastenkürzel stehen in
[`examples/window/README.md`](examples/window/README.md).

Der eigenständige OpenGL-Loader-Demo aus dem Minimalbeispiel liegt unter
`examples/gl_loader/` und heißt `sgl_example_gl_loader`.

Unter `examples/font/` liegen zwei Mesh-Schrift-Beispiele. Sie können im
Fenstermodus oder headless als PNG gerendert werden:

```bash
./build/examples/font/sgl_example_meshfont2d
./build/examples/font/sgl_example_meshfont3d
./build/examples/font/sgl_example_meshfont2d --frame /tmp/font2d.png 1280 720
./build/examples/font/sgl_example_meshfont3d --frame /tmp/font3d.png 1280 720
```

Details: [`examples/font/README.md`](examples/font/README.md).

Unter `examples/model/` zeigt `sgl_example_model_viewer` zwei glTF-Modelle und
`zombie.fbx` mit ihren Materialien und Laufanimationen; alle rotieren um die
Y-Achse, `zombie.fbx` verwendet die Animation aus `walk.fbx`:

```bash
./build/examples/model/sgl_example_model_viewer
./build/examples/model/sgl_example_model_viewer --frame /tmp/model.png --time 0.5
```

Details: [`examples/model/README.md`](examples/model/README.md).


## Leveleditor

Unter `tools/leveleditor` liegt der 2D-Leveleditor als **Java-25-Anwendung**
mit **JavaFX 25**, **FXML** und **CDI (Weld SE)**. Er ersetzt den früheren
C++-Editor; die C++-Bibliothek `sgl` bleibt unabhängig davon bestehen.

![Leveleditor](tools/leveleditor/docs/screenshot.png)

```bash
cd tools/leveleditor
mvn -q javafx:run
# Demo-Projekt:
mvn -q javafx:run -Djavafx.args="--demo"
# Headless-Screenshot:
mvn -q javafx:run -Djavafx.args="--demo --frame target/editor.png"
```

Funktionen: Projektverwaltung mit mehreren Levels, Kachel- und Objektebenen,
Tileset-Import, Stift/Rechteck/Linie/Füllen/Pipette, Entities mit typisierten
Attributen, Inspector, Undo/Redo, Autosave, Kachelanimationen, Playtest (`F5`),
Tiled-Import/-Export (`.tmj`/`.tmx`, CSV/Base64/zlib/gzip, Templates,
Wang-Metadaten) sowie **andockbare und schwebende Panels**. Tabs lassen sich
zwischen linkem, rechtem und unterem Dock verschieben oder per Doppelklick in
ein eigenes Fenster lösen; das Layout wird in `.sgl/layout.json` gespeichert.

Details, Tastenkürzel und Projektformat:
[`tools/leveleditor/README.md`](tools/leveleditor/README.md).

## Fehlerbehandlung

Es gibt zwei Stile, die bewusst getrennt sind:

- **Fenster-, Init- und Modell-API:** `std::expected` – z. B. `sgl::Init()`,
  `sgl::Window::Create()`, `sgl::ModelIO::Load()`. Fehler werden ohne Ausnahme
  zurückgegeben.
- **Bild-API:** `sgl::ImageIO::Load()`, `Save()`, `Encode()` und
  `LoadFromMemory()` werfen `sgl::ImageError`. Für Code ohne Ausnahmen stehen
  die Varianten `TryLoad()`, `TryLoadFromMemory()`, `TrySave()`, `TryEncode()`
  und `TrySaveToMemory()` zur Verfügung, die `std::expected` zurückgeben.

```cpp
auto result = sgl::ImageIO::TryLoad("assets/tex/dirt.png");
if (!result) {
    SLOGE("Bild: %s", result.error().what());
} else {
    sgl::Texture2d texture = sgl::Texture2d::FromImage(*result);
}
```

## Logging

`include/sgl/Log.hpp` stellt einen leichtgewichtigen Logger mit farbigen Ausgaben bereit:

```cpp
#include <sgl/Log.hpp>

SLOGI("Fenster erstellt: %dx%d", 800, 600);
SLOGD << "Renderer: " << "Mesa";
```

Format:

```text
[2026-11-27 14:05:23.123] [INFO ] [Window.cpp:42] Fenster erstellt: 800x600
```

Reihenfolge: Datum/Uhrzeit, Log-Level, Datei:Zeile, Nachricht. Die Farben
hängen vom Level ab. Mit der Umgebungsvariable `NO_COLOR` lassen sich die
ANSI-Farben abschalten. Das Standard-Maximallevel ist `INFO`; über
`sgl::Logger::SetMaxLogLevel()` und `sgl::Logger::SetOutputStream()` lässt sich
das Verhalten anpassen.

## Minimalbeispiel

```cpp
#include <sgl/sgl.hpp>

int main() {
    if (auto result = sgl::Init(); !result) {
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

    auto windowResult = sgl::Window::Create(hints);
    if (!windowResult) {
        sgl::Terminate();
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
        sgl::PollEvents();
    }

    window.reset();
    sgl::Terminate();
}
```

## Achsen-Konvention

Joystick- und Gamepad-Y-Achsen verhalten sich auf allen Plattformen gleich:
**negativ = oben, positiv = unten**. Das entspricht der GLFW/SDL-Konvention.
X-Achsen sind positiv nach rechts. Trigger liegen im Bereich `-1 … +1`.
Die Reihenfolge der Rohachsen und Rohbuttons ist wie bei GLFW
plattformspezifisch; `GamepadState` ist plattformübergreifend normiert.

Gamepad-Namen, Achsen- und Button-Zuordnungen stammen aus der vendorten
SDL_GameControllerDB. Eigene Mappings können zur Laufzeit über
`sgl::UpdateGamepadMappings()` ergänzt werden.

Kalibrierung wird plattformspezifisch aus den Treiberdaten abgeleitet:
`EVIOCGABS` unter Linux, logische Min/Max plus Auto-Kalibrierung unter macOS
und die festen XInput-Bereiche unter Windows.

## Plattformhinweise

- **X11** ist die Referenzplattform und wird lokal vollständig gebaut und
  getestet.
- **Wayland** wird gebaut; Laufzeittests benötigen einen Compositor
  (z. B. Weston). Pointer-Lock, Rohmausbewegung und Fensterpositionierung sind
  durch Wayland nicht verfügbar und werden als `FeatureUnavailable` gemeldet.
- **Windows** und **macOS** werden in der CI gebaut und die Testsuite läuft
  dort ebenfalls; Laufzeittests mit echter GPU und Treibern stehen noch aus.
- **macOS** unterstützt maximal OpenGL 4.1. Ein 4.6-Kontext schlägt dort
  erwartungsgemäß mit `VersionUnavailable` fehl.
- `sgl::Texture2d` und `sgl::Framebuffer` nutzen Direct State Access und
  benötigen OpenGL 4.5; auf macOS 4.1 stehen sie daher nicht zur Verfügung.
  Der Java-Leveleditor ist davon unabhängig (JavaFX).
- **EGL** wird für Wayland und optional für X11 verwendet; X11 kann auch GLX,
  Win32 nutzt WGL, macOS NSOpenGL.
- **OSMesa** wird nur gebaut, wenn `libosmesa` per pkg-config gefunden wird.

## Tests

Die Catch2-Tests decken Fehlerbehandlung, Hints, Zeit, Null-Plattform,
Initialisierungsfehler, Bild-, Font-, Shader-/Textur-, Audio-, Modell- und
Geometrie-API ab; bei gesetztem `DISPLAY` kommen X11-Fenster und Monitore
hinzu. Die Java-Editor-Tests laufen separat über Maven (`mvn test`).

```bash
ctest --test-dir build --output-on-failure
```

Mit `-DSGL_HEADLESS_RENDER_TESTS=ON` werden zusätzlich drei Smoke-Tests
registriert: Font-2D, Font-3D und der Model-Viewer rendern headless als PNG.
Sie benötigen eine funktionierende EGL-/OSMesa- bzw. Xvfb-Umgebung.

Unter `.github/workflows/ci.yml` laufen Build, Tests, Sanitizer, clang-tidy
und die Headless-Renders auf Linux; Windows (WGL) und macOS (Cocoa) werden
ebenfalls gebaut und getestet. Der separate Workflow
`.github/workflows/java-editor.yml` baut und testet den Java-Editor und
erzeugt unter Xvfb einen Screenshot.

## Lizenzhinweise

Catch2 steht unter BSL-1.0, GLM unter MIT, SDL_GameControllerDB unter zlib.
Die Lizenztexte liegen unter `third_party/catch2/LICENSE.txt` bzw.
`third_party/gamecontrollerdb/LICENSE.txt`; GLM wird über das Systempaket
bezogen. Der Java-Editor nutzt JavaFX (GPLv2+CE), Weld SE (Apache 2),
Jackson (Apache 2) und JUnit 5 (EPL 2); die Abhängigkeiten werden über Maven
bezogen.
