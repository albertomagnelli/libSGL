package de.sgl.editor.ui.viewport;

import de.sgl.editor.model.*;
import de.sgl.editor.service.EditorState;
import de.sgl.editor.service.LogService;
import de.sgl.editor.service.ProjectService;
import de.sgl.editor.service.TileImageCache;

import jakarta.annotation.PostConstruct;
import jakarta.annotation.PreDestroy;
import jakarta.enterprise.context.Dependent;
import jakarta.inject.Inject;

import javafx.animation.AnimationTimer;
import javafx.scene.canvas.Canvas;
import javafx.scene.canvas.GraphicsContext;
import javafx.scene.image.Image;
import javafx.scene.input.MouseButton;
import javafx.scene.layout.Region;
import javafx.scene.paint.Color;
import javafx.scene.text.Font;

import java.util.ArrayDeque;
import java.util.Deque;

@Dependent
public class ViewportPane extends Region {
    private static final double MINIMAP_W = 200;
    private static final double MINIMAP_H = 130;

    @Inject ProjectService projects;
    @Inject EditorState state;
    @Inject TileImageCache images;
    @Inject LogService log;

    private final Canvas canvas = new Canvas(800, 600);
    private boolean dirty = true;
    private long lastFrame = System.nanoTime();
    private double fps = 60;
    private long drawWindowStart = System.nanoTime();
    private int drawCount;
    private long lastAnimationCheck;
    private AnimationTimer timer;
    private double panStartX;
    private double panStartY;
    private double cameraStartX;
    private double cameraStartY;
    private boolean panning;
    private boolean stroking;
    private boolean eraseStroke;
    private int strokeStartX;
    private int strokeStartY;
    private int strokeEndX;
    private int strokeEndY;
    private boolean movingEntity;
    private int movingEntityId = -1;
    private double entityStartX;
    private double entityStartY;
    private double mouseStartX;
    private double mouseStartY;
    private boolean minimapDrag;
    private double mouseX = -1;
    private double mouseY = -1;
    private boolean mouseInside;

    public ViewportPane() {
        getChildren().add(canvas);
        setFocusTraversable(true);
        canvas.widthProperty().bind(widthProperty());
        canvas.heightProperty().bind(heightProperty());
        widthProperty().addListener((observable, oldValue, newValue) -> dirty = true);
        heightProperty().addListener((observable, oldValue, newValue) -> dirty = true);
    }

    @PostConstruct
    public void initialize() {
        projects.activeDocument().addListener((observable, oldValue, newValue) -> {
            if (newValue != null) centerOnLevel(newValue.level);
            dirty = true;
        });
        state.zoom().addListener((observable, oldValue, newValue) -> dirty = true);
        state.cameraX().addListener((observable, oldValue, newValue) -> dirty = true);
        state.cameraY().addListener((observable, oldValue, newValue) -> dirty = true);
        state.showGrid().addListener((observable, oldValue, newValue) -> dirty = true);
        state.showMinimap().addListener((observable, oldValue, newValue) -> dirty = true);
        state.selectedTile().addListener((observable, oldValue, newValue) -> dirty = true);
        state.selectedEntityId().addListener((observable, oldValue, newValue) -> dirty = true);
        canvas.setOnMouseMoved(event -> {
            mouseX = event.getX();
            mouseY = event.getY();
            mouseInside = true;
            dirty = true;
        });
        canvas.setOnMouseExited(event -> {
            mouseInside = false;
            dirty = true;
        });
        canvas.setOnMousePressed(this::onPressed);
        canvas.setOnMouseDragged(this::onDragged);
        canvas.setOnMouseReleased(this::onReleased);
        canvas.setOnScroll(event -> {
            final Level level = projects.activeLevel();
            if (level == null) return;
            final double beforeX = worldX(event.getX());
            final double beforeY = worldY(event.getY());
            final double zoom = Math.clamp(state.zoom().get() * Math.exp(event.getDeltaY() * 0.0015), 0.12, 8);
            state.zoom().set(zoom);
            final double afterX = worldX(event.getX());
            final double afterY = worldY(event.getY());
            state.cameraX().set(state.cameraX().get() + beforeX - afterX);
            state.cameraY().set(state.cameraY().get() + beforeY - afterY);
            dirty = true;
            event.consume();
        });
        timer = new AnimationTimer() {
            @Override public void handle(long now) {
                lastFrame = now;
                if (projects.activeLevel() != null && hasAnimations(projects.activeLevel())
                        && now - lastAnimationCheck > 100_000_000L) {
                    lastAnimationCheck = now;
                    dirty = true;
                }
                if (dirty) {
                    dirty = false;
                    draw();
                    drawCount++;
                    if (now - drawWindowStart >= 500_000_000L) {
                        fps = drawCount / ((now - drawWindowStart) / 1_000_000_000.0);
                        drawWindowStart = now;
                        drawCount = 0;
                        state.fps().set(Math.min(fps, 240));
                    }
                }
            }
        };
        timer.start();
    }

