package de.sgl.editor.io;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.node.ArrayNode;
import com.fasterxml.jackson.databind.node.ObjectNode;

import de.sgl.editor.model.AttributeDef;
import de.sgl.editor.model.Entity;
import de.sgl.editor.model.EntityDef;
import de.sgl.editor.model.Level;
import de.sgl.editor.model.ObjectLayer;
import de.sgl.editor.model.Project;
import de.sgl.editor.model.TileAnimation;
import de.sgl.editor.model.TileLayer;
import de.sgl.editor.model.TileSet;

import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;

import javax.xml.XMLConstants;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.transform.OutputKeys;
import javax.xml.transform.Transformer;
import javax.xml.transform.TransformerFactory;
import javax.xml.transform.dom.DOMSource;
import javax.xml.transform.stream.StreamResult;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Base64;
import java.util.List;
import java.util.Map;
import java.util.zip.GZIPInputStream;
import java.util.zip.InflaterInputStream;

import jakarta.enterprise.context.ApplicationScoped;

/** Tiled-TMJ/TMX sowie Projektdatei. Bewusst zustandslos und thread-safe. */
@ApplicationScoped
public class TiledIo {
    private static final int GID_MASK = 0x1FFFFFFF;
    private final ObjectMapper mapper = new ObjectMapper();

    // ── Projekt ─────────────────────────────────────────────────────────────

    public void writeProject(Path file, Project project) throws IOException {
        final ObjectNode root = mapper.createObjectNode();
        root.put("version", project.version);
        root.put("name", project.name);
        final ArrayNode levels = root.putArray("levels");
        for (final String level : project.levels) levels.add(level);
        final ArrayNode tilesets = root.putArray("tilesets");
        for (final TileSet tileSet : project.tilesets) tilesets.add(writeTileSetJson(tileSet, project.root));
        final ArrayNode entities = root.putArray("entities");
        for (final EntityDef def : project.entityDefs) {
            final ObjectNode node = entities.addObject();
            node.put("name", def.name);
            node.put("color", def.color);
            final ArrayNode attrs = node.putArray("attributes");
            for (final AttributeDef attr : def.attributes) {
                final ObjectNode attrNode = attrs.addObject();
                attrNode.put("name", attr.name);
                attrNode.put("type", attr.type);
                attrNode.put("default", attr.defaultValue);
                final ArrayNode values = attrNode.putArray("values");
                for (final String value : attr.values) values.add(value);
            }
        }
        Files.createDirectories(file.toAbsolutePath().getParent());
        mapper.writerWithDefaultPrettyPrinter().writeValue(file.toFile(), root);
    }

    public void readProject(Path file, Project project) throws IOException {
        final JsonNode root = mapper.readTree(file.toFile());
        project.root = file.toAbsolutePath().getParent();
        project.name = text(root, "name", "Projekt");
        project.version = text(root, "version", "1");
        project.levels.clear();
        project.tilesets.clear();
        project.entityDefs.clear();
        for (final JsonNode level : array(root, "levels")) project.levels.add(level.asText());
        for (final JsonNode tileSet : array(root, "tilesets")) {
            project.tilesets.add(readTileSetJson(tileSet, 1, project.root, project));
        }
        for (final JsonNode def : array(root, "entities")) {
            final EntityDef entityDef = new EntityDef(text(def, "name", "Objekt"), text(def, "color", "#7896be"));
            for (final JsonNode attr : array(def, "attributes")) {
                final AttributeDef attribute =
                        new AttributeDef(text(attr, "name", "Attribut"), text(attr, "type", "string"),
                                text(attr, "default", ""));
                for (final JsonNode value : array(attr, "values")) attribute.values.add(value.asText());
                entityDef.attributes.add(attribute);
            }
            project.entityDefs.add(entityDef);
        }
        if (project.entityDefs.isEmpty()) {
            for (final EntityDef def : Project.create(project.name, project.root).entityDefs) {
                project.entityDefs.add(def);
            }
        }
    }

    // ── Level ───────────────────────────────────────────────────────────────

    public Level readLevel(Path file, Project project) throws IOException {
        final String name = file.getFileName().toString().toLowerCase();
        if (name.endsWith(".tmj")) return readTmj(file, project);
        if (name.endsWith(".tmx")) return readTmx(file, project);
        throw new IOException("Unbekanntes Tiled-Format: " + file);
    }

    public void writeLevel(Path file, Level level, Project project) throws IOException {
        final String name = file.getFileName().toString().toLowerCase();
        if (name.endsWith(".tmj")) {
            writeTmj(file, level, project);
        } else if (name.endsWith(".tmx")) {
            writeTmx(file, level, project);
        } else {
            throw new IOException("Unbekanntes Tiled-Format: " + file);
        }
    }

    // ── TMJ ─────────────────────────────────────────────────────────────────

