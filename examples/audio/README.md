# Audio-Beispiele

Die Beispiele nutzen `sgl::Wave`, `sgl::Sound`, `sgl::Music` und
`sgl::AudioStream` mit den Dateien unter `resources/`.

| Beispiel | Beschreibung |
|---|---|
| `audio_info` | Dekodiert alle Wave-fähigen Ressourcen und zeigt Samplerate, Kanäle, Rahmen und Dauer. Kein Audiogerät nötig. |
| `audio_sound_loading` | Lädt kurze Effekte als `Sound` und spielt sie einzeln sowie als mehrstimmigen Akkord. |
| `audio_music_stream` | Spielt `country.mp3` oder eine andere Datei (z. B. `target.qoa`) gestreamt bzw. dekodiert über `Music`, inklusive Schleife, Seek und Zeitabfragen. |
| `audio_module_stream` | Rendert das XM-Modul `mini1111.xm` nach PCM und spielt es über `Music`. |
| `audio_raw_stream` | Erzeugt einen Sinuston und schiebt ihn blockweise in einen `AudioStream`. |

## Bauen

```bash
cmake -S . -B build -G Ninja -DSGL_BUILD_EXAMPLES=ON
cmake --build build
```

Die Targets heißen:

```text
sgl_example_audio_info
sgl_example_audio_sound_loading
sgl_example_audio_music_stream
sgl_example_audio_module_stream
sgl_example_audio_raw_stream
```

## Ausführen

```bash
./build/examples/audio/sgl_example_audio_info
./build/examples/audio/sgl_example_audio_sound_loading
./build/examples/audio/sgl_example_audio_music_stream 8
./build/examples/audio/sgl_example_audio_music_stream target.qoa 4
./build/examples/audio/sgl_example_audio_module_stream 8
./build/examples/audio/sgl_example_audio_raw_stream 3
```

Das Ressourcenverzeichnis wird beim Bauen als `SGL_AUDIO_RESOURCES`
eingetragen; die Beispiele funktionieren damit unabhängig vom aktuellen
Arbeitsverzeichnis.

## Formate

`Wave`/`Sound` unterstützen WAV, FLAC, MP3, OGG und QOA. XM ist kein
Sample-Format, sondern ein Modul-Song und wird ausschließlich über `Music`
unterstützt: `audio_module_stream` rendert `mini1111.xm` dafür einmal nach PCM.

Die Lizenzhinweise der Audiodateien stehen in `resources/LICENSE.md`; die der
eingebundenen Decoder in `third_party/audio/LICENSES.md`.
