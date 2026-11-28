package de.sgl.editor.ui.docking;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.node.ArrayNode;
import com.fasterxml.jackson.databind.node.ObjectNode;

import de.sgl.editor.service.ProjectService;
import de.sgl.editor.service.StageService;

import jakarta.annotation.PreDestroy;
import jakarta.enterprise.context.ApplicationScoped;
import jakarta.inject.Inject;

import javafx.application.Platform;
import javafx.geometry.Orientation;
import javafx.geometry.Point2D;
import javafx.scene.Node;
import javafx.scene.Scene;
import javafx.scene.control.Button;
import javafx.scene.control.Label;
import javafx.scene.control.SplitPane;
import javafx.scene.control.Tab;
import javafx.scene.control.TabPane;
import javafx.scene.input.ClipboardContent;
import javafx.scene.input.DataFormat;
import javafx.scene.input.Dragboard;
import javafx.scene.input.TransferMode;
import javafx.scene.layout.BorderPane;
import javafx.scene.layout.HBox;
import javafx.scene.layout.Priority;
import javafx.scene.layout.Region;
import javafx.scene.layout.StackPane;
import javafx.stage.Screen;
import javafx.stage.Stage;

import java.awt.MouseInfo;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.EnumMap;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

@ApplicationScoped
public class DockManager {
    private static final DataFormat PANEL_FORMAT = new DataFormat("application/x-sgl-editor-panel");

    private final TabPane left = new TabPane();
    private final TabPane right = new TabPane();
    private final TabPane bottom = new TabPane();
    private final SplitPane horizontal = new SplitPane();
    private final SplitPane vertical = new SplitPane();
    private final Map<PanelId, Tab> tabs = new EnumMap<>(PanelId.class);
    private final Map<PanelId, Stage> floating = new EnumMap<>(PanelId.class);
    private final Map<PanelId, double[]> pendingFloating = new EnumMap<>(PanelId.class);
    private final ObjectMapper mapper = new ObjectMapper();
    private boolean initialized;
    private boolean shuttingDown;
    private double[] horizontalDividers = {0.20, 0.80};
    private double[] verticalDividers = {0.76};

    @Inject PanelFactory factory;
    @Inject StageService stages;
    @Inject ProjectService projects;

    public void initialize(StackPane host, Node center) {
        if (initialized) return;
        initialized = true;
        factory.loadAll();
        for (final TabPane pane : List.of(left, right, bottom)) {
            pane.setTabClosingPolicy(TabPane.TabClosingPolicy.UNAVAILABLE);
            pane.getStyleClass().add("dock-tab-pane");
        }
        left.setMinWidth(180);
        right.setMinWidth(200);
        bottom.setMinHeight(100);
        horizontal.setOrientation(Orientation.HORIZONTAL);
        horizontal.getItems().addAll(left, center, right);
        horizontal.setDividerPositions(horizontalDividers);
        vertical.setOrientation(Orientation.VERTICAL);
        vertical.getItems().addAll(horizontal, bottom);
        vertical.setDividerPositions(verticalDividers);
        host.getChildren().add(vertical);
        configureDock(left, 0);
        configureDock(right, 1);
        configureDock(bottom, 2);
        projects.project().addListener((observable, oldValue, newValue) -> {
            if (newValue == null) {
                resetLayout();
            } else {
                applyLayoutForProject();
            }
        });
        applyLayoutForProject();
    }

    public void activate(PanelId id) {
        final Tab tab = tabs.get(id);
        if (tab != null && tab.getTabPane() != null) {
            tab.getTabPane().getSelectionModel().select(tab);
        }
    }

    public void resetLayout() {
        dockAllFloating();
        clearTabs();
        addDefault(PanelId.PROJECT);
        addDefault(PanelId.PALETTE);
        addDefault(PanelId.ENTITIES);
        addDefault(PanelId.INSPECTOR);
        addDefault(PanelId.LAYERS);
        addDefault(PanelId.CONSOLE);
        addDefault(PanelId.ASSETS);
        selectFirst(left);
        selectFirst(right);
        selectFirst(bottom);
        horizontal.setDividerPositions(0.20, 0.80);
        vertical.setDividerPositions(0.76);
        saveLayout();
    }

