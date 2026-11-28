package de.sgl.editor.ui.panels;

import de.sgl.editor.model.*;
import de.sgl.editor.service.EditorState;
import de.sgl.editor.service.ProjectService;

import jakarta.enterprise.context.Dependent;
import jakarta.inject.Inject;

import javafx.fxml.FXML;
import javafx.geometry.Insets;
import javafx.scene.Node;
import javafx.scene.control.*;
import javafx.scene.layout.HBox;
import javafx.scene.layout.Priority;
import javafx.scene.layout.VBox;
import javafx.scene.paint.Color;

import java.util.List;
import java.util.function.Consumer;

@Dependent
public class InspectorPanelController {
    @Inject ProjectService projects;
    @Inject EditorState state;

    @FXML private ScrollPane scroll;
    @FXML private VBox content;

    @FXML
    public void initialize() {
        projects.activeDocument().addListener((observable, oldValue, newValue) -> {
            if (newValue != null) state.selectLevelDefaults(newValue.level);
            rebuild();
        });
        projects.project().addListener((observable, oldValue, newValue) -> rebuild());
        projects.documents().addListener((javafx.collections.ListChangeListener<Document>) change -> rebuild());
        state.selectedEntityId().addListener((observable, oldValue, newValue) -> rebuild());
        state.activeTileLayer().addListener((observable, oldValue, newValue) -> rebuild());
        state.activeObjectLayer().addListener((observable, oldValue, newValue) -> rebuild());
        state.objectLayerSelected().addListener((observable, oldValue, newValue) -> rebuild());
        rebuild();
    }

    private void rebuild() {
        content.getChildren().clear();
        final Level level = projects.activeLevel();
        if (level == null) {
            content.getChildren().add(label("Kein Level geöffnet.", "dim"));
            return;
        }
        addLevelSection(level);
        final Entity entity = state.selectedEntity(level);
        if (entity != null) {
            addEntitySection(level, entity);
        } else if (state.objectLayerSelected().get()) {
            addObjectLayerSection(level);
        } else {
            addTileLayerSection(level);
        }
    }

    private void addLevelSection(Level level) {
        content.getChildren().add(section("Level"));
        content.getChildren().add(textRow("Name", level.name, value -> {
            level.name = value;
            commit();
        }));
        final HBox size = new HBox(6);
        size.getChildren().add(textRow("Breite", Integer.toString(level.width), value -> {
            resize(level, parseInt(value, level.width), level.height);
            commit();
            rebuild();
        }, 90));
        size.getChildren().add(textRow("Höhe", Integer.toString(level.height), value -> {
            resize(level, level.width, parseInt(value, level.height));
            commit();
            rebuild();
        }, 90));
        content.getChildren().add(size);
        final HBox tile = new HBox(6);
        tile.getChildren().add(textRow("Kachel-B", Integer.toString(level.tileW), value -> {
            level.tileW = Math.clamp(parseInt(value, level.tileW), 1, 512);
            commit();
        }, 90));
        tile.getChildren().add(textRow("Kachel-H", Integer.toString(level.tileH), value -> {
            level.tileH = Math.clamp(parseInt(value, level.tileH), 1, 512);
            commit();
        }, 90));
        content.getChildren().add(tile);
        content.getChildren().add(label(level.tileLayers.size() + " Kachel- und " + level.objectLayers.size()
                + " Objektebenen", "dim"));
    }

