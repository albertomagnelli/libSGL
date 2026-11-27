# Audio-Beispiele

Die Beispiele nutzen `sgl::Wave`, `sgl::Sound`, `sgl::Music` und
`sgl::AudioStream` mit den Dateien unter `resources/`.

| Beispiel | Beschreibung |
|---|---|
| `audio_info` | Dekodiert alle Ressourcen und zeigt Samplerate, Kanäle, Rahmen und Dauer. Kein Audiogerät nötig. |
| `audio_sound_loading` | Lädt kurze Effekte als `Sound` und spielt sie einzeln sowie als mehrstimmigen Akkord. |
| `audio_music_stream` | Spielt `country.mp3` gestreamt über `Music`, inklusive Schleife, Seek und Zeitabfragen. |
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
sgl_example_audio_raw_stream
```

## Ausführen

```bash
./build/examples/audio/sgl_example_audio_info
./build/examples/audio/sgl_example_audio_sound_loading
./build/examples/audio/sgl_example_audio_music_stream 8
./build/examples/audio/sgl_example_audio_raw_stream 3
```

Das Ressourcenverzeichnis wird beim Bauen als `SGL_AUDIO_RESOURCES`
eingetragen; die Beispiele funktionieren damit unabhängig vom aktuellen
Arbeitsverzeichnis.

## Formate

Unterstützt werden WAV, FLAC, MP3 und OGG. `mini1111.xm` und `target.qoa`
liegen als zusätzliche Testdateien bei, werden von miniaudio aber nicht
dekodiert. Die Lizenzhinweise stehen in `resources/LICENSE.md`.
