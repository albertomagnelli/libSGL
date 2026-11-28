package de.sgl.editor.ui.panels;

import de.sgl.editor.model.Document;
import de.sgl.editor.model.EntityDef;
import de.sgl.editor.model.Project;
import de.sgl.editor.service.EditorState;
import de.sgl.editor.service.ProjectService;

import jakarta.enterprise.context.Dependent;
import jakarta.inject.Inject;

import javafx.fxml.FXML;
import javafx.scene.control.ListCell;
import javafx.scene.control.ListView;
import javafx.scene.control.TextInputDialog;
import javafx.scene.paint.Color;

@Dependent
public class EntitiesPanelController {
    @Inject ProjectService projects;
    @Inject EditorState state;

    @FXML private ListView<EntityDef> entities;

    @FXML
    public void initialize() {
        entities.setCellFactory(list -> new ListCell<>() {
            @Override protected void updateItem(EntityDef item, boolean empty) {
                super.updateItem(item, empty);
                if (empty || item == null) {
                    setText("");
                    setTextFill(Color.WHITE);
                } else {
                    setText(item.name);
                    try {
                        setTextFill(Color.web(item.color));
                    } catch (IllegalArgumentException exception) {
                        setTextFill(Color.WHITE);
                    }
                }
            }
        });
        entities.getSelectionModel().selectedItemProperty().addListener((observable, oldValue, newValue) -> {
            if (newValue != null) {
                state.selectedEntityDef().set(newValue);
                state.setTool(EditorState.Tool.ENTITY);
            }
        });
        projects.project().addListener((observable, oldValue, newValue) -> refresh());
        projects.activeDocument().addListener((observable, oldValue, newValue) -> refresh());
        refresh();
    }

    @FXML
    private void onNewDefinition() {
        if (projects.currentProject() == null) return;
        final TextInputDialog dialog = new TextInputDialog("Objekt");
        dialog.setTitle("Neue Entity-Definition");
        dialog.setHeaderText("Name der Entity-Definition");
        dialog.showAndWait().ifPresent(name -> {
            if (name.isBlank()) return;
            projects.createEntityDef(name);
            refresh();
        });
    }

    private void refresh() {
        final Project project = projects.currentProject();
        entities.getItems().clear();
        if (project != null) entities.getItems().addAll(project.entityDefs);
        if (state.selectedEntityDef().get() != null) {
            entities.getSelectionModel().select(state.selectedEntityDef().get());
        }
    }
}
