package de.sgl.editor.service;

import de.sgl.editor.model.Entity;
import de.sgl.editor.model.EntityDef;
import de.sgl.editor.model.Level;
import de.sgl.editor.model.ObjectLayer;
import de.sgl.editor.model.TileLayer;

import jakarta.enterprise.context.ApplicationScoped;

import javafx.beans.property.*;

@ApplicationScoped
public class EditorState {
    public enum Tool { SELECT, BRUSH, RECT, LINE, FILL, ERASER, PICKER, ENTITY, PAN }

    private final ObjectProperty<Tool> tool = new SimpleObjectProperty<>(Tool.BRUSH);
    private final IntegerProperty selectedTile = new SimpleIntegerProperty(0);
    private final IntegerProperty activeTileLayer = new SimpleIntegerProperty(0);
    private final IntegerProperty activeObjectLayer = new SimpleIntegerProperty(0);
    private final BooleanProperty objectLayerSelected = new SimpleBooleanProperty(false);
    private final IntegerProperty selectedEntityId = new SimpleIntegerProperty(-1);
    private final ObjectProperty<EntityDef> selectedEntityDef = new SimpleObjectProperty<>();
    private final BooleanProperty showGrid = new SimpleBooleanProperty(true);
    private final BooleanProperty showMinimap = new SimpleBooleanProperty(true);
    private final DoubleProperty zoom = new SimpleDoubleProperty(1.0);
    private final DoubleProperty cameraX = new SimpleDoubleProperty(0.0);
    private final DoubleProperty cameraY = new SimpleDoubleProperty(0.0);
    private final StringProperty status = new SimpleStringProperty("Bereit");
    private final StringProperty toolText = new SimpleStringProperty("Stift");
    private final DoubleProperty fps = new SimpleDoubleProperty(60.0);

    public ObjectProperty<Tool> tool() { return tool; }
    public IntegerProperty selectedTile() { return selectedTile; }
    public IntegerProperty activeTileLayer() { return activeTileLayer; }
    public IntegerProperty activeObjectLayer() { return activeObjectLayer; }
    public BooleanProperty objectLayerSelected() { return objectLayerSelected; }
    public IntegerProperty selectedEntityId() { return selectedEntityId; }
    public ObjectProperty<EntityDef> selectedEntityDef() { return selectedEntityDef; }
    public BooleanProperty showGrid() { return showGrid; }
    public BooleanProperty showMinimap() { return showMinimap; }
    public DoubleProperty zoom() { return zoom; }
    public DoubleProperty cameraX() { return cameraX; }
    public DoubleProperty cameraY() { return cameraY; }
    public StringProperty status() { return status; }
    public StringProperty toolText() { return toolText; }
    public DoubleProperty fps() { return fps; }

    public void setTool(Tool value) {
        tool.set(value);
        toolText.set(switch (value) {
            case SELECT -> "Auswahl";
            case BRUSH -> "Stift";
            case RECT -> "Rechteck";
            case LINE -> "Linie";
            case FILL -> "Füllen";
            case ERASER -> "Radierer";
            case PICKER -> "Pipette";
            case ENTITY -> "Entity";
            case PAN -> "Pan";
        });
    }

    public TileLayer activeTileLayer(Level level) {
        if (level == null || level.tileLayers.isEmpty()) return null;
        final int index = Math.clamp(activeTileLayer.get(), 0, level.tileLayers.size() - 1);
        activeTileLayer.set(index);
        return level.tileLayers.get(index);
    }

    public ObjectLayer activeObjectLayer(Level level) {
        if (level == null || level.objectLayers.isEmpty()) return null;
        final int index = Math.clamp(activeObjectLayer.get(), 0, level.objectLayers.size() - 1);
        activeObjectLayer.set(index);
        return level.objectLayers.get(index);
    }

    public Entity selectedEntity(Level level) {
        if (level == null || selectedEntityId.get() < 0) return null;
        return level.findEntity(selectedEntityId.get());
    }

    public void selectLevelDefaults(Level level) {
        if (level == null) return;
        activeTileLayer.set(level.tileLayers.isEmpty() ? 0 : Math.min(activeTileLayer.get(), level.tileLayers.size() - 1));
        activeObjectLayer.set(level.objectLayers.isEmpty() ? 0 : Math.min(activeObjectLayer.get(), level.objectLayers.size() - 1));
        selectedEntityId.set(-1);
    }
}