    private Level readTmj(Path file, Project project) throws IOException {
        final JsonNode root = mapper.readTree(file.toFile());
        final Level level = new Level();
        level.name = stem(file);
        level.width = Math.max(integer(root, "width", 60), 1);
        level.height = Math.max(integer(root, "height", 34), 1);
        level.tileW = Math.max(integer(root, "tilewidth", 32), 1);
        level.tileH = Math.max(integer(root, "tileheight", 32), 1);
        level.nextEntityId = Math.max(integer(root, "nextobjectid", 1), 1);
        for (final JsonNode tileSetNode : array(root, "tilesets")) {
            final int firstGid = integer(tileSetNode, "firstgid", 1);
            final TileSet tileSet;
            final String source = text(tileSetNode, "source", "");
            if (!source.isEmpty()) {
                tileSet = readTileSetFile(resolve(file.getParent(), source), firstGid, project);
            } else {
                tileSet = readTileSetJson(tileSetNode, firstGid, file.getParent(), project);
            }
            level.tilesets.add(tileSet);
            mergeTileSet(project, tileSet);
        }
        for (final JsonNode layerNode : array(root, "layers")) {
            final String type = text(layerNode, "type", "");
            if ("tilelayer".equals(type)) {
                final TileLayer layer = new TileLayer();
                layer.name = text(layerNode, "name", "Kacheln");
                layer.visible = bool(layerNode, "visible", true);
                layer.opacity = number(layerNode, "opacity", 1);
                layer.tiles = decodeLayerData(layerNode, level.width, level.height);
                level.tileLayers.add(layer);
            } else if ("objectgroup".equals(type)) {
                final ObjectLayer layer = new ObjectLayer();
                layer.name = text(layerNode, "name", "Objekte");
                layer.visible = bool(layerNode, "visible", true);
                for (final JsonNode object : array(layerNode, "objects")) {
                    layer.entities.add(readTmjObject(object, file.getParent(), project, level));
                }
                level.objectLayers.add(layer);
            }
        }
        if (level.tileLayers.isEmpty() && level.objectLayers.isEmpty()) {
            level.tileLayers.addAll(Project.createLevel(level.width, level.height, level.tileW, level.tileH,
                    level.name).tileLayers);
            level.objectLayers.addAll(Project.createLevel(level.width, level.height, level.tileW, level.tileH,
                    level.name).objectLayers);
        }
        return level;
    }

    private Entity readTmjObject(JsonNode object, Path baseDir, Project project, Level level) throws IOException {
        final Entity entity = new Entity();
        final String template = text(object, "template", "");
        if (!template.isEmpty()) {
            final Entity base = readTemplate(resolve(baseDir, template), project);
            entity.def = base.def;
            entity.w = base.w;
            entity.h = base.h;
            entity.rotation = base.rotation;
            entity.attributes.putAll(base.attributes);
            entity.templatePath = projectRelative(project, resolve(baseDir, template));
        }
        if (object.has("id")) entity.id = integer(object, "id", level.nextEntityId);
        else entity.id = level.nextEntityId++;
        if (entity.id >= level.nextEntityId) level.nextEntityId = entity.id + 1;
        String type = text(object, "type", "");
        if (type.isEmpty()) type = text(object, "class", "");
        if (type.isEmpty()) type = entity.def;
        final int gid = integer(object, "gid", 0);
        if (type.isEmpty()) type = gid != 0 ? "Kachelobjekt" : "Objekt";
        entity.def = type;
        project.ensureEntityDef(type);
        entity.x = number(object, "x", 0);
        entity.y = number(object, "y", 0);
        if (object.has("width")) entity.w = number(object, "width", entity.w);
        else if (entity.w <= 0) entity.w = 32;
        if (object.has("height")) entity.h = number(object, "height", entity.h);
        else if (entity.h <= 0) entity.h = 32;
        if (object.has("rotation")) entity.rotation = number(object, "rotation", entity.rotation);
        final String name = text(object, "name", "");
        if (!name.isEmpty()) entity.attributes.put("Name", name);
        if (gid != 0) entity.attributes.put("gid", Integer.toString(gid & GID_MASK));
        readProperties(object.get("properties"), entity, project, type);
        return entity;
    }

    private void writeTmj(Path file, Level level, Project project) throws IOException {
        final ObjectNode root = mapper.createObjectNode();
        root.put("type", "map");
        root.put("version", "1.10");
        root.put("tiledversion", "1.10.2");
        root.put("orientation", "orthogonal");
        root.put("renderorder", "right-down");
        root.put("infinite", false);
        root.put("width", level.width);
        root.put("height", level.height);
        root.put("tilewidth", level.tileW);
        root.put("tileheight", level.tileH);
        root.put("nextlayerid", level.tileLayers.size() + level.objectLayers.size() + 1);
        root.put("nextobjectid", level.nextEntityId);
        final ArrayNode tilesets = root.putArray("tilesets");
        for (final TileSet tileSet : level.tilesets) {
            tilesets.add(writeTileSetJson(tileSet, file.toAbsolutePath().getParent()));
        }
        final ArrayNode layers = root.putArray("layers");
        for (final TileLayer layer : level.tileLayers) {
            final ObjectNode node = layers.addObject();
            node.put("type", "tilelayer");
            node.put("name", layer.name);
            node.put("visible", layer.visible);
            node.put("opacity", layer.opacity);
            final ArrayNode data = node.putArray("data");
            for (final int tile : layer.tiles) data.add(tile);
        }
        for (final ObjectLayer layer : level.objectLayers) {
            final ObjectNode node = layers.addObject();
            node.put("type", "objectgroup");
            node.put("name", layer.name);
            node.put("visible", layer.visible);
            final ArrayNode objects = node.putArray("objects");
            for (final Entity entity : layer.entities) {
                final ObjectNode object = objects.addObject();
                object.put("id", entity.id);
                object.put("name", entity.attributes.getOrDefault("Name", entity.def));
                object.put("type", entity.def);
                object.put("x", entity.x);
                object.put("y", entity.y);
                object.put("width", entity.w);
                object.put("height", entity.h);
                if (entity.rotation != 0) object.put("rotation", entity.rotation);
                if (entity.attributes.containsKey("gid")) {
                    object.put("gid", Integer.parseInt(entity.attributes.get("gid")));
                }
                if (!entity.templatePath.isEmpty()) {
                    object.put("template", relativeTo(file.toAbsolutePath().getParent(),
                            resolve(project.root, entity.templatePath)));
                }
                writeProperties(object, entity, project);
            }
        }
        Files.createDirectories(file.toAbsolutePath().getParent());
        mapper.writerWithDefaultPrettyPrinter().writeValue(file.toFile(), root);
    }

