package de.sgl.editor.io;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.node.ArrayNode;
import com.fasterxml.jackson.databind.node.ObjectNode;

import de.sgl.editor.model.Entity;
import de.sgl.editor.model.Level;
import de.sgl.editor.model.ObjectLayer;
import de.sgl.editor.model.Project;
import de.sgl.editor.model.TileAnimation;
import de.sgl.editor.model.TileLayer;
import de.sgl.editor.model.TileSet;

import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

import java.io.ByteArrayOutputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Base64;
import java.util.zip.DeflaterOutputStream;
import java.util.zip.GZIPOutputStream;

import static org.junit.jupiter.api.Assertions.*;

class TiledIoTest {
    private final TiledIo io = new TiledIo();
    private final ObjectMapper mapper = new ObjectMapper();

    private static Level sampleLevel() {
        final Level level = Project.createLevel(6, 4, 32, 32, "test");
        level.tileLayers.getFirst().tiles[0] = 1;
        level.tileLayers.getFirst().tiles[7] = 5;
        final ObjectLayer layer = level.objectLayers.getFirst();
        final Entity entity = new Entity();
        entity.id = level.nextEntityId++;
        entity.def = "Gegner";
        entity.x = 32;
        entity.y = 64;
        entity.attributes.put("Name", "Wache");
        entity.attributes.put("Leben", "3");
        layer.entities.add(entity);
        return level;
    }

    @Test
    void projectAndLevelJsonRoundtrip(@TempDir Path temp) throws Exception {
        final Project project = Project.create("Testprojekt", temp);
        project.levels.add("levels/test.tmj");
        final TileSet tileSet = new TileSet();
        tileSet.name = "terrain";
        tileSet.image = "assets/terrain.png";
        tileSet.wangsets = mapper.readTree("[{\"name\":\"Erde\"}]");
        tileSet.animations.put(3, new TileAnimation());
        tileSet.animations.get(3).frames.add(new TileAnimation.Frame(3, 100));
        tileSet.animations.get(3).frames.add(new TileAnimation.Frame(11, 200));
        project.tilesets.add(tileSet);

        final Path projectFile = temp.resolve("project.sglproj");
        io.writeProject(projectFile, project);
        final Project loaded = new Project();
        io.readProject(projectFile, loaded);
        assertEquals("Testprojekt", loaded.name);
        assertEquals(1, loaded.tilesets.size());
        assertNotNull(loaded.tilesets.getFirst().wangsets);
        assertEquals(2, loaded.tilesets.getFirst().animations.get(3).frames.size());

        final Level level = sampleLevel();
        level.tilesets.add(tileSet);
        final Path levelFile = temp.resolve("level.tmj");
        io.writeLevel(levelFile, level, project);
        final Level loadedLevel = io.readLevel(levelFile, project);
        assertEquals(6, loadedLevel.width);
        assertEquals(1, loadedLevel.tileLayers.getFirst().tiles[0]);
        assertEquals(1, loadedLevel.objectLayers.getFirst().entities.size());
        assertEquals("Wache", loadedLevel.objectLayers.getFirst().entities.getFirst().attributes.get("Name"));
    }

    @Test
    void tmjRoundtripKeepsTilesEntitiesWangAndAnimation(@TempDir Path temp) throws Exception {
        final Project project = Project.create("Tiled", temp);
        final TileSet tileSet = new TileSet();
        tileSet.name = "terrain";
        tileSet.image = "assets/terrain.png";
        tileSet.wangsets = mapper.readTree("[{\"name\":\"Erde\",\"wangtiles\":[{\"tileid\":0,\"wangid\":[1,1,1,1]}]}]");
        tileSet.animations.put(0, new TileAnimation());
        tileSet.animations.get(0).frames.add(new TileAnimation.Frame(1, 100));
        project.tilesets.add(tileSet);
        final Level level = sampleLevel();
        level.tilesets.add(tileSet);

        final Path file = temp.resolve("levels/test.tmj");
        io.writeLevel(file, level, project);
        final String text = Files.readString(file);
        assertTrue(text.contains("\"wangsets\""));
        assertTrue(text.contains("\"animation\""));

        final Level imported = io.readLevel(file, project);
        assertEquals(1, imported.tileLayers.getFirst().tiles[0]);
        assertEquals("Gegner", imported.objectLayers.getFirst().entities.getFirst().def);
        assertEquals(1, imported.tilesets.size());
        assertNotNull(imported.tilesets.getFirst().wangsets);
        assertEquals(1, imported.tilesets.getFirst().animations.get(0).frames.size());
    }

    @Test
    void tmxRoundtripWithCsv(@TempDir Path temp) throws Exception {
        final Project project = Project.create("Tiled", temp);
        final Level level = sampleLevel();
        final Path file = temp.resolve("level.tmx");
        io.writeLevel(file, level, project);
        final String text = Files.readString(file);
        assertTrue(text.contains("<map"));
        final Level imported = io.readLevel(file, project);
        assertEquals(6, imported.width);
        assertEquals(1, imported.tileLayers.getFirst().tiles[0]);
        assertEquals("Gegner", imported.objectLayers.getFirst().entities.getFirst().def);
    }

