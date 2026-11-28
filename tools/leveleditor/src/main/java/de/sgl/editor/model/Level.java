package de.sgl.editor.model;

import java.util.ArrayList;
import java.util.List;

public class Level {
    public String name = "level";
    public int width = 60;
    public int height = 34;
    public int tileW = 32;
    public int tileH = 32;
    public int nextEntityId = 1;
    public final List<TileSet> tilesets = new ArrayList<>();
    public final List<TileLayer> tileLayers = new ArrayList<>();
    public final List<ObjectLayer> objectLayers = new ArrayList<>();

    public int tileAt(TileLayer layer, int x, int y) {
        if (x < 0 || y < 0 || x >= width || y >= height || layer.tiles.length == 0) return 0;
        return layer.tiles[y * width + x];
    }

    public void setTileAt(TileLayer layer, int x, int y, int value) {
        if (x < 0 || y < 0 || x >= width || y >= height || layer.tiles.length == 0) return;
        layer.tiles[y * width + x] = value;
    }

    public TileSet tileSetForGid(int gid) {
        TileSet result = null;
        for (final TileSet tileSet : tilesets) {
            if (tileSet.firstGid <= gid && (result == null || tileSet.firstGid > result.firstGid)) {
                result = tileSet;
            }
        }
        return result;
    }

    public EntityDef findEntityDef(Project project, String name) {
        return project.findEntityDef(name);
    }

    public Entity findEntity(int id) {
        for (final ObjectLayer layer : objectLayers) {
            for (final Entity entity : layer.entities) {
                if (entity.id == id) return entity;
            }
        }
        return null;
    }

    public Level copy() {
        final Level copy = new Level();
        copy.name = name;
        copy.width = width;
        copy.height = height;
        copy.tileW = tileW;
        copy.tileH = tileH;
        copy.nextEntityId = nextEntityId;
        for (final TileSet tileSet : tilesets) copy.tilesets.add(tileSet.copy());
        for (final TileLayer layer : tileLayers) copy.tileLayers.add(layer.copy());
        for (final ObjectLayer layer : objectLayers) copy.objectLayers.add(layer.copy());
        return copy;
    }

    public int animatedTile(int gid, double seconds) {
        if (gid == 0) return 0;
        final TileSet tileSet = tileSetForGid(gid);
        if (tileSet == null) return gid;
        final int local = gid - tileSet.firstGid;
        final TileAnimation animation = tileSet.animations.get(local);
        if (animation == null || animation.frames.isEmpty()) return gid;
        int total = 0;
        for (final TileAnimation.Frame frame : animation.frames) total += Math.max(frame.durationMs(), 1);
        int time = (int) ((Math.max(seconds, 0) * 1000.0) % total);
        for (final TileAnimation.Frame frame : animation.frames) {
            if (time < frame.durationMs()) return tileSet.firstGid + frame.tileId();
            time -= Math.max(frame.durationMs(), 1);
        }
        return gid;
    }
}
