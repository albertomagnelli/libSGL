package de.sgl.editor.model;

import java.util.Arrays;

public class TileLayer {
    public String name = "Kacheln";
    public boolean visible = true;
    public boolean locked = false;
    public double opacity = 1.0;
    public int[] tiles = new int[0];

    public TileLayer copy() {
        final TileLayer copy = new TileLayer();
        copy.name = name;
        copy.visible = visible;
        copy.locked = locked;
        copy.opacity = opacity;
        copy.tiles = Arrays.copyOf(tiles, tiles.length);
        return copy;
    }
}
