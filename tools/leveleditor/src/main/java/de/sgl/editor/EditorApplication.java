package de.sgl.editor;

import de.sgl.editor.cdi.CdiContext;
import de.sgl.editor.service.ProjectService;
import de.sgl.editor.service.StageService;
import de.sgl.editor.ui.FxLoader;
import de.sgl.editor.ui.MainWindowController;
import de.sgl.editor.ui.docking.DockManager;
import de.sgl.editor.ui.docking.PanelId;

import jakarta.enterprise.inject.se.SeContainer;
import jakarta.enterprise.inject.se.SeContainerInitializer;

import javafx.animation.PauseTransition;
import javafx.application.Application;
import javafx.application.Platform;
import javafx.embed.swing.SwingFXUtils;
import javafx.scene.Scene;
import javafx.scene.image.WritableImage;
import javafx.stage.Stage;
import javafx.util.Duration;

import javax.imageio.ImageIO;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;

public final class EditorApplication extends Application {
    private SeContainer container;
    private EditorOptions options;
    private MainWindowController controller;

    public static void main(String[] args) {
        launch(args);
    }

    @Override
    public void init() {
        options = EditorOptions.parse(getParameters().getRaw());
        container = SeContainerInitializer.newInstance().initialize();
        container.select(CdiContext.class).get().set(container);
    }

    @Override
    public void start(Stage stage) throws Exception {
        final FxLoader loader = container.select(FxLoader.class).get();
        final FxLoader.View<?> view = loader.load("/fxml/MainWindow.fxml");
        controller = (MainWindowController) view.controller();
        final Scene scene = new Scene(view.root(), 1440, 900);
        scene.getStylesheets().add(getClass().getResource("/css/editor.css").toExternalForm());
        stage.setTitle("sgl Leveleditor");
        stage.setScene(scene);
        stage.setMinWidth(980);
        stage.setMinHeight(640);
        container.select(StageService.class).get().setMain(stage);
        final StageService stages = container.select(StageService.class).get();
        stage.setOnCloseRequest(event -> {
            container.select(DockManager.class).get().saveLayout();
            stages.closeFloating();
        });
        stage.show();

        Platform.runLater(() -> {
            openInitialProject();
            if (options.floatPanel() != null) {
                final PanelId panel = PanelId.fromKey(options.floatPanel());
                if (panel != null) container.select(DockManager.class).get().floatPanel(panel);
            }
            if (options.play()) controller.startPreview();
            if (options.frame() != null) {
                final PauseTransition pause = new PauseTransition(Duration.millis(700));
                pause.setOnFinished(event -> {
                    snapshot(stage, options.frame());
                    container.select(DockManager.class).get().saveLayout();
                    Platform.exit();
                });
                pause.play();
            }
        });
    }

    private void openInitialProject() {
        final ProjectService projects = container.select(ProjectService.class).get();
        Path project = options.project();
        if (options.demo()) {
            project = Files.isDirectory(Path.of("demo")) ? Path.of("demo") : Path.of("tools/leveleditor/demo");
        }
        if (project == null) return;
        if (Files.isDirectory(project)) project = project.resolve("project.sglproj");
        if (!Files.isRegularFile(project)) {
            container.select(de.sgl.editor.service.LogService.class).get()
                    .log("Projekt nicht gefunden: " + project.toAbsolutePath());
            return;
        }
        projects.openProject(project.toAbsolutePath());
        if (options.level() != null) {
            projects.documents().stream().filter(document -> document.title.equals(options.level()))
                    .findFirst().ifPresent(document -> projects.activeDocument().set(document));
        }
        if (options.tab() >= 0) {
            final PanelId[] left = {PanelId.PROJECT, PanelId.PALETTE, PanelId.ENTITIES};
            final int index = Math.clamp(options.tab(), 0, left.length - 1);
            container.select(DockManager.class).get().activate(left[index]);
        }
    }

    private void snapshot(Stage stage, Path file) {
        try {
            Files.createDirectories(file.toAbsolutePath().getParent());
            final WritableImage image = stage.getScene().snapshot(null);
            ImageIO.write(SwingFXUtils.fromFXImage(image, null), "png", file.toFile());
            int index = 1;
            for (final Stage floating : container.select(StageService.class).get().floating()) {
                if (floating.getScene() == null || !floating.isShowing()) continue;
                final String name = file.getFileName().toString();
                final int dot = name.lastIndexOf('.');
                final String suffix = (dot < 0 ? name : name.substring(0, dot)) + "-panel" + index++
                        + (dot < 0 ? "" : name.substring(dot));
                final Path target = file.toAbsolutePath().getParent().resolve(suffix);
                final WritableImage floatingImage = floating.getScene().snapshot(null);
                ImageIO.write(SwingFXUtils.fromFXImage(floatingImage, null), "png", target.toFile());
            }
        } catch (IOException | RuntimeException exception) {
            System.err.println("Screenshot fehlgeschlagen: " + exception.getMessage());
        }
    }

    @Override
    public void stop() {
        if (container != null) {
            container.close();
        }
    }
}