    // ── TMX ─────────────────────────────────────────────────────────────────

    private Level readTmx(Path file, Project project) throws IOException {
        try {
            final Document document = parseXml(file);
            final Element map = document.getDocumentElement();
            final Level level = new Level();
            level.name = stem(file);
            level.width = Math.max(attrInt(map, "width", 60), 1);
            level.height = Math.max(attrInt(map, "height", 34), 1);
            level.tileW = Math.max(attrInt(map, "tilewidth", 32), 1);
            level.tileH = Math.max(attrInt(map, "tileheight", 32), 1);
            level.nextEntityId = Math.max(attrInt(map, "nextobjectid", 1), 1);
            for (final Element tileSetElement : children(map, "tileset")) {
                final int firstGid = attrInt(tileSetElement, "firstgid", 1);
                final String source = tileSetElement.getAttribute("source");
                final TileSet tileSet = source.isEmpty()
                        ? readTileSetElement(tileSetElement, firstGid, file.getParent(), project)
                        : readTileSetFile(resolve(file.getParent(), source), firstGid, project);
                level.tilesets.add(tileSet);
                mergeTileSet(project, tileSet);
            }
            for (final Element layerElement : children(map, "layer")) {
                final TileLayer layer = new TileLayer();
                layer.name = attr(layerElement, "name", "Kacheln");
                layer.visible = attrBool(layerElement, "visible", true);
                layer.opacity = attrDouble(layerElement, "opacity", 1);
                layer.tiles = decodeTmxData(layerElement, level.width, level.height);
                level.tileLayers.add(layer);
            }
            for (final Element group : children(map, "objectgroup")) {
                final ObjectLayer layer = new ObjectLayer();
                layer.name = attr(group, "name", "Objekte");
                layer.visible = attrBool(group, "visible", true);
                for (final Element object : children(group, "object")) {
                    layer.entities.add(readTmxObject(object, file.getParent(), project, level));
                }
                level.objectLayers.add(layer);
            }
            if (level.tileLayers.isEmpty() && level.objectLayers.isEmpty()) {
                final Level defaults = Project.createLevel(level.width, level.height, level.tileW, level.tileH,
                        level.name);
                level.tileLayers.addAll(defaults.tileLayers);
                level.objectLayers.addAll(defaults.objectLayers);
            }
            return level;
        } catch (IOException exception) {
            throw exception;
        } catch (Exception exception) {
            throw new IOException("TMX konnte nicht gelesen werden: " + file, exception);
        }
    }

    private Entity readTmxObject(Element object, Path baseDir, Project project, Level level) throws IOException {
        final Entity entity = new Entity();
        final String template = object.getAttribute("template");
        if (!template.isEmpty()) {
            final Entity base = readTemplate(resolve(baseDir, template), project);
            entity.def = base.def;
            entity.w = base.w;
            entity.h = base.h;
            entity.rotation = base.rotation;
            entity.attributes.putAll(base.attributes);
            entity.templatePath = projectRelative(project, resolve(baseDir, template));
        }
        entity.id = level.nextEntityId++;
        String type = object.getAttribute("type");
        if (type.isEmpty()) type = object.getAttribute("class");
        if (type.isEmpty()) type = entity.def;
        final int gid = object.hasAttribute("gid") ? parseInt(object.getAttribute("gid"), 0)
                : entity.attributes.containsKey("gid") ? Integer.parseInt(entity.attributes.get("gid")) : 0;
        if (type.isEmpty()) type = gid != 0 ? "Kachelobjekt" : "Objekt";
        entity.def = type;
        project.ensureEntityDef(type);
        entity.x = attrDouble(object, "x", 0);
        entity.y = attrDouble(object, "y", 0);
        entity.w = object.hasAttribute("width") ? attrDouble(object, "width", entity.w) : entity.w > 0 ? entity.w : 32;
        entity.h = object.hasAttribute("height") ? attrDouble(object, "height", entity.h) : entity.h > 0 ? entity.h : 32;
        if (object.hasAttribute("rotation")) entity.rotation = attrDouble(object, "rotation", entity.rotation);
        final String name = object.getAttribute("name");
        if (!name.isEmpty()) entity.attributes.put("Name", name);
        if (gid != 0) entity.attributes.put("gid", Integer.toString(gid & GID_MASK));
        for (final Element properties : children(object, "properties")) {
            for (final Element property : children(properties, "property")) {
                final String propertyName = property.getAttribute("name");
                if (propertyName.isEmpty()) continue;
                entity.attributes.put(propertyName, property.getAttribute("value"));
                addPropertyDef(project, type, propertyName, property.getAttribute("type"));
            }
        }
        return entity;
    }