    private void addTileLayerSection(Level level) {
        if (level.tileLayers.isEmpty()) {
            content.getChildren().add(section("Kachelebene"));
            content.getChildren().add(label("Keine Kachelebene.", "dim"));
            return;
        }
        final int index = Math.clamp(state.activeTileLayer().get(), 0, level.tileLayers.size() - 1);
        final TileLayer layer = level.tileLayers.get(index);
        content.getChildren().add(section("Kachelebene"));
        content.getChildren().add(textRow("Name", layer.name, value -> {
            layer.name = value;
            commit();
        }));
        content.getChildren().add(checkRow("sichtbar", layer.visible, value -> {
            layer.visible = value;
            commit();
        }));
        content.getChildren().add(checkRow("gesperrt", layer.locked, value -> {
            layer.locked = value;
            commit();
        }));
        final Slider opacity = new Slider(0, 1, layer.opacity);
        opacity.setShowTickMarks(false);
        opacity.valueProperty().addListener((observable, oldValue, newValue) -> layer.opacity = newValue.doubleValue());
        opacity.setOnMouseReleased(event -> commit());
        content.getChildren().add(new Label("Deckkraft"));
        content.getChildren().add(opacity);
        content.getChildren().add(label("GID " + state.selectedTile().get(), "dim"));
    }

    private void addObjectLayerSection(Level level) {
        if (level.objectLayers.isEmpty()) {
            content.getChildren().add(section("Objektebene"));
            content.getChildren().add(label("Keine Objektebene.", "dim"));
            return;
        }
        final int index = Math.clamp(state.activeObjectLayer().get(), 0, level.objectLayers.size() - 1);
        final ObjectLayer layer = level.objectLayers.get(index);
        content.getChildren().add(section("Objektebene"));
        content.getChildren().add(textRow("Name", layer.name, value -> {
            layer.name = value;
            commit();
        }));
        content.getChildren().add(checkRow("sichtbar", layer.visible, value -> {
            layer.visible = value;
            commit();
        }));
        content.getChildren().add(checkRow("gesperrt", layer.locked, value -> {
            layer.locked = value;
            commit();
        }));
        content.getChildren().add(label(layer.entities.size() + " Entities", "dim"));
    }

    private void addEntitySection(Level level, Entity entity) {
        content.getChildren().add(section("Entity #" + entity.id + " · " + entity.def));
        if (!entity.templatePath.isEmpty()) content.getChildren().add(label("Template: " + entity.templatePath, "success"));
        final HBox position = new HBox(6);
        position.getChildren().add(textRow("X", format(entity.x), value -> {
            entity.x = parseDouble(value, entity.x);
            commit();
        }, 90));
        position.getChildren().add(textRow("Y", format(entity.y), value -> {
            entity.y = parseDouble(value, entity.y);
            commit();
        }, 90));
        content.getChildren().add(position);
        final HBox size = new HBox(6);
        size.getChildren().add(textRow("Breite", format(entity.w), value -> {
            entity.w = Math.max(1, parseDouble(value, entity.w));
            commit();
        }, 90));
        size.getChildren().add(textRow("Höhe", format(entity.h), value -> {
            entity.h = Math.max(1, parseDouble(value, entity.h));
            commit();
        }, 90));
        content.getChildren().add(size);
        content.getChildren().add(textRow("Rotation", format(entity.rotation), value -> {
            entity.rotation = parseDouble(value, entity.rotation);
            commit();
        }));

        final EntityDef def = projects.currentProject() == null ? null : projects.currentProject().findEntityDef(entity.def);
        if (def == null) {
            content.getChildren().add(label("Keine Definition für '" + entity.def + "'.", "dim"));
            return;
        }
        content.getChildren().add(section("Attribute"));
        for (final AttributeDef attribute : def.attributes) {
            entity.attributes.putIfAbsent(attribute.name, attribute.defaultValue);
            final String value = entity.attributes.getOrDefault(attribute.name, "");
            final Consumer<String> update = updated -> {
                entity.attributes.put(attribute.name, updated);
                commit();
            };
            if ("bool".equals(attribute.type)) {
                content.getChildren().add(checkRow(attribute.name, "true".equals(value), updated ->
                        update.accept(updated ? "true" : "false")));
            } else if ("enum".equals(attribute.type) && !attribute.values.isEmpty()) {
                final ComboBox<String> combo = new ComboBox<>();
                combo.getItems().addAll(attribute.values);
                combo.setValue(attribute.values.contains(value) ? value : attribute.values.getFirst());
                combo.setMaxWidth(Double.MAX_VALUE);
                combo.valueProperty().addListener((observable, oldValue, newValue) -> {
                    if (newValue != null && !newValue.equals(value)) update.accept(newValue);
                });
                content.getChildren().add(new Label(attribute.name));
                content.getChildren().add(combo);
            } else if ("color".equals(attribute.type)) {
                final ColorPicker picker = new ColorPicker(parseColor(value));
                picker.setMaxWidth(Double.MAX_VALUE);
                picker.setOnAction(event -> update.accept(toHex(picker.getValue())));
                content.getChildren().add(new Label(attribute.name));
                content.getChildren().add(picker);
            } else {
                content.getChildren().add(textRow(attribute.name, value, update));
            }
        }
    }