    public void requestRedraw() {
        dirty = true;
    }

    private static boolean hasAnimations(Level level) {
        for (final TileSet tileSet : level.tilesets) {
            if (!tileSet.animations.isEmpty()) return true;
        }
        return false;
    }

    @PreDestroy
    public void shutdown() {
        if (timer != null) timer.stop();
    }

    private void centerOnLevel(Level level) {
        state.cameraX().set(level.width * level.tileW * 0.5);
        state.cameraY().set(level.height * level.tileH * 0.5);
        state.zoom().set(1.0);
        state.selectLevelDefaults(level);
    }

    private double worldX(double screenX) {
        return (screenX - canvas.getWidth() * 0.5) / state.zoom().get() + state.cameraX().get();
    }

    private double worldY(double screenY) {
        return (screenY - canvas.getHeight() * 0.5) / state.zoom().get() + state.cameraY().get();
    }

    private double screenX(double worldX) {
        return canvas.getWidth() * 0.5 + (worldX - state.cameraX().get()) * state.zoom().get();
    }

    private double screenY(double worldY) {
        return canvas.getHeight() * 0.5 + (worldY - state.cameraY().get()) * state.zoom().get();
    }

    private int cellX(double screenX) {
        final Level level = projects.activeLevel();
        return level == null ? -1 : (int) Math.floor(worldX(screenX) / level.tileW);
    }

    private int cellY(double screenY) {
        final Level level = projects.activeLevel();
        return level == null ? -1 : (int) Math.floor(worldY(screenY) / level.tileH);
    }

    private void onPressed(javafx.scene.input.MouseEvent event) {
        requestFocus();
        final Level level = projects.activeLevel();
        if (level == null) return;
        final double minimapX = canvas.getWidth() - MINIMAP_W - 12;
        final double minimapY = canvas.getHeight() - MINIMAP_H - 12;
        if (state.showMinimap().get() && event.getX() >= minimapX && event.getY() >= minimapY) {
            minimapDrag = true;
            jumpMinimap(event.getX() - minimapX, event.getY() - minimapY, level);
            return;
        }
        if (event.getButton() == MouseButton.MIDDLE || state.tool().get() == EditorState.Tool.PAN) {
            panning = true;
            panStartX = event.getX();
            panStartY = event.getY();
            cameraStartX = state.cameraX().get();
            cameraStartY = state.cameraY().get();
            return;
        }
        final boolean erase = event.getButton() == MouseButton.SECONDARY;
        final int x = cellX(event.getX());
        final int y = cellY(event.getY());
        switch (state.tool().get()) {
            case SELECT -> {
                selectEntity(worldX(event.getX()), worldY(event.getY()));
                if (state.selectedEntityId().get() >= 0) {
                    final Entity entity = level.findEntity(state.selectedEntityId().get());
                    if (entity != null) {
                        movingEntity = true;
                        movingEntityId = entity.id;
                        entityStartX = entity.x;
                        entityStartY = entity.y;
                        mouseStartX = event.getX();
                        mouseStartY = event.getY();
                    }
                }
            }
            case ENTITY -> {
                final EntityDef def = state.selectedEntityDef().get();
                if (def != null && !erase && x >= 0 && y >= 0) {
                    addEntity(level, def, x, y);
                }
            }
            case PICKER -> {
                final TileLayer layer = state.activeTileLayer(level);
                if (layer != null && x >= 0 && y >= 0) {
                    final int gid = level.tileAt(layer, x, y);
                    if (gid != 0) state.selectedTile().set(gid);
                }
            }
            case FILL -> {
                if (!erase) floodFill(level, x, y, state.selectedTile().get());
                else floodFill(level, x, y, 0);
                projects.markDirty();
                projects.commit();
            }
            case BRUSH, ERASER, RECT, LINE -> {
                stroking = true;
                eraseStroke = erase || state.tool().get() == EditorState.Tool.ERASER;
                strokeStartX = x;
                strokeStartY = y;
                strokeEndX = x;
                strokeEndY = y;
                if (state.tool().get() == EditorState.Tool.BRUSH || state.tool().get() == EditorState.Tool.ERASER) {
                    paint(level, x, y, eraseStroke ? 0 : state.selectedTile().get());
                }
                dirty = true;
            }
            default -> {
            }
        }
    }

