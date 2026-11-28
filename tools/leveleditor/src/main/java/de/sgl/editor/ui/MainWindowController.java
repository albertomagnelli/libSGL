package de.sgl.editor.ui;

import de.sgl.editor.model.Document;
import de.sgl.editor.model.Entity;
import de.sgl.editor.model.Level;
import de.sgl.editor.model.ObjectLayer;
import de.sgl.editor.model.TileLayer;
import de.sgl.editor.service.AutosaveService;
import de.sgl.editor.service.EditorState;
import de.sgl.editor.service.ProjectService;
import de.sgl.editor.service.StageService;
import de.sgl.editor.ui.dialogs.FileDialogs;
import de.sgl.editor.ui.docking.DockManager;
import de.sgl.editor.ui.docking.PanelId;
import de.sgl.editor.ui.playtest.PlaytestPane;
import de.sgl.editor.ui.viewport.ViewportPane;

import jakarta.enterprise.context.Dependent;
import jakarta.inject.Inject;

import javafx.application.Platform;
import javafx.beans.binding.Bindings;
import javafx.collections.ListChangeListener;
import javafx.fxml.FXML;
import javafx.scene.Scene;
import javafx.scene.control.*;
import javafx.scene.input.KeyCode;
import javafx.scene.input.KeyEvent;
import javafx.scene.layout.BorderPane;
import javafx.scene.layout.Priority;
import javafx.scene.layout.StackPane;
import javafx.scene.layout.VBox;
import javafx.stage.Stage;

import java.util.List;

@Dependent
public class MainWindowController {
    @Inject ProjectService projects;
    @Inject EditorState state;
    @Inject StageService stages;
    @Inject FileDialogs dialogs;
    @Inject DockManager docks;
    @Inject ViewportPane viewport;
    @Inject PlaytestPane playtest;
    @Inject AutosaveService autosave;

    @FXML private StackPane shell;
    @FXML private BorderPane root;
    @FXML private StackPane dockHost;
    @FXML private Label statusLeft;
    @FXML private Label statusRight;
    @FXML private CheckMenuItem gridItem;
    @FXML private CheckMenuItem minimapItem;
    @FXML private ToolBar toolBar;

    private final TabPane levelTabs = new TabPane();
    private boolean updatingTabs;
    private Scene installedScene;
    private final ToggleGroup tools = new ToggleGroup();

    @FXML
    public void initialize() {
        levelTabs.setTabClosingPolicy(TabPane.TabClosingPolicy.ALL_TABS);
        levelTabs.setPrefHeight(30);
        levelTabs.setMinHeight(30);
        final VBox center = new VBox(levelTabs, viewport);
        VBox.setVgrow(viewport, Priority.ALWAYS);
        docks.initialize(dockHost, center);
        playtest.setVisible(false);
        playtest.setOnExit(() -> {
            playtest.setVisible(false);
            viewport.requestFocus();
        });
        shell.getChildren().add(playtest);
        buildToolbar();
        gridItem.setSelected(state.showGrid().get());
        minimapItem.setSelected(state.showMinimap().get());
        state.showGrid().addListener((observable, oldValue, newValue) -> gridItem.setSelected(newValue));
        state.showMinimap().addListener((observable, oldValue, newValue) -> minimapItem.setSelected(newValue));
        projects.documents().addListener((ListChangeListener<Document>) change -> refreshTabs());
        projects.activeDocument().addListener((observable, oldValue, newValue) -> {
            refreshTabs();
            if (newValue != null) state.selectLevelDefaults(newValue.level);
        });
        projects.dirty().addListener((observable, oldValue, newValue) -> refreshTabs());
        statusLeft.textProperty().bind(state.status());
        statusRight.textProperty().bind(Bindings.createStringBinding(() ->
                        state.toolText().get() + "  ·  Zoom " + Math.round(state.zoom().get() * 100) + " %  ·  "
                                + (projects.dirty().get() ? "  ·  ungespeichert" : ""),
                state.toolText(), state.zoom(), projects.dirty()));
        levelTabs.getSelectionModel().selectedItemProperty().addListener((observable, oldValue, newValue) -> {
            if (updatingTabs || newValue == null) return;
            final Object value = newValue.getUserData();
            if (value instanceof Document document) projects.activeDocument().set(document);
        });
        refreshTabs();
        autosave.start();
        Platform.runLater(this::installShortcuts);
    }

