# Model-Beispiel

`sgl_example_model_viewer` lädt beide glTF-Modelle aus `resources/`:

- `character.glb` (CesiumMan)
- `turrican.glb`

Beide Modelle werden nebeneinander als PBR-Metallic-Roughness-Meshes gerendert
und rotieren kontinuierlich um die Y-Achse. Albedo, Metallic, Roughness, AO,
Normal und Emissive werden aus den geladenen `sgl::Material`-Maps verwendet.

Die Modelle werden in ihrer Bind-Pose gezeigt; Animationen werden in diesem
Beispiel nicht angewendet.

## Bauen

```bash
cmake -S . -B build -G Ninja -DSGL_BUILD_EXAMPLES=ON
cmake --build build --target sgl_example_model_viewer
```

## Ausführen

```bash
./build/examples/model/sgl_example_model_viewer
```

Beenden mit `Esc` oder durch Schließen des Fensters.

### Headless-Screenshot

```bash
./build/examples/model/sgl_example_model_viewer --frame bild.png --time 0.5
```

`--time` wählt den Rotationszeitpunkt; das Fenster wird bei `--frame` nicht
angezeigt.