    private void writeTmx(Path file, Level level, Project project) throws IOException {
        try {
            final Document document = DocumentBuilderFactory.newInstance().newDocumentBuilder().newDocument();
            final Element map = document.createElement("map");
            map.setAttribute("version", "1.10");
            map.setAttribute("tiledversion", "1.10.2");
            map.setAttribute("orientation", "orthogonal");
            map.setAttribute("renderorder", "right-down");
            map.setAttribute("width", Integer.toString(level.width));
            map.setAttribute("height", Integer.toString(level.height));
            map.setAttribute("tilewidth", Integer.toString(level.tileW));
            map.setAttribute("tileheight", Integer.toString(level.tileH));
            map.setAttribute("infinite", "0");
            map.setAttribute("nextlayerid", Integer.toString(level.tileLayers.size() + level.objectLayers.size() + 1));
            map.setAttribute("nextobjectid", Integer.toString(level.nextEntityId));
            document.appendChild(map);

            for (final TileSet tileSet : level.tilesets) {
                final Element element = document.createElement("tileset");
                element.setAttribute("firstgid", Integer.toString(tileSet.firstGid));
                element.setAttribute("name", tileSet.name);
                element.setAttribute("tilewidth", Integer.toString(tileSet.tileW));
                element.setAttribute("tileheight", Integer.toString(tileSet.tileH));
                element.setAttribute("tilecount", Integer.toString(tileSet.tileCount));
                element.setAttribute("columns", Integer.toString(tileSet.columns));
                element.setAttribute("margin", Integer.toString(tileSet.margin));
                element.setAttribute("spacing", Integer.toString(tileSet.spacing));
                if (!tileSet.image.isEmpty()) {
                    final Element image = document.createElement("image");
                    image.setAttribute("source", relativeTo(file.toAbsolutePath().getParent(),
                            resolve(project.root, tileSet.image)));
                    element.appendChild(image);
                }
                appendWangSets(document, element, tileSet.wangsets);
                for (final Map.Entry<Integer, TileAnimation> entry : tileSet.animations.entrySet()) {
                    final Element tile = document.createElement("tile");
                    tile.setAttribute("id", Integer.toString(entry.getKey()));
                    final Element animation = document.createElement("animation");
                    for (final TileAnimation.Frame frame : entry.getValue().frames) {
                        final Element frameElement = document.createElement("frame");
                        frameElement.setAttribute("tileid", Integer.toString(frame.tileId()));
                        frameElement.setAttribute("duration", Integer.toString(frame.durationMs()));
                        animation.appendChild(frameElement);
                    }
                    tile.appendChild(animation);
                    element.appendChild(tile);
                }
                map.appendChild(element);
            }
            int layerId = 1;
            for (final TileLayer layer : level.tileLayers) {
                final Element element = document.createElement("layer");
                element.setAttribute("id", Integer.toString(layerId++));
                element.setAttribute("name", layer.name);
                element.setAttribute("width", Integer.toString(level.width));
                element.setAttribute("height", Integer.toString(level.height));
                if (!layer.visible) element.setAttribute("visible", "0");
                if (layer.opacity != 1) element.setAttribute("opacity", Double.toString(layer.opacity));
                final Element data = document.createElement("data");
                data.setAttribute("encoding", "csv");
                final StringBuilder csv = new StringBuilder();
                for (int i = 0; i < layer.tiles.length; i++) {
                    if (i != 0) csv.append(',');
                    csv.append(layer.tiles[i]);
                    if (i % 32 == 31) csv.append('\n');
                }
                data.setTextContent(csv.toString());
                element.appendChild(data);
                map.appendChild(element);
            }
            for (final ObjectLayer layer : level.objectLayers) {
                final Element element = document.createElement("objectgroup");
                element.setAttribute("id", Integer.toString(layerId++));
                element.setAttribute("name", layer.name);
                if (!layer.visible) element.setAttribute("visible", "0");
                for (final Entity entity : layer.entities) {
                    final Element object = document.createElement("object");
                    object.setAttribute("id", Integer.toString(entity.id));
                    object.setAttribute("name", entity.attributes.getOrDefault("Name", entity.def));
                    object.setAttribute("type", entity.def);
                    object.setAttribute("x", Double.toString(entity.x));
                    object.setAttribute("y", Double.toString(entity.y));
                    object.setAttribute("width", Double.toString(entity.w));
                    object.setAttribute("height", Double.toString(entity.h));
                    if (entity.rotation != 0) object.setAttribute("rotation", Double.toString(entity.rotation));
                    if (entity.attributes.containsKey("gid")) {
                        object.setAttribute("gid", entity.attributes.get("gid"));
                    }
                    if (!entity.templatePath.isEmpty()) {
                        object.setAttribute("template", relativeTo(file.toAbsolutePath().getParent(),
                                resolve(project.root, entity.templatePath)));
                    }
                    final Element properties = document.createElement("properties");
                    for (final Map.Entry<String, String> attr : entity.attributes.entrySet()) {
                        if ("gid".equals(attr.getKey())) continue;
                        final Element property = document.createElement("property");
                        property.setAttribute("name", attr.getKey());
                        property.setAttribute("type", "string");
                        property.setAttribute("value", attr.getValue());
                        properties.appendChild(property);
                    }
                    object.appendChild(properties);
                    element.appendChild(object);
                }
                map.appendChild(element);
            }
            Files.createDirectories(file.toAbsolutePath().getParent());
            final Transformer transformer = TransformerFactory.newInstance().newTransformer();
            transformer.setOutputProperty(OutputKeys.INDENT, "yes");
            transformer.setOutputProperty(OutputKeys.ENCODING, "UTF-8");
            transformer.transform(new DOMSource(document), new StreamResult(file.toFile()));
        } catch (IOException exception) {
            throw exception;
        } catch (Exception exception) {
            throw new IOException("TMX konnte nicht geschrieben werden: " + file, exception);
        }
    }