    private void buildToolbar() {
        toolBar.getItems().clear();
        addTool("Auswahl", EditorState.Tool.SELECT, "V");
        addTool("Stift", EditorState.Tool.BRUSH, "B");
        addTool("Rechteck", EditorState.Tool.RECT, "R");
        addTool("Linie", EditorState.Tool.LINE, "L");
        addTool("Füllen", EditorState.Tool.FILL, "F");
        addTool("Radierer", EditorState.Tool.ERASER, "E");
        addTool("Pipette", EditorState.Tool.PICKER, "I");
        addTool("Entity", EditorState.Tool.ENTITY, "N");
        addTool("Pan", EditorState.Tool.PAN, "Leertaste");
        toolBar.getItems().add(new Separator());
        toolBar.getItems().add(button("Rückgängig", this::onUndo));
        toolBar.getItems().add(button("Wiederholen", this::onRedo));
        toolBar.getItems().add(new Separator());
        toolBar.getItems().add(button("Speichern", this::onSave));
        toolBar.getItems().add(button("Öffnen", this::onOpenProject));
        toolBar.getItems().add(new Separator());
        toolBar.getItems().add(button("Vorschau (F5)", this::onPreview));
        tools.selectedToggleProperty().addListener((observable, oldValue, newValue) -> {
            if (newValue != null && newValue.getUserData() instanceof EditorState.Tool tool) {
                state.setTool(tool);
            }
        });
    }

    private void addTool(String text, EditorState.Tool tool, String shortcut) {
        final ToggleButton button = new ToggleButton(text);
        button.setToggleGroup(tools);
        button.setUserData(tool);
        button.setTooltip(new Tooltip(text + " (" + shortcut + ")"));
        button.setSelected(state.tool().get() == tool);
        state.tool().addListener((observable, oldValue, newValue) -> button.setSelected(newValue == tool));
        toolBar.getItems().add(button);
    }

    private Button button(String text, Runnable action) {
        final Button button = new Button(text);
        button.setOnAction(event -> action.run());
        return button;
    }

    private void refreshTabs() {
        if (updatingTabs) return;
        updatingTabs = true;
        levelTabs.getTabs().clear();
        for (final Document document : projects.documents()) {
            final Tab tab = new Tab(document.title + (document.dirty ? " *" : ""));
            tab.setUserData(document);
            tab.setOnCloseRequest(event -> {
                if (document.dirty && !confirmClose(document)) event.consume();
            });
            tab.setOnClosed(event -> projects.closeDocument(document));
            levelTabs.getTabs().add(tab);
        }
        final Document active = projects.activeDocument().get();
        if (active != null) {
            for (final Tab tab : levelTabs.getTabs()) {
                if (tab.getUserData() == active) {
                    levelTabs.getSelectionModel().select(tab);
                    break;
                }
            }
        }
        updatingTabs = false;
    }

    private boolean confirmClose(Document document) {
        final Alert alert = new Alert(Alert.AlertType.CONFIRMATION);
        alert.initOwner(stages.main());
        alert.setTitle("Ungespeicherte Änderungen");
        alert.setHeaderText(document.title + " wurde geändert.");
        alert.setContentText("Trotzdem schließen?");
        return alert.showAndWait().orElse(ButtonType.CANCEL) == ButtonType.OK;
    }

