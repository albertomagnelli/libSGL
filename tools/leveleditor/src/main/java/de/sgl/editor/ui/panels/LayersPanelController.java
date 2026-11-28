package de.sgl.editor.ui.panels;

import de.sgl.editor.model.Entity;
import de.sgl.editor.model.Level;
import de.sgl.editor.model.ObjectLayer;
import de.sgl.editor.model.TileLayer;
import de.sgl.editor.service.EditorState;
import de.sgl.editor.service.ProjectService;

import jakarta.enterprise.context.Dependent;
import jakarta.inject.Inject;

import javafx.collections.ListChangeListener;
import javafx.fxml.FXML;
import javafx.scene.control.CheckBox;
import javafx.scene.control.Label;
import javafx.scene.control.ListCell;
import javafx.scene.control.ListView;
import javafx.scene.layout.HBox;
import javafx.scene.layout.Priority;

@Dependent
public class LayersPanelController {
    public record LayerRow(boolean object, int index) {}

    @Inject ProjectService projects;
    @Inject EditorState state;

    @FXML private ListView<LayerRow> rows;

    @FXML
    public void initialize() {
        rows.setCellFactory(list -> new ListCell<>() {
            @Override protected void updateItem(LayerRow item, boolean empty) {
                super.updateItem(item, empty);
                if (empty || item == null) {
                    setText(null);
                    setGraphic(null);
                    return;
                }
                final Level level = projects.activeLevel();
                if (level == null) return;
                final String name;
                final boolean visible;
                final boolean locked;
                if (item.object()) {
                    final ObjectLayer layer = level.objectLayers.get(item.index());
                    name = layer.name;
                    visible = layer.visible;
                    locked = layer.locked;
                } else {
                    final TileLayer layer = level.tileLayers.get(item.index());
                    name = layer.name;
                    visible = layer.visible;
                    locked = layer.locked;
                }
                final CheckBox eye = new CheckBox();
                eye.setSelected(visible);
                eye.setTooltip(new javafx.scene.control.Tooltip("Sichtbarkeit"));
                eye.setOnAction(event -> {
                    if (item.object()) level.objectLayers.get(item.index()).visible = eye.isSelected();
                    else level.tileLayers.get(item.index()).visible = eye.isSelected();
                    projects.markDirty();
                    projects.commit();
                });
                final CheckBox lock = new CheckBox();
                lock.setSelected(locked);
                lock.setTooltip(new javafx.scene.control.Tooltip("Sperren"));
                lock.setOnAction(event -> {
                    if (item.object()) level.objectLayers.get(item.index()).locked = lock.isSelected();
                    else level.tileLayers.get(item.index()).locked = lock.isSelected();
                    projects.markDirty();
                    projects.commit();
                });
                final Label label = new Label(name);
                label.setMaxWidth(Double.MAX_VALUE);
                HBox.setHgrow(label, Priority.ALWAYS);
                setGraphic(new HBox(6, eye, label, lock));
                setText(null);
            }
        });
        rows.getSelectionModel().selectedItemProperty().addListener((observable, oldValue, newValue) -> {
            if (newValue == null) return;
            state.objectLayerSelected().set(newValue.object());
            if (newValue.object()) state.activeObjectLayer().set(newValue.index());
            else state.activeTileLayer().set(newValue.index());
            state.selectedEntityId().set(-1);
        });
        projects.activeDocument().addListener((observable, oldValue, newValue) -> refresh());
        projects.project().addListener((observable, oldValue, newValue) -> refresh());
        projects.documents().addListener((ListChangeListener<de.sgl.editor.model.Document>) change -> refresh());
        refresh();
    }