    // ── Tilesets und Templates ───────────────────────────────────────────────

    private TileSet readTileSetFile(Path file, int firstGid, Project project) throws IOException {
        final String name = file.getFileName().toString().toLowerCase();
        if (name.endsWith(".tsj") || name.endsWith(".json")) {
            final JsonNode json = mapper.readTree(file.toFile());
            return readTileSetJson(json, firstGid, file.getParent(), project);
        }
        if (name.endsWith(".tsx")) {
            try {
                return readTileSetElement(parseXml(file).getDocumentElement(), firstGid, file.getParent(), project);
            } catch (Exception exception) {
                throw new IOException("TSX konnte nicht gelesen werden: " + file, exception);
            }
        }
        throw new IOException("Unbekanntes Tileset-Format: " + file);
    }

    private TileSet readTileSetJson(JsonNode json, int firstGid, Path baseDir, Project project) {
        final TileSet tileSet = new TileSet();
        tileSet.firstGid = firstGid;
        tileSet.name = text(json, "name", "tileset");
        tileSet.tileW = integer(json, "tilewidth", 32);
        tileSet.tileH = integer(json, "tileheight", 32);
        tileSet.columns = integer(json, "columns", 0);
        tileSet.tileCount = integer(json, "tilecount", 0);
        tileSet.margin = integer(json, "margin", 0);
        tileSet.spacing = integer(json, "spacing", 0);
        final String image = text(json, "image", "");
        if (!image.isEmpty()) tileSet.image = projectRelative(project, resolve(baseDir, image));
        if (json.has("wangsets") && json.get("wangsets").isArray()) tileSet.wangsets = json.get("wangsets").deepCopy();
        for (final JsonNode tile : array(json, "tiles")) {
            if (!tile.has("animation")) continue;
            final TileAnimation animation = new TileAnimation();
            for (final JsonNode frame : array(tile, "animation")) {
                animation.frames.add(new TileAnimation.Frame(integer(frame, "tileid", 0),
                        integer(frame, "duration", 100)));
            }
            tileSet.animations.put(integer(tile, "id", 0), animation);
        }
        return tileSet;
    }

    private TileSet readTileSetElement(Element root, int firstGid, Path baseDir, Project project) {
        final TileSet tileSet = new TileSet();
        tileSet.firstGid = firstGid;
        tileSet.name = attr(root, "name", "tileset");
        tileSet.tileW = attrInt(root, "tilewidth", 32);
        tileSet.tileH = attrInt(root, "tileheight", 32);
        tileSet.columns = attrInt(root, "columns", 0);
        tileSet.tileCount = attrInt(root, "tilecount", 0);
        tileSet.margin = attrInt(root, "margin", 0);
        tileSet.spacing = attrInt(root, "spacing", 0);
        for (final Element image : children(root, "image")) {
            final String source = image.getAttribute("source");
            if (!source.isEmpty()) tileSet.image = projectRelative(project, resolve(baseDir, source));
        }
        for (final Element wangsets : children(root, "wangsets")) {
            tileSet.wangsets = wangSetsFromXml(wangsets);
        }
        for (final Element tile : children(root, "tile")) {
            final Element animation = firstChild(tile, "animation");
            if (animation == null) continue;
            final TileAnimation value = new TileAnimation();
            for (final Element frame : children(animation, "frame")) {
                value.frames.add(new TileAnimation.Frame(attrInt(frame, "tileid", 0),
                        attrInt(frame, "duration", 100)));
            }
            tileSet.animations.put(attrInt(tile, "id", 0), value);
        }
        return tileSet;
    }

