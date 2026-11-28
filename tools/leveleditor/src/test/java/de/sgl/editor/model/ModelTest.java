package de.sgl.editor.model;

import org.junit.jupiter.api.Test;

import static org.junit.jupiter.api.Assertions.*;

class ModelTest {
    @Test
    void undoAndRedoRestoreSnapshots() {
        final Level level = Project.createLevel(4, 3, 32, 32, "test");
        final UndoManager undo = new UndoManager();
        undo.reset(level);
        assertFalse(undo.canUndo());
        level.tileLayers.getFirst().tiles[0] = 42;
        undo.commit(level);
        assertTrue(undo.canUndo());
        final Level target = level.copy();
        assertTrue(undo.undo(target));
        assertEquals(0, target.tileLayers.getFirst().tiles[0]);
        assertTrue(undo.redo(target));
        assertEquals(42, target.tileLayers.getFirst().tiles[0]);
    }

    @Test
    void playtestLandsAndJumps() {
        final Level level = Project.createLevel(8, 6, 32, 32, "playtest");
        for (int x = 0; x < 8; x++) level.setTileAt(level.tileLayers.getFirst(), x, 5, 1);
        final Entity start = new Entity();
        start.id = level.nextEntityId++;
        start.def = "Startpunkt";
        start.x = 64;
        start.y = 128;
        start.w = 32;
        start.h = 32;
        level.objectLayers.getFirst().entities.add(start);

        final Playtest playtest = new Playtest();
        playtest.reset(level);
        for (int i = 0; i < 120; i++) playtest.step(level, 1.0 / 60.0, 0, false);
        assertTrue(playtest.grounded());
        assertTrue(playtest.y() + playtest.height() <= 5 * 32 + 2);
        final double groundedY = playtest.y();
        playtest.step(level, 1.0 / 60.0, 0, true);
        assertTrue(playtest.y() < groundedY);
    }

    @Test
    void animatedTilesResolveFrames() {
        final Level level = Project.createLevel(2, 2, 32, 32, "anim");
        final TileSet tileSet = new TileSet();
        tileSet.firstGid = 1;
        tileSet.animations.put(0, new TileAnimation());
        tileSet.animations.get(0).frames.add(new TileAnimation.Frame(1, 100));
        tileSet.animations.get(0).frames.add(new TileAnimation.Frame(2, 200));
        level.tilesets.add(tileSet);
        assertEquals(2, level.animatedTile(1, 0.05));
        assertEquals(3, level.animatedTile(1, 0.15));
        assertEquals(2, level.animatedTile(1, 0.35));
    }
}