    private void onDragged(javafx.scene.input.MouseEvent event) {
        final Level level = projects.activeLevel();
        if (level == null) return;
        if (panning) {
            state.cameraX().set(cameraStartX - (event.getX() - panStartX) / state.zoom().get());
            state.cameraY().set(cameraStartY - (event.getY() - panStartY) / state.zoom().get());
            return;
        }
        if (minimapDrag) {
            jumpMinimap(event.getX() - (canvas.getWidth() - MINIMAP_W - 12),
                    event.getY() - (canvas.getHeight() - MINIMAP_H - 12), level);
            return;
        }
        if (movingEntity) {
            final Entity entity = level.findEntity(movingEntityId);
            if (entity != null) {
                entity.x = entityStartX + (event.getX() - mouseStartX) / state.zoom().get();
                entity.y = entityStartY + (event.getY() - mouseStartY) / state.zoom().get();
                if (!event.isAltDown()) {
                    entity.x = Math.round(entity.x / level.tileW) * level.tileW;
                    entity.y = Math.round(entity.y / level.tileH) * level.tileH;
                }
                projects.markDirty();
                dirty = true;
            }
            return;
        }
        if (stroking) {
            strokeEndX = cellX(event.getX());
            strokeEndY = cellY(event.getY());
            if (state.tool().get() == EditorState.Tool.BRUSH || state.tool().get() == EditorState.Tool.ERASER) {
                paint(level, strokeEndX, strokeEndY, eraseStroke ? 0 : state.selectedTile().get());
            }
            dirty = true;
        }
    }

    private void onReleased(javafx.scene.input.MouseEvent event) {
        final Level level = projects.activeLevel();
        if (level == null) return;
        if (minimapDrag) {
            minimapDrag = false;
            return;
        }
        if (panning) {
            panning = false;
            return;
        }
        if (movingEntity) {
            movingEntity = false;
            projects.commit();
            return;
        }
        if (stroking) {
            if (state.tool().get() == EditorState.Tool.RECT) {
                paintRect(level, strokeStartX, strokeStartY, strokeEndX, strokeEndY,
                        eraseStroke ? 0 : state.selectedTile().get());
            } else if (state.tool().get() == EditorState.Tool.LINE) {
                paintLine(level, strokeStartX, strokeStartY, strokeEndX, strokeEndY,
                        eraseStroke ? 0 : state.selectedTile().get());
            }
            stroking = false;
            projects.markDirty();
            projects.commit();
        }
    }

    private void addEntity(Level level, EntityDef def, int cellX, int cellY) {
        final ObjectLayer layer = state.activeObjectLayer(level);
        if (layer == null || layer.locked) return;
        final Entity entity = new Entity();
        entity.id = level.nextEntityId++;
        entity.def = def.name;
        entity.x = cellX * (double) level.tileW;
        entity.y = cellY * (double) level.tileH;
        entity.w = level.tileW;
        entity.h = level.tileH;
        for (final AttributeDef attribute : def.attributes) {
            entity.attributes.put(attribute.name, attribute.defaultValue);
        }
        layer.entities.add(entity);
        state.selectedEntityId().set(entity.id);
        projects.markDirty();
        projects.commit();
    }

