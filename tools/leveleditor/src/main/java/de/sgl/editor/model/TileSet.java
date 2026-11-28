package de.sgl.editor.model;

import com.fasterxml.jackson.databind.JsonNode;

import java.util.LinkedHashMap;
import java.util.Map;

public class TileSet {
    public String name = "tileset";
    public String image = "";
    public int firstGid = 1;
    public int tileW = 32;
    public int tileH = 32;
    public int columns = 0;
    public int tileCount = 0;
    public int margin = 0;
    public int spacing = 0;
    public final Map<Integer, TileAnimation> animations = new LinkedHashMap<>();
    public JsonNode wangsets;

    public TileSet copy() {
        final TileSet copy = new TileSet();
        copy.name = name;
        copy.image = image;
        copy.firstGid = firstGid;
        copy.tileW = tileW;
        copy.tileH = tileH;
        copy.columns = columns;
        copy.tileCount = tileCount;
        copy.margin = margin;
        copy.spacing = spacing;
        for (final Map.Entry<Integer, TileAnimation> entry : animations.entrySet()) {
            final TileAnimation animation = new TileAnimation();
            animation.frames.addAll(entry.getValue().frames);
            copy.animations.put(entry.getKey(), animation);
        }
        copy.wangsets = wangsets == null ? null : wangsets.deepCopy();
        return copy;
    }
}