    private ObjectNode writeTileSetJson(TileSet tileSet, Path projectRoot) {
        final ObjectNode node = mapper.createObjectNode();
        node.put("name", tileSet.name);
        node.put("image", tileSet.image);
        node.put("firstgid", tileSet.firstGid);
        node.put("tilewidth", tileSet.tileW);
        node.put("tileheight", tileSet.tileH);
        node.put("columns", tileSet.columns);
        node.put("tilecount", tileSet.tileCount);
        node.put("margin", tileSet.margin);
        node.put("spacing", tileSet.spacing);
        if (tileSet.wangsets != null) node.set("wangsets", tileSet.wangsets.deepCopy());
        if (!tileSet.animations.isEmpty()) {
            final ArrayNode tiles = node.putArray("tiles");
            for (final Map.Entry<Integer, TileAnimation> entry : tileSet.animations.entrySet()) {
                final ObjectNode tile = tiles.addObject();
                tile.put("id", entry.getKey());
                final ArrayNode animation = tile.putArray("animation");
                for (final TileAnimation.Frame frame : entry.getValue().frames) {
                    final ObjectNode frameNode = animation.addObject();
                    frameNode.put("tileid", frame.tileId());
                    frameNode.put("duration", frame.durationMs());
                }
            }
        }
        return node;
    }

    private Entity readTemplate(Path file, Project project) throws IOException {
        final String name = file.getFileName().toString().toLowerCase();
        final Entity entity = new Entity();
        if (name.endsWith(".tx")) {
            try {
                final Document document = parseXml(file);
                final Element root = document.getDocumentElement();
                final Element object = firstChild(root, "object");
                if (object == null) throw new IOException("Template ohne <object>: " + file);
                entity.def = object.getAttribute("type");
                if (entity.def.isEmpty()) entity.def = object.getAttribute("class");
                entity.w = attrDouble(object, "width", 0);
                entity.h = attrDouble(object, "height", 0);
                entity.rotation = attrDouble(object, "rotation", 0);
                final String gid = object.getAttribute("gid");
                if (!gid.isEmpty()) entity.attributes.put("gid", Integer.toString(parseInt(gid, 0) & GID_MASK));
                for (final Element properties : children(object, "properties")) {
                    for (final Element property : children(properties, "property")) {
                        final String key = property.getAttribute("name");
                        if (key.isEmpty()) continue;
                        entity.attributes.put(key, property.getAttribute("value"));
                        addPropertyDef(project, entity.def.isBlank() ? "Objekt" : entity.def, key,
                                property.getAttribute("type"));
                    }
                }
            } catch (Exception exception) {
                throw new IOException("Template konnte nicht gelesen werden: " + file, exception);
            }
            return entity;
        }
        if (name.endsWith(".tj") || name.endsWith(".json")) {
            final JsonNode root = mapper.readTree(file.toFile());
            final JsonNode object = root.has("object") ? root.get("object") : root;
            entity.def = text(object, "type", "");
            if (entity.def.isEmpty()) entity.def = text(object, "class", "");
            entity.w = number(object, "width", 0);
            entity.h = number(object, "height", 0);
            entity.rotation = number(object, "rotation", 0);
            if (object.has("gid")) entity.attributes.put("gid", Integer.toString(integer(object, "gid", 0) & GID_MASK));
            readProperties(object.get("properties"), entity, project, entity.def.isBlank() ? "Objekt" : entity.def);
            return entity;
        }
        throw new IOException("Unbekanntes Template-Format: " + file);
    }

    // ── Daten, Properties, Wang ──────────────────────────────────────────────

    private int[] decodeLayerData(JsonNode layer, int width, int height) throws IOException {
        final int[] tiles = new int[width * height];
        final JsonNode data = layer.get("data");
        if (data == null) throw new IOException("Kachelebene ohne Daten");
        if (data.isArray()) {
            for (int i = 0; i < Math.min(data.size(), tiles.length); i++) tiles[i] = data.get(i).asInt() & GID_MASK;
            return tiles;
        }
        if (!data.isTextual()) throw new IOException("Unbekanntes Kacheldatenformat");
        final String encoding = text(layer, "encoding", "");
        final String compression = text(layer, "compression", "");
        byte[] bytes = Base64.getDecoder().decode(data.asText());
        if (!"base64".equals(encoding)) throw new IOException("Nur CSV und Base64 werden unterstützt");
        bytes = decompress(bytes, compression);
        if (bytes.length < tiles.length * 4) throw new IOException("Base64-Kacheldaten sind zu kurz");
        for (int i = 0; i < tiles.length; i++) {
            final int offset = i * 4;
            tiles[i] = ((bytes[offset] & 0xff) | ((bytes[offset + 1] & 0xff) << 8)
                    | ((bytes[offset + 2] & 0xff) << 16) | ((bytes[offset + 3] & 0xff) << 24)) & GID_MASK;
        }
        return tiles;
    }