    private void selectEntity(double worldX, double worldY) {
        final Level level = projects.activeLevel();
        if (level == null) return;
        for (int l = level.objectLayers.size() - 1; l >= 0; l--) {
            final ObjectLayer layer = level.objectLayers.get(l);
            if (!layer.visible || layer.locked) continue;
            for (int e = layer.entities.size() - 1; e >= 0; e--) {
                final Entity entity = layer.entities.get(e);
                if (worldX >= entity.x && worldY >= entity.y && worldX <= entity.x + entity.w
                        && worldY <= entity.y + entity.h) {
                    state.selectedEntityId().set(entity.id);
                    state.objectLayerSelected().set(true);
                    state.activeObjectLayer().set(l);
                    return;
                }
            }
        }
        state.selectedEntityId().set(-1);
    }

    private void paint(Level level, int x, int y, int value) {
        final TileLayer layer = state.activeTileLayer(level);
        if (layer == null || layer.locked || !layer.visible) return;
        if (x < 0 || y < 0 || x >= level.width || y >= level.height) return;
        if (level.tileAt(layer, x, y) != value) {
            level.setTileAt(layer, x, y, value);
            projects.markDirty();
        }
    }

    private void paintRect(Level level, int x0, int y0, int x1, int y1, int value) {
        final int left = Math.clamp(Math.min(x0, x1), 0, level.width - 1);
        final int right = Math.clamp(Math.max(x0, x1), 0, level.width - 1);
        final int top = Math.clamp(Math.min(y0, y1), 0, level.height - 1);
        final int bottom = Math.clamp(Math.max(y0, y1), 0, level.height - 1);
        for (int y = top; y <= bottom; y++) for (int x = left; x <= right; x++) paint(level, x, y, value);
    }

    private void paintLine(Level level, int x0, int y0, int x1, int y1, int value) {
        int x = x0;
        int y = y0;
        final int dx = Math.abs(x1 - x0);
        final int sx = x0 < x1 ? 1 : -1;
        final int dy = -Math.abs(y1 - y0);
        final int sy = y0 < y1 ? 1 : -1;
        int error = dx + dy;
        while (true) {
            paint(level, x, y, value);
            if (x == x1 && y == y1) break;
            final int e2 = 2 * error;
            if (e2 >= dy) { error += dy; x += sx; }
            if (e2 <= dx) { error += dx; y += sy; }
        }
    }

    private void floodFill(Level level, int x, int y, int value) {
        final TileLayer layer = state.activeTileLayer(level);
        if (layer == null || layer.locked || x < 0 || y < 0 || x >= level.width || y >= level.height) return;
        final int original = level.tileAt(layer, x, y);
        if (original == value) return;
        final Deque<int[]> stack = new ArrayDeque<>();
        stack.push(new int[]{x, y});
        int guard = 0;
        while (!stack.isEmpty() && guard++ < 250000) {
            final int[] point = stack.pop();
            if (point[0] < 0 || point[1] < 0 || point[0] >= level.width || point[1] >= level.height) continue;
            if (level.tileAt(layer, point[0], point[1]) != original) continue;
            level.setTileAt(layer, point[0], point[1], value);
            stack.push(new int[]{point[0] + 1, point[1]});
            stack.push(new int[]{point[0] - 1, point[1]});
            stack.push(new int[]{point[0], point[1] + 1});
            stack.push(new int[]{point[0], point[1] - 1});
        }
    }

    private void jumpMinimap(double localX, double localY, Level level) {
        final double scale = Math.min((MINIMAP_W - 8) / (level.width * level.tileW),
                (MINIMAP_H - 8) / (level.height * level.tileH));
        final double offsetX = (MINIMAP_W - level.width * level.tileW * scale) * 0.5;
        final double offsetY = (MINIMAP_H - level.height * level.tileH * scale) * 0.5;
        state.cameraX().set(Math.clamp((localX - offsetX) / scale, 0, level.width * level.tileW));
        state.cameraY().set(Math.clamp((localY - offsetY) / scale, 0, level.height * level.tileH));
    }

