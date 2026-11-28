package de.sgl.editor.service;

import de.sgl.editor.io.TiledIo;
import de.sgl.editor.model.Document;
import de.sgl.editor.model.Entity;
import de.sgl.editor.model.EntityDef;
import de.sgl.editor.model.Level;
import de.sgl.editor.model.Project;
import de.sgl.editor.model.TileSet;

import jakarta.enterprise.context.ApplicationScoped;
import jakarta.inject.Inject;

import javafx.beans.property.*;
import javafx.collections.FXCollections;
import javafx.collections.ObservableList;

import javax.imageio.ImageIO;
import java.awt.image.BufferedImage;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Comparator;
import java.util.List;

@ApplicationScoped
public class ProjectService {
    private final ObjectProperty<Project> project = new SimpleObjectProperty<>();
    private final ObservableList<Document> documents = FXCollections.observableArrayList();
    private final ObjectProperty<Document> activeDocument = new SimpleObjectProperty<>();
    private final BooleanProperty dirty = new SimpleBooleanProperty(false);
    private final IntegerProperty tilesetsRevision = new SimpleIntegerProperty(0);
    private Path projectFile;

    @Inject
    TiledIo io;
    @Inject
    LogService log;

    public ObjectProperty<Project> project() { return project; }
    public ObservableList<Document> documents() { return documents; }
    public ObjectProperty<Document> activeDocument() { return activeDocument; }
    public BooleanProperty dirty() { return dirty; }
    public IntegerProperty tilesetsRevision() { return tilesetsRevision; }
    public Path projectFile() { return projectFile; }
    public Project currentProject() { return project.get(); }

    /** Signal an die Panels, dass sich die Tileset-Listen von Projekt oder aktivem Level geändert haben. */
    private void bumpTilesetsRevision() {
        tilesetsRevision.set(tilesetsRevision.get() + 1);
    }

    public Level activeLevel() {
        final Document document = activeDocument.get();
        return document == null ? null : document.level;
    }

    public void newProject(Path root, String name) {
        try {
            final Path directory = root.resolve(name);
            Files.createDirectories(directory.resolve("levels"));
            final Project value = Project.create(name, directory);
            final Level level = Project.createLevel(60, 34, 32, 32, "level01");
            level.tilesets.addAll(value.tilesets);
            final Document document = new Document(directory.resolve("levels/level01.tmj"), "level01", level);
            documents.setAll(document);
            activeDocument.set(document);
            project.set(value);
            projectFile = directory.resolve("project.sglproj");
            value.levels.clear();
            value.levels.add("levels/level01.tmj");
            saveProject();
            log.log("Projekt angelegt: " + name);
        } catch (IOException exception) {
            log.log("Projekt konnte nicht angelegt werden: " + exception.getMessage());
        }
    }

    public void openProject(Path file) {
        try {
            final Project value = new Project();
            io.readProject(file, value);
            project.set(value);
            projectFile = file;
            documents.clear();
            for (final String levelPath : value.levels) {
                final Path absolute = value.root.resolve(levelPath).normalize();
                try {
                    final Document document = new Document(absolute, stem(absolute), io.readLevel(absolute, value));
                    documents.add(document);
                } catch (IOException exception) {
                    log.log("Level '" + levelPath + "': " + exception.getMessage());
                }
            }
            activeDocument.set(documents.isEmpty() ? null : documents.getFirst());
            dirty.set(false);
            log.log("Projekt geöffnet: " + value.name + " (" + documents.size() + " Levels)");
        } catch (IOException exception) {
            log.log("Projekt konnte nicht geöffnet werden: " + exception.getMessage());
        }
    }

    public void saveProject() {
        final Project value = project.get();
        if (value == null || projectFile == null) return;
        try {
            for (final Document document : documents) {
                io.writeLevel(document.path, document.level, value);
                document.dirty = false;
            }
            value.levels.clear();
            documents.stream().map(document -> relative(value.root, document.path)).forEach(value.levels::add);
            io.writeProject(projectFile, value);
            dirty.set(false);
            log.log("Projekt gespeichert.");
        } catch (IOException exception) {
            log.log("Speichern fehlgeschlagen: " + exception.getMessage());
        }
    }

    public void saveProjectAs(Path file) {
        final Project value = project.get();
        if (value == null) return;
        final Path oldRoot = value.root;
        final Path newRoot = file.toAbsolutePath().getParent();
        if (oldRoot != null && !oldRoot.toAbsolutePath().equals(newRoot)) {
            try {
                copyProjectTree(oldRoot, newRoot);
                for (final Document document : documents) {
                    if (document.path.startsWith(oldRoot.toAbsolutePath())) {
                        document.path = newRoot.resolve(oldRoot.toAbsolutePath().relativize(document.path));
                    }
                }
                value.root = newRoot;
            } catch (IOException exception) {
                log.log("Projekt konnte nicht kopiert werden: " + exception.getMessage());
                return;
            }
        }
        projectFile = file;
        saveProject();
    }

    private static void copyProjectTree(Path source, Path target) throws IOException {
        try (var stream = Files.walk(source)) {
            for (final Path path : stream.toList()) {
                final Path relative = source.relativize(path);
                if (relative.toString().startsWith(".sgl")) continue;
                final Path destination = target.resolve(relative);
                if (Files.isDirectory(path)) {
                    Files.createDirectories(destination);
                } else if (Files.isRegularFile(path)) {
                    Files.createDirectories(destination.getParent());
                    Files.copy(path, destination, java.nio.file.StandardCopyOption.REPLACE_EXISTING);
                }
            }
        }
    }

