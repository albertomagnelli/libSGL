package de.sgl.editor.model;

import java.util.ArrayList;
import java.util.List;

/** Snapshot-basiertes Undo/Redo pro Dokument. */
public class UndoManager {
    private static final int LIMIT = 80;

    private final List<Level> states = new ArrayList<>();
    private int index = 0;

    public void reset(Level level) {
        states.clear();
        states.add(level.copy());
        index = 0;
    }

    public void commit(Level level) {
        if (states.isEmpty()) {
            reset(level);
            return;
        }
        while (states.size() > index + 1) {
            states.removeLast();
        }
        states.add(level.copy());
        if (states.size() > LIMIT) {
            states.removeFirst();
        }
        index = states.size() - 1;
    }

    public boolean undo(Level target) {
        if (index <= 0) return false;
        --index;
        copyInto(states.get(index), target);
        return true;
    }

    public boolean redo(Level target) {
        if (index + 1 >= states.size()) return false;
        ++index;
        copyInto(states.get(index), target);
        return true;
    }

    public boolean canUndo() { return index > 0; }
    public boolean canRedo() { return index + 1 < states.size(); }

    private static void copyInto(Level source, Level target) {
        target.name = source.name;
        target.width = source.width;
        target.height = source.height;
        target.tileW = source.tileW;
        target.tileH = source.tileH;
        target.nextEntityId = source.nextEntityId;
        target.tilesets.clear();
        target.tileLayers.clear();
        target.objectLayers.clear();
        for (final TileSet tileSet : source.tilesets) target.tilesets.add(tileSet.copy());
        for (final TileLayer layer : source.tileLayers) target.tileLayers.add(layer.copy());
        for (final ObjectLayer layer : source.objectLayers) target.objectLayers.add(layer.copy());
    }
}