    private void draw() {
        final GraphicsContext gc = canvas.getGraphicsContext2D();
        final double width = Math.max(canvas.getWidth(), 1);
        final double height = Math.max(canvas.getHeight(), 1);
        gc.setFill(Color.web("#14161b"));
        gc.fillRect(0, 0, width, height);
        final Level level = projects.activeLevel();
        if (level == null) {
            gc.setFill(Color.web("#646b7a"));
            gc.setFont(Font.font(15));
            gc.fillText("Kein Level geöffnet – Datei → Neues Projekt…", 24, 40);
            return;
        }
        final double zoom = state.zoom().get();
        final double left = worldX(0);
        final double top = worldY(0);
        final double right = worldX(width);
        final double bottom = worldY(height);
        final int x0 = Math.max((int) Math.floor(left / level.tileW) - 1, 0);
        final int y0 = Math.max((int) Math.floor(top / level.tileH) - 1, 0);
        final int x1 = Math.min((int) Math.ceil(right / level.tileW) + 1, level.width);
        final int y1 = Math.min((int) Math.ceil(bottom / level.tileH) + 1, level.height);
        final double seconds = System.nanoTime() / 1_000_000_000.0;
        for (final TileLayer layer : level.tileLayers) {
            if (!layer.visible) continue;
            gc.setGlobalAlpha(Math.clamp(layer.opacity, 0, 1));
            for (int y = y0; y < y1; y++) {
                for (int x = x0; x < x1; x++) {
                    int gid = level.tileAt(layer, x, y);
                    if (gid == 0) continue;
                    gid = level.animatedTile(gid, seconds);
                    final TileSet tileSet = level.tileSetForGid(gid);
                    if (tileSet == null) continue;
                    final Image image = images.image(tileSet);
                    if (image == null) continue;
                    final int columns = Math.max(tileSet.columns, 1);
                    final int local = gid - tileSet.firstGid;
                    final double sx = tileSet.margin + (local % columns) * (tileSet.tileW + tileSet.spacing);
                    final double sy = tileSet.margin + (local / columns) * (tileSet.tileH + tileSet.spacing);
                    gc.drawImage(image, sx, sy, tileSet.tileW, tileSet.tileH,
                            screenX(x * (double) level.tileW), screenY(y * (double) level.tileH),
                            level.tileW * zoom, level.tileH * zoom);
                }
            }
        }
        gc.setGlobalAlpha(1);
        if (state.showGrid().get()) {
            final double stepX = level.tileW * zoom;
            final double stepY = level.tileH * zoom;
            if (stepX >= 6 && stepY >= 6) {
                for (int x = 0; x <= level.width; x++) {
                    final double sx = screenX(x * (double) level.tileW);
                    gc.setStroke(x % 8 == 0 ? Color.web("#4e5668") : Color.web("#3a404e"));
                    gc.setLineWidth(1);
                    gc.strokeLine(sx, 0, sx, height);
                }
                for (int y = 0; y <= level.height; y++) {
                    final double sy = screenY(y * (double) level.tileH);
                    gc.setStroke(y % 8 == 0 ? Color.web("#4e5668") : Color.web("#3a404e"));
                    gc.strokeLine(0, sy, width, sy);
                }
            }
        }
        for (final ObjectLayer layer : level.objectLayers) {
            if (!layer.visible) continue;
            for (final Entity entity : layer.entities) {
                final Color color = entityColor(entity);
                final double x = screenX(entity.x);
                final double y = screenY(entity.y);
                gc.setFill(new Color(color.getRed(), color.getGreen(), color.getBlue(), 0.28));
                gc.fillRect(x, y, entity.w * zoom, entity.h * zoom);
                gc.setStroke(color);
                gc.setLineWidth(entity.id == state.selectedEntityId().get() ? 2.5 : 1.5);
                gc.strokeRect(x, y, entity.w * zoom, entity.h * zoom);
                if (entity.w * zoom > 38 && entity.h * zoom > 16) {
                    gc.setFill(Color.web("#e0e4ec"));
                    gc.setFont(Font.font(11));
                    gc.fillText(entity.attributes.getOrDefault("Name", entity.def), x + 3, y + 13);
                }
            }
        }
        if (stroking && (state.tool().get() == EditorState.Tool.RECT || state.tool().get() == EditorState.Tool.LINE)) {
            gc.setStroke(Color.web("#68a4f0"));
            gc.setLineWidth(2);
            if (state.tool().get() == EditorState.Tool.RECT) {
                final int leftCell = Math.min(strokeStartX, strokeEndX);
                final int topCell = Math.min(strokeStartY, strokeEndY);
                final int rightCell = Math.max(strokeStartX, strokeEndX);
                final int bottomCell = Math.max(strokeStartY, strokeEndY);
                gc.strokeRect(screenX(leftCell * (double) level.tileW), screenY(topCell * (double) level.tileH),
                        (rightCell - leftCell + 1) * level.tileW * zoom,
                        (bottomCell - topCell + 1) * level.tileH * zoom);
            } else {
                gc.strokeLine(screenX((strokeStartX + 0.5) * level.tileW),
                        screenY((strokeStartY + 0.5) * level.tileH),
                        screenX((strokeEndX + 0.5) * level.tileW),
                        screenY((strokeEndY + 0.5) * level.tileH));
            }
        }
        if (mouseInside) {
            final int hoverX = cellX(mouseX);
            final int hoverY = cellY(mouseY);
            if (hoverX >= 0 && hoverY >= 0 && hoverX < level.width && hoverY < level.height) {
                gc.setStroke(Color.web("#68a4f0"));
                gc.setLineWidth(1.5);
                gc.strokeRect(screenX(hoverX * (double) level.tileW), screenY(hoverY * (double) level.tileH),
                        level.tileW * zoom, level.tileH * zoom);
            }
            state.status().set("Zelle " + hoverX + ", " + hoverY);
        }
        if (state.showMinimap().get()) drawMinimap(gc, level);
    }

