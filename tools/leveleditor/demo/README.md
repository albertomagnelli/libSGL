# Demo-Projekt

Kleine orthogonale Plattform-Karte mit einem generierten Tileset
(`assets/terrain.png`), zwei Kachelebenen und einer Objektebene mit
Entities (Startpunkt, Gegner, Beute, Trigger).

Öffnen:

```bash
./build/tools/leveleditor/sgl_editor --project tools/leveleditor/demo
```

Die Leveldatei ist Tiled-kompatibel (`levels/demo.tmj`) und kann auch direkt
in Tiled geöffnet werden. Der Pfad in `project.sglproj` ist relativ zum
Projektverzeichnis, sodass das Verzeichnis verschoben werden kann.
