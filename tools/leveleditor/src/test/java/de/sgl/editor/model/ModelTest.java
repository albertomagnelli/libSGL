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
    void playtestIgnoresBackgroundLayerWhenWalking() {
        final Level level = Project.createLevel(10, 8, 32, 32, "hintergrund");
        final TileLayer background = level.tileLayers.getFirst();
        background.name = "Hintergrund";
        final TileLayer ground = new TileLayer();
        ground.name = "Kacheln";
        ground.tiles = new int[level.width * level.height];
        level.tileLayers.add(ground);
        for (int x = 0; x < level.width; x++) {
            background.tiles[4 * level.width + x] = 1;
            ground.tiles[5 * level.width + x] = 1;
        }
        final Entity start = new Entity();
        start.id = level.nextEntityId++;
        start.def = "Startpunkt";
        start.x = 64;
        start.y = 96;
        start.w = 32;
        start.h = 64;
        level.objectLayers.getFirst().entities.add(start);

        final Playtest playtest = new Playtest();
        playtest.reset(level);
        final double startX = playtest.x();
        for (int i = 0; i < 30; i++) playtest.step(level, 1.0 / 60.0, 1, false);
        assertTrue(playtest.x() > startX + 20, "Figur darf nicht in der Hintergrundebene feststecken");
        assertTrue(playtest.grounded());
        assertEquals(5 * 32, playtest.y() + playtest.height(), 0.6);
    }

    @Test
    void playtestLiftsSpawnOutOfSolidTiles() {
        final Level level = Project.createLevel(8, 8, 32, 32, "spawn");
        for (int x = 0; x < level.width; x++) {
            level.setTileAt(level.tileLayers.getFirst(), x, 4, 1);
            level.setTileAt(level.tileLayers.getFirst(), x, 5, 1);
        }
        final Entity start = new Entity();
        start.id = level.nextEntityId++;
        start.def = "Startpunkt";
        start.x = 64;
        start.y = 128;
        start.w = 32;
        start.h = 64;
        level.objectLayers.getFirst().entities.add(start);

        final Playtest playtest = new Playtest();
        playtest.reset(level);
        assertTrue(playtest.y() + playtest.height() <= 4 * 32 + 0.01,
                "Startpunkt muss aus fester Geometrie herausgehoben werden");
        for (int i = 0; i < 90; i++) playtest.step(level, 1.0 / 60.0, 0, false);
        assertTrue(playtest.grounded());
        assertEquals(4 * 32, playtest.y() + playtest.height(), 0.6);
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
