# Demo-Projekt

Kleine orthogonale Plattform-Karte mit einem generierten Tileset
(`assets/terrain.png`), zwei Kachelebenen und einer Objektebene mit
Entities (Startpunkt, Gegner, Beute, Trigger).

Öffnen:

```bash
cd tools/leveleditor
mvn -q javafx:run -Djavafx.args="--demo"
```

Die Leveldatei ist Tiled-kompatibel (`levels/demo.tmj`) und kann auch direkt
in Tiled geöffnet werden. Der Pfad in `project.sglproj` ist relativ zum
Projektverzeichnis, sodass das Verzeichnis verschoben werden kann.