    public void saveLayout() {
        final var project = projects.currentProject();
        if (project == null || project.root == null) return;
        try {
            final ObjectNode root = mapper.createObjectNode();
            root.put("version", 2);
            writeDock(root, "left", left);
            writeDock(root, "right", right);
            writeDock(root, "bottom", bottom);
            final ArrayNode dividers = root.putArray("horizontalDividers");
            for (final double divider : horizontal.getDividerPositions()) dividers.add(divider);
            final ArrayNode vDividers = root.putArray("verticalDividers");
            for (final double divider : vertical.getDividerPositions()) vDividers.add(divider);
            final ArrayNode floats = root.putArray("floating");
            for (final Map.Entry<PanelId, Stage> entry : floating.entrySet()) {
                final Stage stage = entry.getValue();
                if (stage == null || !stage.isShowing()) continue;
                final ObjectNode node = floats.addObject();
                node.put("panel", entry.getKey().key());
                node.put("x", stage.getX());
                node.put("y", stage.getY());
                node.put("w", stage.getWidth());
                node.put("h", stage.getHeight());
            }
            final Path file = project.root.resolve(".sgl/layout.json");
            Files.createDirectories(file.getParent());
            mapper.writerWithDefaultPrettyPrinter().writeValue(file.toFile(), root);
        } catch (IOException ignored) {
        }
    }

    private void writeDock(ObjectNode root, String key, TabPane pane) {
        final ArrayNode ids = root.putArray(key);
        for (final Tab tab : pane.getTabs()) {
            if (tab.getUserData() instanceof PanelId id) ids.add(id.key());
        }
        root.put(key + "Active", Math.max(pane.getSelectionModel().getSelectedIndex(), 0));
    }

    private void applyLayoutForProject() {
        final var project = projects.currentProject();
        if (project == null || project.root == null) {
            resetLayout();
            return;
        }
        final Path file = project.root.resolve(".sgl/layout.json");
        if (!Files.isRegularFile(file)) {
            resetLayout();
            return;
        }
        try {
            final JsonNode root = mapper.readTree(file.toFile());
            if (root.path("version").asInt(0) < 2) {
                resetLayout();
                return;
            }
            dockAllFloating();
            clearTabs();
            readDock(root, "left", left);
            readDock(root, "right", right);
            readDock(root, "bottom", bottom);
            final JsonNode floats = root.get("floating");
            if (floats != null && floats.isArray()) {
                for (final JsonNode node : floats) {
                    final PanelId id = PanelId.fromKey(node.path("panel").asText());
                    if (id == null || tabs.containsKey(id)) continue;
                    pendingFloating.put(id, new double[]{node.path("x").asDouble(100), node.path("y").asDouble(100),
                            node.path("w").asDouble(380), node.path("h").asDouble(560)});
                }
            }
            addMissingDefaults();
            final JsonNode h = root.get("horizontalDividers");
            if (h != null && h.isArray()) {
                horizontalDividers = new double[h.size()];
                for (int i = 0; i < h.size(); i++) horizontalDividers[i] = h.get(i).asDouble();
                horizontal.setDividerPositions(horizontalDividers);
            }
            final JsonNode v = root.get("verticalDividers");
            if (v != null && v.isArray()) {
                verticalDividers = new double[v.size()];
                for (int i = 0; i < v.size(); i++) verticalDividers[i] = v.get(i).asDouble();
                vertical.setDividerPositions(verticalDividers);
            }
            Platform.runLater(this::restoreFloating);
        } catch (IOException exception) {
            resetLayout();
        }
    }

    private void readDock(JsonNode root, String key, TabPane pane) {
        final JsonNode ids = root.get(key);
        if (ids == null || !ids.isArray()) return;
        for (final JsonNode node : ids) {
            final PanelId id = PanelId.fromKey(node.asText());
            if (id != null && !tabs.containsKey(id)) addTab(id, pane, pane.getTabs().size(), false);
        }
        final int active = root.path(key + "Active").asInt(0);
        if (!pane.getTabs().isEmpty()) pane.getSelectionModel().select(Math.clamp(active, 0, pane.getTabs().size() - 1));
    }

