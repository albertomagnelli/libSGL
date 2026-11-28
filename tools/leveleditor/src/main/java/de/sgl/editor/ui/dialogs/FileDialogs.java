package de.sgl.editor.ui.dialogs;

import de.sgl.editor.model.Document;
import de.sgl.editor.service.ProjectService;
import de.sgl.editor.service.StageService;

import jakarta.enterprise.context.ApplicationScoped;
import jakarta.inject.Inject;

import javafx.geometry.Insets;
import javafx.scene.control.*;
import javafx.scene.layout.GridPane;
import javafx.stage.DirectoryChooser;
import javafx.stage.FileChooser;
import javafx.stage.Stage;

import java.io.File;
import java.nio.file.Path;

@ApplicationScoped
public class FileDialogs {
    @Inject
    ProjectService projects;
    @Inject
    StageService stages;

    private Stage owner() {
        return stages.main();
    }

    public void openProject() {
        if (owner() == null) return;
        final FileChooser chooser = new FileChooser();
        chooser.setTitle("Projekt öffnen");
        chooser.getExtensionFilters().add(new FileChooser.ExtensionFilter("sgl-Projekt", "*.sglproj"));
        final File file = chooser.showOpenDialog(owner());
        if (file != null) projects.openProject(file.toPath());
    }

    public void saveProjectAs() {
        if (owner() == null) return;
        final FileChooser chooser = new FileChooser();
        chooser.setTitle("Projekt speichern unter");
        chooser.getExtensionFilters().add(new FileChooser.ExtensionFilter("sgl-Projekt", "*.sglproj"));
        chooser.setInitialFileName((projects.currentProject() == null ? "project" : projects.currentProject().name) + ".sglproj");
        final File file = chooser.showSaveDialog(owner());
        if (file != null) projects.saveProjectAs(file.toPath());
    }

    public void importLevel() {
        if (owner() == null) return;
        final FileChooser chooser = new FileChooser();
        chooser.setTitle("Level importieren");
        chooser.getExtensionFilters().add(new FileChooser.ExtensionFilter("Tiled", "*.tmj", "*.tmx"));
        final File file = chooser.showOpenDialog(owner());
        if (file != null) projects.importLevel(file.toPath());
    }

    public void exportLevel() {
        final Document document = projects.activeDocument().get();
        if (owner() == null || document == null) return;
        final FileChooser chooser = new FileChooser();
        chooser.setTitle("Level exportieren");
        chooser.setInitialFileName(document.title + ".tmj");
        chooser.getExtensionFilters().add(new FileChooser.ExtensionFilter("Tiled JSON", "*.tmj"));
        chooser.getExtensionFilters().add(new FileChooser.ExtensionFilter("Tiled XML", "*.tmx"));
        final File file = chooser.showSaveDialog(owner());
        if (file != null) projects.exportLevel(file.toPath(), document);
    }

    public void importTileset() {
        if (owner() == null) return;
        final FileChooser chooser = new FileChooser();
        chooser.setTitle("Tileset importieren");
        chooser.getExtensionFilters().add(new FileChooser.ExtensionFilter("Bilder", "*.png", "*.jpg", "*.jpeg", "*.bmp", "*.tga"));
        final File file = chooser.showOpenDialog(owner());
        if (file != null) projects.importTileset(file.toPath());
    }

    public void newProject() {
        if (owner() == null) return;
        final Dialog<ButtonType> dialog = new Dialog<>();
        dialog.initOwner(owner());
        dialog.setTitle("Neues Projekt");
        dialog.getDialogPane().getButtonTypes().addAll(ButtonType.OK, ButtonType.CANCEL);
        final TextField name = new TextField("MeinProjekt");
        final TextField directory = new TextField(System.getProperty("user.home"));
        final Button browse = new Button("…");
        browse.setOnAction(event -> {
            final DirectoryChooser chooser = new DirectoryChooser();
            chooser.setTitle("Projektverzeichnis");
            final File file = chooser.showDialog(owner());
            if (file != null) directory.setText(file.getAbsolutePath());
        });
        final GridPane grid = new GridPane();
        grid.setHgap(8);
        grid.setVgap(8);
        grid.setPadding(new Insets(10));
        grid.add(new Label("Name"), 0, 0);
        grid.add(name, 1, 0);
        grid.add(new Label("Verzeichnis"), 0, 1);
        grid.add(directory, 1, 1);
        grid.add(browse, 2, 1);
        dialog.getDialogPane().setContent(grid);
        if (dialog.showAndWait().orElse(ButtonType.CANCEL) == ButtonType.OK) {
            projects.newProject(Path.of(directory.getText()), name.getText().isBlank() ? "Projekt" : name.getText());
        }
    }
}
