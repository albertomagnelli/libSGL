# Font-Beispiele

Beide Beispiele nutzen `sgl::Font2d`/`sgl::Font3d`, `sgl::Shader` und `sgl::Window`.

```bash
cmake -S . -B build -G Ninja -DSGL_BUILD_EXAMPLES=ON
cmake --build build

# Fenstermodus
./build/examples/font/sgl_example_meshfont2d
./build/examples/font/sgl_example_meshfont3d

# Headless: PNG schreiben
./build/examples/font/sgl_example_meshfont2d --frame /tmp/font2d.png 1280 720
./build/examples/font/sgl_example_meshfont3d --frame /tmp/font3d.png 1280 720
```

| Beispiel | Ziel | Inhalt |
|---|---|---|
| `meshfont_2d_example.cpp` | `sgl_example_meshfont2d` | 2D-Glyphennetze, Schatten, Kontur, Verlaufsfüllung |
| `meshfont_3d_example.cpp` | `sgl_example_meshfont3d` | extrudierte 3D-Glyphen, Bodenebene, Beleuchtung, Wireframe |

## Steuerung

- **2D:** `ESC` beendet.
- **3D:** `ESC` beendet, `F` schaltet das Drahtgitter um.

## Schrift

`FindSystemFont()` sucht eine TrueType-Systemschrift. Mit `SGL_FONT` kann
explizit eine `.ttf`-Datei angegeben werden; `LIBRAY_FONT` wird weiterhin als
Fallback akzeptiert.

## Headless

`--frame <datei> [breite höhe]` erzeugt ein verstecktes `sgl`-Fenster mit
OpenGL-4.6-Core-Kontext, rendert in ein FBO und schreibt das Ergebnis als PNG.
Dafür wird ein funktionierender GL-Kontext benötigt (unter Linux z. B. X11).
