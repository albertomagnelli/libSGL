package de.sgl.editor.ui.panels;

import de.sgl.editor.model.Document;
import de.sgl.editor.service.ProjectService;
import de.sgl.editor.ui.dialogs.FileDialogs;

import jakarta.enterprise.context.Dependent;
import jakarta.inject.Inject;

import javafx.collections.ListChangeListener;
import javafx.fxml.FXML;
import javafx.scene.control.Label;
import javafx.scene.control.ListView;
import javafx.scene.input.MouseButton;

import java.nio.file.Path;

@Dependent
public class ProjectPanelController {
    @Inject
    ProjectService projects;
    @Inject
    FileDialogs dialogs;

    @FXML private Label projectName;
    @FXML private Label projectPath;
    @FXML private ListView<String> levels;
    @FXML private ListView<String> tilesets;

    @FXML
    public void initialize() {
        projects.project().addListener((observable, oldValue, newValue) -> refresh());
        projects.activeDocument().addListener((observable, oldValue, newValue) -> refresh());
        projects.tilesetsRevision().addListener((observable, oldValue, newValue) -> refresh());
        projects.documents().addListener((ListChangeListener<Document>) change -> refresh());
        levels.setOnMouseClicked(event -> {
            if (event.getButton() == MouseButton.PRIMARY && event.getClickCount() == 2) {
                final String title = levels.getSelectionModel().getSelectedItem();
                if (title == null) return;
                for (final Document document : projects.documents()) {
                    if (document.title.equals(title)) {
                        projects.activeDocument().set(document);
                        break;
                    }
                }
            }
        });
        refresh();
    }

    @FXML
    private void onNewLevel() {
        projects.addLevel();
    }

    @FXML
    private void onOpenProject() {
        dialogs.openProject();
    }

    private void refresh() {
        final var project = projects.currentProject();
        projectName.setText(project == null ? "Kein Projekt" : project.name);
        final String path = project == null || project.root == null ? "" : project.root.toString();
        projectPath.setText(path);
        projectPath.setTooltip(path.isEmpty() ? null : new javafx.scene.control.Tooltip(path));
        levels.getItems().clear();
        for (final Document document : projects.documents()) levels.getItems().add(document.title);
        final Document active = projects.activeDocument().get();
        if (active != null) levels.getSelectionModel().select(active.title);
        tilesets.getItems().clear();
        if (project != null) {
            for (final var tileSet : project.tilesets) tilesets.getItems().add(tileSet.name);
        }
    }
}