    private void drawMinimap(GraphicsContext gc, Level level) {
        final double x = canvas.getWidth() - MINIMAP_W - 12;
        final double y = canvas.getHeight() - MINIMAP_H - 12;
        gc.setFill(Color.web("#121419", 0.9));
        gc.fillRect(x, y, MINIMAP_W, MINIMAP_H);
        gc.setStroke(Color.web("#373d4b"));
        gc.strokeRect(x, y, MINIMAP_W, MINIMAP_H);
        final double scale = Math.min((MINIMAP_W - 8) / (level.width * level.tileW),
                (MINIMAP_H - 8) / (level.height * level.tileH));
        final double offsetX = x + (MINIMAP_W - level.width * level.tileW * scale) * 0.5;
        final double offsetY = y + (MINIMAP_H - level.height * level.tileH * scale) * 0.5;
        for (final TileLayer layer : level.tileLayers) {
            if (!layer.visible) continue;
            for (int ty = 0; ty < level.height; ty += 2) {
                for (int tx = 0; tx < level.width; tx += 2) {
                    if (level.tileAt(layer, tx, ty) == 0) continue;
                    gc.setFill(Color.web("#6e82a0", 0.55));
                    gc.fillRect(offsetX + tx * level.tileW * scale, offsetY + ty * level.tileH * scale,
                            Math.max(level.tileW * scale * 2, 1), Math.max(level.tileH * scale * 2, 1));
                }
            }
        }
        gc.setStroke(Color.web("#68a4f0"));
        gc.setLineWidth(1);
        gc.strokeRect(offsetX + worldX(0) * scale, offsetY + worldY(0) * scale,
                (worldX(canvas.getWidth()) - worldX(0)) * scale,
                (worldY(canvas.getHeight()) - worldY(0)) * scale);
    }

    private Color entityColor(Entity entity) {
        final var project = projects.currentProject();
        if (project != null) {
            final var def = project.findEntityDef(entity.def);
            if (def != null) {
                try {
                    return Color.web(def.color);
                } catch (IllegalArgumentException ignored) {
                }
            }
        }
        return Color.web("#7896be");
    }
}