    private void addMissingDefaults() {
        for (final PanelId id : PanelId.values()) {
            if (tabs.containsKey(id) || pendingFloating.containsKey(id)) continue;
            addTab(id, switch (id) {
                case INSPECTOR -> right;
                case LAYERS, CONSOLE, ASSETS -> bottom;
                default -> left;
            }, Integer.MAX_VALUE, false);
        }
    }

    private void selectFirst(TabPane pane) {
        if (!pane.getTabs().isEmpty()) pane.getSelectionModel().select(0);
    }

    private void addDefault(PanelId id) {
        addTab(id, switch (id) {
            case INSPECTOR -> right;
            case LAYERS, CONSOLE, ASSETS -> bottom;
            default -> left;
        }, Integer.MAX_VALUE, true);
    }

    private void clearTabs() {
        for (final TabPane pane : List.of(left, right, bottom)) {
            for (final Tab tab : List.copyOf(pane.getTabs())) {
                tab.setContent(null);
            }
            pane.getTabs().clear();
        }
        tabs.clear();
    }

    private void addTab(PanelId id, TabPane pane, int index, boolean active) {
        final PanelHandle handle = factory.handle(id);
        if (handle == null) return;
        final Tab existing = tabs.remove(id);
        if (existing != null) {
            existing.setContent(null);
            if (existing.getTabPane() != null) existing.getTabPane().getTabs().remove(existing);
        }
        final Tab tab = new Tab();
        final Label label = new Label(id.title());
        label.getStyleClass().add("dock-tab-label");
        tab.setGraphic(label);
        tab.setClosable(false);
        tab.setContent(handle.content());
        tab.setUserData(id);
        installDrag(label, id);
        if (index < 0 || index > pane.getTabs().size()) pane.getTabs().add(tab);
        else pane.getTabs().add(index, tab);
        tabs.put(id, tab);
        if (active || pane.getTabs().size() == 1) pane.getSelectionModel().select(tab);
    }

    private void installDrag(Label label, PanelId id) {
        label.setOnDragDetected(event -> {
            final Dragboard board = label.startDragAndDrop(TransferMode.MOVE);
            final ClipboardContent content = new ClipboardContent();
            content.put(PANEL_FORMAT, id.key());
            board.setContent(content);
            event.consume();
        });
        label.setOnMouseClicked(event -> {
            if (event.getClickCount() == 2 && !shuttingDown) {
                floatPanel(id);
                event.consume();
            }
        });
        label.setOnDragDone(event -> {
            if (event.getTransferMode() == null && !shuttingDown) {
                try {
                    final var point = MouseInfo.getPointerInfo().getLocation();
                    floatPanel(id, point.x - 80.0, point.y - 40.0);
                } catch (Exception ignored) {
                }
            }
        });
    }

    private void configureDock(TabPane pane, int dock) {
        pane.setOnDragOver(event -> {
            if (event.getDragboard().hasContent(PANEL_FORMAT)) {
                event.acceptTransferModes(TransferMode.MOVE);
            }
            event.consume();
        });
        pane.setOnDragDropped(event -> {
            final Object value = event.getDragboard().getContent(PANEL_FORMAT);
            if (value instanceof String key) {
                final PanelId id = PanelId.fromKey(key);
                if (id != null) {
                    dockPanel(id, dock, pane.getTabs().size());
                    event.setDropCompleted(true);
                }
            }
            event.consume();
        });
    }

    private TabPane dockPane(int dock) {
        return dock == 1 ? right : dock == 2 ? bottom : left;
    }

    private int dockOf(PanelId id) {
        final Tab tab = tabs.get(id);
        if (tab == null || tab.getTabPane() == null) return 0;
        if (tab.getTabPane() == right) return 1;
        if (tab.getTabPane() == bottom) return 2;
        return 0;
    }

    public void dockPanel(PanelId id, int dock, int index) {
        closeFloatingStage(id, true);
        addTab(id, dockPane(dock), index, true);
        final PanelHandle handle = factory.handle(id);
        if (handle != null) handle.lastDock(dock);
        saveLayout();
    }

