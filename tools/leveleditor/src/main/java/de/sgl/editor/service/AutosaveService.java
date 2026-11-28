package de.sgl.editor.service;

import de.sgl.editor.io.TiledIo;
import de.sgl.editor.model.Document;

import jakarta.annotation.PreDestroy;
import jakarta.enterprise.context.ApplicationScoped;
import jakarta.inject.Inject;

import javafx.animation.KeyFrame;
import javafx.animation.Timeline;
import javafx.util.Duration;

import java.nio.file.Files;
import java.nio.file.Path;

@ApplicationScoped
public class AutosaveService {
    private Timeline timeline;

    @Inject
    TiledIo io;
    @Inject
    ProjectService projects;
    @Inject
    LogService log;

    public void start() {
        if (timeline != null) return;
        timeline = new Timeline(new KeyFrame(Duration.seconds(30), event -> save()));
        timeline.setCycleCount(Timeline.INDEFINITE);
        timeline.play();
    }

    private void save() {
        final var project = projects.currentProject();
        if (project == null || project.root == null) return;
        try {
            final Path directory = project.root.resolve(".sgl/autosave");
            Files.createDirectories(directory);
            boolean saved = false;
            for (final Document document : projects.documents()) {
                if (!document.dirty) continue;
                io.writeLevel(directory.resolve(document.title + ".tmj"), document.level, project);
                saved = true;
            }
            if (saved) log.log("Autosave geschrieben.");
        } catch (Exception exception) {
            log.log("Autosave fehlgeschlagen: " + exception.getMessage());
        }
    }

    @PreDestroy
    public void stop() {
        if (timeline != null) timeline.stop();
    }
}