    private int[] decodeTmxData(Element layer, int width, int height) throws Exception {
        final int[] tiles = new int[width * height];
        final Element data = firstChild(layer, "data");
        if (data == null) throw new IOException("TMX-Kachelebene ohne <data>");
        final String encoding = data.getAttribute("encoding");
        final String compression = data.getAttribute("compression");
        if (encoding.isEmpty()) {
            int index = 0;
            for (final Element tile : children(data, "tile")) {
                if (index >= tiles.length) break;
                tiles[index++] = attrInt(tile, "gid", 0) & GID_MASK;
            }
            return tiles;
        }
        if ("csv".equals(encoding)) {
            final String[] parts = data.getTextContent().trim().split(",");
            for (int i = 0; i < Math.min(parts.length, tiles.length); i++) {
                tiles[i] = parseInt(parts[i].trim(), 0) & GID_MASK;
            }
            return tiles;
        }
        if ("base64".equals(encoding)) {
            byte[] bytes = Base64.getDecoder().decode(data.getTextContent().replaceAll("\\s+", ""));
            bytes = decompress(bytes, compression);
            if (bytes.length < tiles.length * 4) throw new IOException("Base64-Kacheldaten sind zu kurz");
            for (int i = 0; i < tiles.length; i++) {
                final int offset = i * 4;
                tiles[i] = ((bytes[offset] & 0xff) | ((bytes[offset + 1] & 0xff) << 8)
                        | ((bytes[offset + 2] & 0xff) << 16) | ((bytes[offset + 3] & 0xff) << 24)) & GID_MASK;
            }
            return tiles;
        }
        throw new IOException("Unbekannte TMX-Kodierung: " + encoding);
    }

    private static byte[] decompress(byte[] data, String compression) throws IOException {
        if (compression == null || compression.isEmpty() || "none".equals(compression)) return data;
        try (InputStream input = switch (compression) {
            case "zlib" -> new InflaterInputStream(new ByteArrayInputStream(data));
            case "gzip" -> new GZIPInputStream(new ByteArrayInputStream(data));
            default -> throw new IOException("Nicht unterstützte Kompression: " + compression);
        }) {
            final ByteArrayOutputStream output = new ByteArrayOutputStream();
            input.transferTo(output);
            return output.toByteArray();
        }
    }

    private void readProperties(JsonNode properties, Entity entity, Project project, String defName) {
        if (properties == null || !properties.isArray()) return;
        for (final JsonNode property : properties) {
            final String name = text(property, "name", "");
            if (name.isEmpty()) continue;
            final JsonNode value = property.get("value");
            entity.attributes.put(name, value == null ? "" : value.isTextual() ? value.asText() : value.toString());
            addPropertyDef(project, defName, name, text(property, "type", "string"));
        }
    }

    private void writeProperties(ObjectNode object, Entity entity, Project project) {
        final ArrayNode properties = object.putArray("properties");
        for (final Map.Entry<String, String> attribute : entity.attributes.entrySet()) {
            if ("gid".equals(attribute.getKey())) continue;
            final ObjectNode property = properties.addObject();
            property.put("name", attribute.getKey());
            property.put("type", "string");
            property.put("value", attribute.getValue());
        }
    }

    private void addPropertyDef(Project project, String defName, String name, String type) {
        project.ensureEntityDef(defName);
        final EntityDef def = project.findEntityDef(defName);
        if (def == null) return;
        for (final AttributeDef attribute : def.attributes) {
            if (attribute.name.equals(name)) return;
        }
        def.attributes.add(new AttributeDef(name, "file".equals(type) ? "asset" : type, ""));
    }

    private void mergeTileSet(Project project, TileSet tileSet) {
        if (project.findTileSet(tileSet.name) == null) project.tilesets.add(tileSet);
    }

    private JsonNode wangSetsFromXml(Element wangsets) {
        final ArrayNode sets = mapper.createArrayNode();
        for (final Element set : children(wangsets, "wangset")) {
            final ObjectNode node = sets.addObject();
            node.put("name", attr(set, "name", ""));
            node.put("type", attr(set, "type", "corner"));
            node.put("tile", attrInt(set, "tile", -1));
            final ArrayNode colors = node.putArray("colors");
            for (final Element color : children(set, "wangcolor")) {
                final ObjectNode colorNode = colors.addObject();
                colorNode.put("name", attr(color, "name", ""));
                colorNode.put("color", attr(color, "color", "#ff0000"));
                colorNode.put("tile", attrInt(color, "tile", -1));
                colorNode.put("probability", attrDouble(color, "probability", 1));
            }
            final ArrayNode tiles = node.putArray("wangtiles");
            for (final Element tile : children(set, "wangtile")) {
                final ObjectNode tileNode = tiles.addObject();
                tileNode.put("tileid", attrInt(tile, "tileid", 0));
                final ArrayNode wangid = tileNode.putArray("wangid");
                for (final String part : attr(tile, "wangid", "").split(",")) {
                    if (!part.isBlank()) wangid.add(parseInt(part.trim(), 0));
                }
            }
        }
        return sets;
    }