    private Node section(String text) {
        final Label label = new Label(text);
        label.getStyleClass().add("section");
        VBox.setMargin(label, new Insets(8, 0, 0, 0));
        return label;
    }

    private Label label(String text, String styleClass) {
        final Label label = new Label(text);
        label.setWrapText(true);
        label.getStyleClass().add(styleClass);
        return label;
    }

    private Node textRow(String name, String value, Consumer<String> commit) {
        return textRow(name, value, commit, 0);
    }

    private Node textRow(String name, String value, Consumer<String> commit, double width) {
        final Label label = new Label(name);
        label.setMinWidth(64);
        final TextField field = new TextField(value);
        field.setMaxWidth(Double.MAX_VALUE);
        HBox.setHgrow(field, Priority.ALWAYS);
        if (width > 0) {
            field.setPrefWidth(width);
            HBox.setHgrow(field, Priority.NEVER);
        }
        field.setOnAction(event -> commit.accept(field.getText()));
        field.focusedProperty().addListener((observable, oldValue, newValue) -> {
            if (!newValue) commit.accept(field.getText());
        });
        final HBox box = new HBox(6, label, field);
        box.setFillHeight(true);
        return box;
    }

    private Node checkRow(String name, boolean value, Consumer<Boolean> commit) {
        final CheckBox box = new CheckBox(name);
        box.setSelected(value);
        box.setOnAction(event -> commit.accept(box.isSelected()));
        return box;
    }

    private void commit() {
        projects.markDirty();
        projects.commit();
    }

    private static void resize(Level level, int width, int height) {
        width = Math.clamp(width, 1, 4096);
        height = Math.clamp(height, 1, 4096);
        if (width == level.width && height == level.height) return;
        for (final TileLayer layer : level.tileLayers) {
            final int[] tiles = new int[width * height];
            final int copyW = Math.min(width, level.width);
            final int copyH = Math.min(height, level.height);
            for (int y = 0; y < copyH; y++) {
                System.arraycopy(layer.tiles, y * level.width, tiles, y * width, copyW);
            }
            layer.tiles = tiles;
        }
        level.width = width;
        level.height = height;
    }

    private static int parseInt(String value, int fallback) {
        try {
            return Integer.parseInt(value.trim());
        } catch (NumberFormatException exception) {
            return fallback;
        }
    }

    private static double parseDouble(String value, double fallback) {
        try {
            return Double.parseDouble(value.trim().replace(',', '.'));
        } catch (NumberFormatException exception) {
            return fallback;
        }
    }

    private static String format(double value) {
        return Math.abs(value - Math.rint(value)) < 0.0001 ? Long.toString(Math.round(value)) : Double.toString(value);
    }

    private static Color parseColor(String value) {
        try {
            return Color.web(value);
        } catch (IllegalArgumentException exception) {
            return Color.web("#78a0c8");
        }
    }

    private static String toHex(Color color) {
        return String.format("#%02x%02x%02x", Math.round(color.getRed() * 255),
                Math.round(color.getGreen() * 255), Math.round(color.getBlue() * 255));
    }
}
