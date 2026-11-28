package de.sgl.editor.service;

import de.sgl.editor.model.TileSet;

import jakarta.enterprise.context.ApplicationScoped;
import jakarta.inject.Inject;

import javafx.scene.image.Image;

import java.nio.file.Files;
import java.nio.file.Path;
import java.util.HashMap;
import java.util.Map;

@ApplicationScoped
public class TileImageCache {
    private final Map<String, Image> images = new HashMap<>();
    private final Map<String, Boolean> failed = new HashMap<>();

    @Inject
    ProjectService projects;

    public Image image(TileSet tileSet) {
        if (tileSet == null || tileSet.image == null || tileSet.image.isBlank()) return null;
        if (images.containsKey(tileSet.image)) return images.get(tileSet.image);
        if (failed.containsKey(tileSet.image)) return null;
        final var project = projects.currentProject();
        if (project == null || project.root == null) return null;
        Path file = Path.of(tileSet.image);
        if (!file.isAbsolute()) file = project.root.resolve(file);
        if (!Files.isRegularFile(file)) {
            failed.put(tileSet.image, true);
            return null;
        }
        final Image image = new Image(file.toUri().toString(), false);
        if (image.isError()) {
            failed.put(tileSet.image, true);
            return null;
        }
        images.put(tileSet.image, image);
        return image;
    }

    public void clear() {
        images.clear();
        failed.clear();
    }
}