    private void appendWangSets(Document document, Element tileset, JsonNode wangsets) {
        if (wangsets == null || !wangsets.isArray()) return;
        final Element root = document.createElement("wangsets");
        for (final JsonNode set : wangsets) {
            final Element setElement = document.createElement("wangset");
            setElement.setAttribute("name", text(set, "name", ""));
            setElement.setAttribute("type", text(set, "type", "corner"));
            setElement.setAttribute("tile", Integer.toString(integer(set, "tile", -1)));
            for (final JsonNode color : array(set, "colors")) {
                final Element colorElement = document.createElement("wangcolor");
                colorElement.setAttribute("name", text(color, "name", ""));
                colorElement.setAttribute("color", text(color, "color", "#ff0000"));
                colorElement.setAttribute("tile", Integer.toString(integer(color, "tile", -1)));
                colorElement.setAttribute("probability", Double.toString(number(color, "probability", 1)));
                setElement.appendChild(colorElement);
            }
            for (final JsonNode tile : array(set, "wangtiles")) {
                final Element tileElement = document.createElement("wangtile");
                tileElement.setAttribute("tileid", Integer.toString(integer(tile, "tileid", 0)));
                final StringBuilder ids = new StringBuilder();
                for (final JsonNode id : array(tile, "wangid")) {
                    if (!ids.isEmpty()) ids.append(',');
                    ids.append(id.asInt());
                }
                tileElement.setAttribute("wangid", ids.toString());
                setElement.appendChild(tileElement);
            }
            root.appendChild(setElement);
        }
        tileset.appendChild(root);
    }

    // ── Pfade und XML-Helfer ─────────────────────────────────────────────────

    private static String stem(Path file) {
        final String name = file.getFileName().toString();
        final int dot = name.lastIndexOf('.');
        return dot < 0 ? name : name.substring(0, dot);
    }

    private static Path resolve(Path base, String value) {
        final Path path = Path.of(value);
        return path.isAbsolute() ? path : base.resolve(path).normalize();
    }

    private static String projectRelative(Project project, Path path) {
        if (project.root == null) return path.toString();
        try {
            return project.root.toAbsolutePath().relativize(path.toAbsolutePath()).toString().replace('\\', '/');
        } catch (IllegalArgumentException exception) {
            return path.toAbsolutePath().toString();
        }
    }

    private static String relativeTo(Path from, Path target) {
        try {
            return from.toAbsolutePath().relativize(target.toAbsolutePath()).toString().replace('\\', '/');
        } catch (IllegalArgumentException exception) {
            return target.toAbsolutePath().toString();
        }
    }

    private static String text(JsonNode node, String field, String fallback) {
        if (node == null || !node.hasNonNull(field)) return fallback;
        return node.get(field).asText();
    }

    private static int integer(JsonNode node, String field, int fallback) {
        if (node == null || !node.hasNonNull(field)) return fallback;
        return node.get(field).asInt(fallback);
    }

    private static double number(JsonNode node, String field, double fallback) {
        if (node == null || !node.hasNonNull(field)) return fallback;
        return node.get(field).asDouble(fallback);
    }

    private static boolean bool(JsonNode node, String field, boolean fallback) {
        if (node == null || !node.hasNonNull(field)) return fallback;
        return node.get(field).asBoolean(fallback);
    }

    private static Iterable<JsonNode> array(JsonNode node, String field) {
        if (node == null) return List.of();
        final JsonNode value = node.get(field);
        return value != null && value.isArray() ? value : List.of();
    }

    private static Iterable<JsonNode> array(JsonNode node) {
        return node != null && node.isArray() ? node : List.of();
    }

    private static Document parseXml(Path file) throws Exception {
        final DocumentBuilderFactory factory = DocumentBuilderFactory.newInstance();
        factory.setFeature(XMLConstants.FEATURE_SECURE_PROCESSING, true);
        factory.setFeature("http://apache.org/xml/features/disallow-doctype-decl", true);
        return factory.newDocumentBuilder().parse(file.toFile());
    }

    private static Element firstChild(Element parent, String name) {
        final NodeList nodes = parent.getChildNodes();
        for (int i = 0; i < nodes.getLength(); i++) {
            final Node node = nodes.item(i);
            if (node instanceof Element element && element.getTagName().equals(name)) return element;
        }
        return null;
    }

    private static List<Element> children(Element parent, String name) {
        final List<Element> result = new ArrayList<>();
        final NodeList nodes = parent.getChildNodes();
        for (int i = 0; i < nodes.getLength(); i++) {
            final Node node = nodes.item(i);
            if (node instanceof Element element && (name == null || element.getTagName().equals(name))) {
                result.add(element);
            }
        }
        return result;
    }

    private static String attr(Element element, String name, String fallback) {
        final String value = element.getAttribute(name);
        return value == null || value.isEmpty() ? fallback : value;
    }

    private static int attrInt(Element element, String name, int fallback) {
        return parseInt(element.getAttribute(name), fallback);
    }

    private static double attrDouble(Element element, String name, double fallback) {
        final String value = element.getAttribute(name);
        if (value == null || value.isEmpty()) return fallback;
        try {
            return Double.parseDouble(value);
        } catch (NumberFormatException exception) {
            return fallback;
        }
    }

    private static boolean attrBool(Element element, String name, boolean fallback) {
        final String value = element.getAttribute(name);
        if (value == null || value.isEmpty()) return fallback;
        return "1".equals(value) || "true".equalsIgnoreCase(value);
    }

    private static int parseInt(String value, int fallback) {
        if (value == null || value.isBlank()) return fallback;
        try {
            return Integer.parseInt(value.trim());
        } catch (NumberFormatException exception) {
            return fallback;
        }
    }
}
