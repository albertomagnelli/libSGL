package de.sgl.editor.ui.panels;

import de.sgl.editor.model.Document;
import de.sgl.editor.service.ProjectService;
import de.sgl.editor.ui.dialogs.FileDialogs;

import jakarta.enterprise.context.Dependent;
import jakarta.inject.Inject;

import javafx.fxml.FXML;
import javafx.scene.control.ListView;

@Dependent
public class AssetsPanelController {
    @Inject ProjectService projects;
    @Inject FileDialogs dialogs;

    @FXML private ListView<String> assets;

    @FXML
    public void initialize() {
        projects.project().addListener((observable, oldValue, newValue) -> refresh());
        projects.tilesetsRevision().addListener((observable, oldValue, newValue) -> refresh());
        projects.documents().addListener((javafx.collections.ListChangeListener<Document>) change -> refresh());
        refresh();
    }

    @FXML private void onTileset() { dialogs.importTileset(); }
    @FXML private void onLevel() { dialogs.importLevel(); }

    private void refresh() {
        assets.getItems().clear();
        final var project = projects.currentProject();
        if (project != null) {
            for (final var tileSet : project.tilesets) {
                assets.getItems().add("Tileset · " + tileSet.name + "  (" + tileSet.image + ")");
            }
            for (final var def : project.entityDefs) {
                assets.getItems().add("Entity · " + def.name);
            }
        }
        for (final Document document : projects.documents()) {
            assets.getItems().add("Level · " + document.title + "  (" + document.path + ")");
        }
    }
}