    public void addLevel() {
        final Project value = project.get();
        if (value == null) return;
        String name;
        int index = 1;
        while (true) {
            name = "level" + (index < 10 ? "0" : "") + index;
            index++;
            final String candidate = name;
            if (documents.stream().noneMatch(document -> document.title.equals(candidate))) break;
        }
        final String levelName = name;
        final Level level = Project.createLevel(60, 34, 32, 32, levelName);
        level.tilesets.addAll(value.tilesets);
        final Document document = new Document(value.root.resolve("levels").resolve(levelName + ".tmj"), levelName, level);
        documents.add(document);
        activeDocument.set(document);
        dirty.set(true);
        log.log("Neues Level: " + levelName);
    }

    public void closeDocument(Document document) {
        final int index = Math.max(0, documents.indexOf(document) - 1);
        documents.remove(document);
        activeDocument.set(documents.isEmpty() ? null : documents.get(Math.min(index, documents.size() - 1)));
    }

    public void markDirty() {
        final Document document = activeDocument.get();
        if (document != null) document.dirty = true;
        dirty.set(true);
    }

    public void commit() {
        final Document document = activeDocument.get();
        if (document != null) document.undo.commit(document.level);
    }

    public void undo() {
        final Document document = activeDocument.get();
        if (document != null && document.undo.undo(document.level)) {
            document.dirty = true;
            dirty.set(true);
            bumpTilesetsRevision();
            log.log("Rückgängig.");
        }
    }

    public void redo() {
        final Document document = activeDocument.get();
        if (document != null && document.undo.redo(document.level)) {
            document.dirty = true;
            dirty.set(true);
            bumpTilesetsRevision();
            log.log("Wiederholt.");
        }
    }

    public void importLevel(Path file) {
        final Project value = project.get();
        if (value == null) return;
        try {
            final Level level = io.readLevel(file, value);
            final Path target = value.root.resolve("levels").resolve(stem(file) + ".tmj");
            final Document document = new Document(target, stem(file), level);
            documents.add(document);
            activeDocument.set(document);
            io.writeLevel(target, level, value);
            dirty.set(true);
            log.log("Level importiert: " + document.title);
        } catch (IOException exception) {
            log.log("Import fehlgeschlagen: " + exception.getMessage());
        }
    }

    public void exportLevel(Path file, Document document) {
        final Project value = project.get();
        if (value == null || document == null) return;
        try {
            io.writeLevel(file, document.level, value);
            log.log("Exportiert: " + file);
        } catch (IOException exception) {
            log.log("Export fehlgeschlagen: " + exception.getMessage());
        }
    }

    public void importTileset(Path imageFile) {
        final Project value = project.get();
        final Document document = activeDocument.get();
        if (value == null || document == null) return;
        try {
            final BufferedImage image = ImageIO.read(imageFile.toFile());
            if (image == null) {
                log.log("Bild konnte nicht gelesen werden: " + imageFile);
                return;
            }
            final TileSet tileSet = new TileSet();
            tileSet.name = stem(imageFile);
            tileSet.image = relative(value.root, imageFile);
            tileSet.tileW = Math.max(document.level.tileW, 1);
            tileSet.tileH = Math.max(document.level.tileH, 1);
            tileSet.columns = Math.max((image.getWidth() + tileSet.spacing) / (tileSet.tileW + tileSet.spacing), 1);
            tileSet.tileCount = tileSet.columns
                    * Math.max((image.getHeight() + tileSet.spacing) / (tileSet.tileH + tileSet.spacing), 1);
            tileSet.firstGid = value.tilesets.stream()
                    .mapToInt(existing -> existing.firstGid + Math.max(existing.tileCount, 1))
                    .max().orElse(1);
            value.tilesets.add(tileSet);
            document.level.tilesets.add(tileSet);
            value.ensureEntityDef("Startpunkt");
            dirty.set(true);
            document.dirty = true;
            bumpTilesetsRevision();
            log.log("Tileset importiert: " + tileSet.name + " (" + tileSet.tileCount + " Kacheln)");
        } catch (IOException exception) {
            log.log("Tileset-Import fehlgeschlagen: " + exception.getMessage());
        }
    }

    public void createEntityDef(String name) {
        final Project value = project.get();
        if (value == null) return;
        value.ensureEntityDef(name);
        dirty.set(true);
        log.log("Entity-Definition: " + name);
    }

    public void addEntity(Level level, EntityDef def, int cellX, int cellY) {
        if (level.objectLayers.isEmpty() || def == null) return;
        final var layer = level.objectLayers.get(Math.min(Math.max(0, 0), level.objectLayers.size() - 1));
        final Entity entity = new Entity();
        entity.id = level.nextEntityId++;
        entity.def = def.name;
        entity.x = cellX * (double) level.tileW;
        entity.y = cellY * (double) level.tileH;
        entity.w = level.tileW;
        entity.h = level.tileH;
        for (final var attribute : def.attributes) entity.attributes.put(attribute.name, attribute.defaultValue);
        layer.entities.add(entity);
        markDirty();
        commit();
    }

    private static String stem(Path file) {
        final String name = file.getFileName().toString();
        final int dot = name.lastIndexOf('.');
        return dot < 0 ? name : name.substring(0, dot);
    }

    private static String relative(Path root, Path file) {
        try {
            return root.toAbsolutePath().relativize(file.toAbsolutePath()).toString().replace('\\', '/');
        } catch (IllegalArgumentException exception) {
            return file.toAbsolutePath().toString();
        }
    }
}