    private void installShortcuts() {
        final Stage stage = stages.main();
        if (stage == null || stage.getScene() == installedScene) return;
        installedScene = stage.getScene();
        installedScene.addEventFilter(KeyEvent.KEY_RELEASED, event -> {
            if (playtest.isRunning()) playtest.handleKeyReleased(event);
        });
        installedScene.addEventFilter(KeyEvent.KEY_PRESSED, event -> {
            if (playtest.isRunning()) {
                if (event.getCode() == KeyCode.ESCAPE || event.getCode() == KeyCode.F5) {
                    playtest.stop();
                    event.consume();
                } else {
                    playtest.handleKeyPressed(event);
                }
                return;
            }
            if (event.getTarget() instanceof TextInputControl) {
                if (event.getCode() == KeyCode.S && event.isControlDown()) {
                    onSave();
                    event.consume();
                }
                return;
            }
            final boolean control = event.isControlDown();
            if (control && event.getCode() == KeyCode.N) { onNewProject(); event.consume(); }
            else if (control && event.getCode() == KeyCode.O) { onOpenProject(); event.consume(); }
            else if (control && event.getCode() == KeyCode.S) {
                if (event.isShiftDown()) onSaveAs(); else onSave();
                event.consume();
            } else if (control && event.getCode() == KeyCode.Z) {
                if (event.isShiftDown()) onRedo(); else onUndo();
                event.consume();
            } else if (control && event.getCode() == KeyCode.Y) { onRedo(); event.consume(); }
            else if (control && event.getCode() == KeyCode.W) {
                final Document document = projects.activeDocument().get();
                if (document != null) projects.closeDocument(document);
                event.consume();
            } else if (event.getCode() == KeyCode.F5) { onPreview(); event.consume(); }
            else if (event.getCode() == KeyCode.DELETE) { deleteSelectedEntity(); event.consume(); }
            else if (event.getCode() == KeyCode.G) { onToggleGrid(); event.consume(); }
            else if (event.getCode() == KeyCode.M) { onToggleMinimap(); event.consume(); }
            else if (event.getCode() == KeyCode.V) { state.setTool(EditorState.Tool.SELECT); }
            else if (event.getCode() == KeyCode.B) { state.setTool(EditorState.Tool.BRUSH); }
            else if (event.getCode() == KeyCode.R) { state.setTool(EditorState.Tool.RECT); }
            else if (event.getCode() == KeyCode.L) { state.setTool(EditorState.Tool.LINE); }
            else if (event.getCode() == KeyCode.F) { state.setTool(EditorState.Tool.FILL); }
            else if (event.getCode() == KeyCode.E) { state.setTool(EditorState.Tool.ERASER); }
            else if (event.getCode() == KeyCode.I) { state.setTool(EditorState.Tool.PICKER); }
            else if (event.getCode() == KeyCode.N) { state.setTool(EditorState.Tool.ENTITY); }
            else if (event.getCode() == KeyCode.EQUALS || event.getCode() == KeyCode.ADD) {
                state.zoom().set(Math.min(state.zoom().get() * 1.2, 8));
            } else if (event.getCode() == KeyCode.MINUS || event.getCode() == KeyCode.SUBTRACT) {
                state.zoom().set(Math.max(state.zoom().get() / 1.2, 0.12));
            }
        });
    }

    private void deleteSelectedEntity() {
        final Level level = projects.activeLevel();
        final int id = state.selectedEntityId().get();
        if (level == null || id < 0) return;
        for (final ObjectLayer layer : level.objectLayers) {
            if (layer.entities.removeIf(entity -> entity.id == id)) {
                state.selectedEntityId().set(-1);
                projects.markDirty();
                projects.commit();
                return;
            }
        }
    }

    @FXML private void onNewProject() { dialogs.newProject(); }
    @FXML private void onOpenProject() { dialogs.openProject(); }
    @FXML private void onSave() { projects.saveProject(); refreshTabs(); }
    @FXML private void onSaveAs() { dialogs.saveProjectAs(); }
    @FXML private void onImportLevel() { dialogs.importLevel(); }
    @FXML private void onExportLevel() { dialogs.exportLevel(); }
    @FXML private void onImportTileset() { dialogs.importTileset(); }
    @FXML private void onUndo() { projects.undo(); refreshTabs(); viewport.requestRedraw(); }
    @FXML private void onRedo() { projects.redo(); refreshTabs(); viewport.requestRedraw(); }