    @FXML private void onAddTile() { final Level level = projects.activeLevel(); if (level == null) return;
        final TileLayer layer = new TileLayer(); layer.name = "Kacheln " + (level.tileLayers.size() + 1);
        layer.tiles = new int[level.width * level.height]; level.tileLayers.add(layer);
        state.objectLayerSelected().set(false); state.activeTileLayer().set(level.tileLayers.size() - 1);
        projects.markDirty(); projects.commit(); refresh(); }
    @FXML private void onAddObject() { final Level level = projects.activeLevel(); if (level == null) return;
        final ObjectLayer layer = new ObjectLayer(); layer.name = "Objekte " + (level.objectLayers.size() + 1);
        level.objectLayers.add(layer);
        state.objectLayerSelected().set(true); state.activeObjectLayer().set(level.objectLayers.size() - 1);
        projects.markDirty(); projects.commit(); refresh(); }
    @FXML private void onDuplicate() {
        final Level level = projects.activeLevel(); if (level == null) return;
        if (state.objectLayerSelected().get() && !level.objectLayers.isEmpty()) {
            final int index = Math.clamp(state.activeObjectLayer().get(), 0, level.objectLayers.size() - 1);
            final ObjectLayer copy = level.objectLayers.get(index).copy();
            copy.name += " Kopie";
            for (final Entity entity : copy.entities) entity.id = level.nextEntityId++;
            level.objectLayers.add(index + 1, copy);
            state.activeObjectLayer().set(index + 1);
        } else if (!level.tileLayers.isEmpty()) {
            final int index = Math.clamp(state.activeTileLayer().get(), 0, level.tileLayers.size() - 1);
            final TileLayer copy = level.tileLayers.get(index).copy();
            copy.name += " Kopie";
            level.tileLayers.add(index + 1, copy);
            state.activeTileLayer().set(index + 1);
        }
        projects.markDirty(); projects.commit(); refresh();
    }
    @FXML private void onDelete() {
        final Level level = projects.activeLevel(); if (level == null) return;
        if (state.objectLayerSelected().get() && !level.objectLayers.isEmpty()) {
            final int index = Math.clamp(state.activeObjectLayer().get(), 0, level.objectLayers.size() - 1);
            level.objectLayers.remove(index);
            state.activeObjectLayer().set(Math.clamp(index - 1, 0, Math.max(level.objectLayers.size() - 1, 0)));
        } else if (!level.tileLayers.isEmpty()) {
            final int index = Math.clamp(state.activeTileLayer().get(), 0, level.tileLayers.size() - 1);
            level.tileLayers.remove(index);
            state.activeTileLayer().set(Math.clamp(index - 1, 0, Math.max(level.tileLayers.size() - 1, 0)));
        }
        state.selectedEntityId().set(-1);
        projects.markDirty(); projects.commit(); refresh();
    }
    @FXML private void onUp() { move(-1); }
    @FXML private void onDown() { move(1); }

    private void move(int direction) {
        final Level level = projects.activeLevel(); if (level == null) return;
        if (state.objectLayerSelected().get()) {
            final int index = state.activeObjectLayer().get();
            final int target = index + direction;
            if (index < 0 || index >= level.objectLayers.size() || target < 0 || target >= level.objectLayers.size()) return;
            java.util.Collections.swap(level.objectLayers, index, target);
            state.activeObjectLayer().set(target);
        } else {
            final int index = state.activeTileLayer().get();
            final int target = index + direction;
            if (index < 0 || index >= level.tileLayers.size() || target < 0 || target >= level.tileLayers.size()) return;
            java.util.Collections.swap(level.tileLayers, index, target);
            state.activeTileLayer().set(target);
        }
        projects.markDirty(); projects.commit(); refresh();
    }

    private void refresh() {
        final Level level = projects.activeLevel();
        final boolean object = state.objectLayerSelected().get();
        final int index = object ? state.activeObjectLayer().get() : state.activeTileLayer().get();
        rows.getItems().clear();
        if (level != null) {
            for (int i = 0; i < level.tileLayers.size(); i++) rows.getItems().add(new LayerRow(false, i));
            for (int i = 0; i < level.objectLayers.size(); i++) rows.getItems().add(new LayerRow(true, i));
        }
        for (int i = 0; i < rows.getItems().size(); i++) {
            final LayerRow row = rows.getItems().get(i);
            if (row.object() == object && row.index() == index) {
                rows.getSelectionModel().select(i);
                break;
            }
        }
    }
}