    public void floatPanel(PanelId id) {
        final Stage main = stages.main();
        final double x = main == null ? 180 : main.getX() + 180;
        final double y = main == null ? 140 : main.getY() + 140;
        floatPanel(id, x, y);
    }

    private void floatPanel(PanelId id, double x, double y) {
        if (floating.containsKey(id) || shuttingDown) return;
        final PanelHandle handle = factory.handle(id);
        if (handle == null) return;
        final int lastDock = dockOf(id);
        handle.lastDock(lastDock);
        final Tab tab = tabs.remove(id);
        if (tab != null) {
            tab.setContent(null);
            if (tab.getTabPane() != null) tab.getTabPane().getTabs().remove(tab);
        }
        final BorderPane root = new BorderPane();
        root.getStyleClass().add("floating-panel");
        final Label title = new Label(handle.title());
        title.getStyleClass().add("floating-title");
        final Region spacer = new Region();
        HBox.setHgrow(spacer, Priority.ALWAYS);
        final Button dock = new Button("Andocken");
        dock.setOnAction(event -> dockPanel(id, lastDock, Integer.MAX_VALUE));
        final HBox bar = new HBox(8, title, spacer, dock);
        bar.getStyleClass().add("floating-bar");
        root.setTop(bar);
        root.setCenter(handle.content());
        final Scene scene = new Scene(root, 380, 560);
        scene.getStylesheets().add(getClass().getResource("/css/editor.css").toExternalForm());
        final Stage stage = new Stage();
        stage.initOwner(stages.main());
        stage.setTitle("sgl · " + handle.title());
        stage.setScene(scene);
        final var bounds = Screen.getPrimary().getVisualBounds();
        stage.setX(Math.clamp(x, bounds.getMinX(), Math.max(bounds.getMaxX() - 380, bounds.getMinX())));
        stage.setY(Math.clamp(y, bounds.getMinY(), Math.max(bounds.getMaxY() - 560, bounds.getMinY())));
        stage.setOnCloseRequest(event -> {
            if (shuttingDown || stages.isShuttingDown()) return;
            event.consume();
            dockPanel(id, lastDock, Integer.MAX_VALUE);
        });
        floating.put(id, stage);
        stages.addFloating(stage);
        stage.show();
        saveLayout();
    }

    private void restoreFloating() {
        if (stages.main() == null) {
            Platform.runLater(this::restoreFloating);
            return;
        }
        final Map<PanelId, double[]> pending = new HashMap<>(pendingFloating);
        pendingFloating.clear();
        for (final Map.Entry<PanelId, double[]> entry : pending.entrySet()) {
            if (floating.containsKey(entry.getKey())) continue;
            final double[] bounds = entry.getValue();
            floatPanel(entry.getKey(), bounds[0], bounds[1]);
            final Stage stage = floating.get(entry.getKey());
            if (stage != null && bounds[2] > 120 && bounds[3] > 120) {
                stage.setWidth(bounds[2]);
                stage.setHeight(bounds[3]);
            }
        }
    }

    private void closeFloatingStage(PanelId id, boolean remove) {
        final Stage stage = floating.remove(id);
        if (stage == null) return;
        stage.setOnCloseRequest(null);
        final var root = stage.getScene() == null ? null : stage.getScene().getRoot();
        if (root instanceof BorderPane pane) pane.setCenter(null);
        stage.hide();
        stages.removeFloating(stage);
    }

    private void dockAllFloating() {
        for (final PanelId id : List.copyOf(floating.keySet())) {
            final PanelHandle handle = factory.handle(id);
            final int dock = handle == null ? 0 : handle.lastDock();
            closeFloatingStage(id, true);
            addTab(id, dockPane(dock), Integer.MAX_VALUE, true);
        }
        pendingFloating.clear();
    }

    @PreDestroy
    public void shutdown() {
        shuttingDown = true;
        dockAllFloating();
    }

    public boolean isShuttingDown() {
        return shuttingDown;
    }
}
