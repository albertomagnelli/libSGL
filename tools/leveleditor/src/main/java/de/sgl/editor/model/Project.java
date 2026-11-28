package de.sgl.editor.model;

import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;

public class Project {
    public String name = "Neues Projekt";
    public Path root;
    public String version = "1";
    public final List<String> levels = new ArrayList<>();
    public final List<TileSet> tilesets = new ArrayList<>();
    public final List<EntityDef> entityDefs = new ArrayList<>();

    public EntityDef findEntityDef(String name) {
        for (final EntityDef def : entityDefs) {
            if (def.name.equals(name)) return def;
        }
        return null;
    }

    public TileSet findTileSet(String name) {
        for (final TileSet tileSet : tilesets) {
            if (tileSet.name.equals(name)) return tileSet;
        }
        return null;
    }

    public void ensureEntityDef(String name) {
        if (findEntityDef(name) != null) return;
        final EntityDef def = new EntityDef(name == null || name.isBlank() ? "Objekt" : name, "#7896be");
        def.attributes.add(new AttributeDef("Name", "string", def.name));
        entityDefs.add(def);
    }

    public static Project create(String name, Path root) {
        final Project project = new Project();
        project.name = name;
        project.root = root;
        project.entityDefs.add(new EntityDef("Startpunkt", "#50c878"));
        project.entityDefs.add(new EntityDef("Gegner", "#dc5a5a"));
        project.entityDefs.add(new EntityDef("Beute", "#e6be46"));
        project.entityDefs.add(new EntityDef("Trigger", "#8c78e6"));
        for (final EntityDef def : project.entityDefs) {
            def.attributes.add(new AttributeDef("Name", "string", def.name));
        }
        return project;
    }

    public static Level createLevel(int width, int height, int tileW, int tileH, String name) {
        final Level level = new Level();
        level.name = name;
        level.width = Math.max(width, 1);
        level.height = Math.max(height, 1);
        level.tileW = Math.max(tileW, 1);
        level.tileH = Math.max(tileH, 1);
        final TileLayer tiles = new TileLayer();
        tiles.name = "Kacheln";
        tiles.tiles = new int[level.width * level.height];
        level.tileLayers.add(tiles);
        final ObjectLayer objects = new ObjectLayer();
        objects.name = "Objekte";
        level.objectLayers.add(objects);
        return level;
    }
}