    @Test
    void compressedLayerDataZlibAndGzip(@TempDir Path temp) throws Exception {
        final int[] values = {1, 2, 3, 4};
        for (final String compression : new String[]{"zlib", "gzip", "none"}) {
            final byte[] raw = new byte[values.length * 4];
            for (int i = 0; i < values.length; i++) {
                raw[i * 4] = (byte) values[i];
            }
            final ByteArrayOutputStream compressed = new ByteArrayOutputStream();
            if ("gzip".equals(compression)) {
                try (GZIPOutputStream output = new GZIPOutputStream(compressed)) {
                    output.write(raw);
                }
            } else if ("zlib".equals(compression)) {
                try (DeflaterOutputStream output = new DeflaterOutputStream(compressed)) {
                    output.write(raw);
                }
            } else {
                compressed.write(raw);
            }
            final ObjectNode map = mapper.createObjectNode();
            map.put("type", "map");
            map.put("width", 2);
            map.put("height", 2);
            map.put("tilewidth", 32);
            map.put("tileheight", 32);
            final ArrayNode tilesets = map.putArray("tilesets");
            final ObjectNode layer = mapper.createObjectNode();
            layer.put("type", "tilelayer");
            layer.put("name", "Kacheln");
            layer.put("width", 2);
            layer.put("height", 2);
            layer.put("encoding", "base64");
            layer.put("compression", compression);
            layer.put("data", Base64.getEncoder().encodeToString(compressed.toByteArray()));
            map.putArray("layers").add(layer);
            final Path file = temp.resolve("level-" + compression + ".tmj");
            Files.writeString(file, mapper.writeValueAsString(map));
            final Project project = Project.create("Compressed", temp);
            final Level level = io.readLevel(file, project);
            assertEquals(1, level.tileLayers.getFirst().tiles[0]);
            assertEquals(4, level.tileLayers.getFirst().tiles[3]);
        }
    }

    @Test
    void templateJsonIsResolvedAndPreserved(@TempDir Path temp) throws Exception {
        final Project project = Project.create("Templates", temp);
        Files.createDirectories(temp.resolve("templates"));
        Files.createDirectories(temp.resolve("levels"));
        Files.writeString(temp.resolve("templates/enemy.tj"), """
                {"type":"template","object":{"type":"Gegner","width":40,"height":50,
                "properties":[{"name":"Leben","type":"int","value":"10"}]}}
                """);
        final ObjectNode map = mapper.createObjectNode();
        map.put("type", "map");
        map.put("width", 4);
        map.put("height", 3);
        map.put("tilewidth", 32);
        map.put("tileheight", 32);
        map.putArray("tilesets");
        final ObjectNode group = mapper.createObjectNode();
        group.put("type", "objectgroup");
        group.put("name", "Objekte");
        final ObjectNode object = group.putArray("objects").addObject();
        object.put("id", 1);
        object.put("name", "Wache");
        object.put("x", 32);
        object.put("y", 64);
        object.put("template", "../templates/enemy.tj");
        final ObjectNode property = object.putArray("properties").addObject();
        property.put("name", "Leben");
        property.put("type", "int");
        property.put("value", "25");
        map.putArray("layers").add(group);
        final Path file = temp.resolve("levels/map.tmj");
        Files.writeString(file, mapper.writeValueAsString(map));

        final Level level = io.readLevel(file, project);
        final Entity entity = level.objectLayers.getFirst().entities.getFirst();
        assertEquals("Gegner", entity.def);
        assertEquals(40, entity.w);
        assertEquals(50, entity.h);
        assertEquals("25", entity.attributes.get("Leben"));
        assertEquals("templates/enemy.tj", entity.templatePath);

        final Path exported = temp.resolve("levels/out.tmj");
        io.writeLevel(exported, level, project);
        assertTrue(Files.readString(exported).contains("../templates/enemy.tj"));
    }

    @Test
    void templateXmlIsResolved(@TempDir Path temp) throws Exception {
        final Project project = Project.create("Templates", temp);
        Files.createDirectories(temp.resolve("templates"));
        Files.writeString(temp.resolve("templates/pickup.tx"), """
                <?xml version="1.0" encoding="UTF-8"?>
                <template><object type="Beute" width="16" height="16">
                <properties><property name="Wert" type="int" value="5"/></properties>
                </object></template>
                """);
        Files.writeString(temp.resolve("map.tmx"), """
                <?xml version="1.0" encoding="UTF-8"?>
                <map version="1.10" orientation="orthogonal" width="4" height="3" tilewidth="32" tileheight="32">
                <objectgroup name="Objekte"><object id="1" name="Münze" x="32" y="32" template="templates/pickup.tx">
                <properties><property name="Wert" type="int" value="9"/></properties>
                </object></objectgroup></map>
                """);
        final Level level = io.readLevel(temp.resolve("map.tmx"), project);
        final Entity entity = level.objectLayers.getFirst().entities.getFirst();
        assertEquals("Beute", entity.def);
        assertEquals(16, entity.w);
        assertEquals("9", entity.attributes.get("Wert"));
        assertEquals("templates/pickup.tx", entity.templatePath);
    }
}