    @FXML private void onToggleGrid() {
        state.showGrid().set(!state.showGrid().get());
        gridItem.setSelected(state.showGrid().get());
    }

    @FXML private void onToggleMinimap() {
        state.showMinimap().set(!state.showMinimap().get());
        minimapItem.setSelected(state.showMinimap().get());
    }

    @FXML private void onPreview() {
        if (playtest.isRunning()) playtest.stop();
        else playtest.start();
    }

    public void startPreview() {
        if (!playtest.isRunning()) playtest.start();
    }

    @FXML private void onResetLayout() { docks.resetLayout(); }

    @FXML private void onFitView() {
        final Level level = projects.activeLevel();
        if (level == null) return;
        final double zoom = Math.min(viewport.getWidth() / (level.width * (double) level.tileW),
                viewport.getHeight() / (level.height * (double) level.tileH)) * 0.92;
        state.zoom().set(Math.clamp(zoom, 0.12, 8));
        state.cameraX().set(level.width * level.tileW * 0.5);
        state.cameraY().set(level.height * level.tileH * 0.5);
    }

    @FXML private void onAddTileLayer() {
        final Level level = projects.activeLevel();
        if (level == null) return;
        final TileLayer layer = new TileLayer();
        layer.name = "Kacheln " + (level.tileLayers.size() + 1);
        layer.tiles = new int[level.width * level.height];
        level.tileLayers.add(layer);
        state.objectLayerSelected().set(false);
        state.activeTileLayer().set(level.tileLayers.size() - 1);
        projects.markDirty(); projects.commit();
    }

    @FXML private void onAddObjectLayer() {
        final Level level = projects.activeLevel();
        if (level == null) return;
        final ObjectLayer layer = new ObjectLayer();
        layer.name = "Objekte " + (level.objectLayers.size() + 1);
        level.objectLayers.add(layer);
        state.objectLayerSelected().set(true);
        state.activeObjectLayer().set(level.objectLayers.size() - 1);
        projects.markDirty(); projects.commit();
    }

    @FXML private void onDuplicateLayer() {
        // Die Ebenen-Panel-Schaltfläche ist der primäre Weg; hier ein kurzer Weg.
        final Level level = projects.activeLevel();
        if (level == null) return;
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
        projects.markDirty(); projects.commit();
    }

    @FXML private void onDeleteLayer() {
        final Level level = projects.activeLevel();
        if (level == null) return;
        if (state.objectLayerSelected().get() && !level.objectLayers.isEmpty()) {
            level.objectLayers.remove(Math.clamp(state.activeObjectLayer().get(), 0, level.objectLayers.size() - 1));
        } else if (!level.tileLayers.isEmpty()) {
            level.tileLayers.remove(Math.clamp(state.activeTileLayer().get(), 0, level.tileLayers.size() - 1));
        }
        projects.markDirty(); projects.commit();
    }

    @FXML private void onShortcuts() {
        final Alert alert = new Alert(Alert.AlertType.INFORMATION);
        alert.initOwner(stages.main());
        alert.setTitle("Tastenkürzel");
        alert.setHeaderText("sgl Leveleditor");
        alert.setContentText("""
                Strg+N/O/S: Neu/Öffnen/Speichern
                Strg+Z/Y: Rückgängig/Wiederholen
                V B R L F E I N: Werkzeuge
                G/M: Raster/Minimap
                F5: Vorschau, Esc beendet sie
                Entf: Entity löschen
                Tab-Ziehen: Panel andocken oder lösen""");
        alert.showAndWait();
    }

    @FXML private void onAbout() {
        final Alert alert = new Alert(Alert.AlertType.INFORMATION);
        alert.initOwner(stages.main());
        alert.setTitle("Über den Editor");
        alert.setHeaderText("sgl Leveleditor 0.1");
        alert.setContentText("Java 25 · JavaFX 25 · FXML · CDI (Weld SE)");
        alert.showAndWait();
    }

    @FXML private void onExit() {
        if (stages.main() != null) stages.main().close();
    }
}
