package de.sgl.editor.ui.playtest;

import de.sgl.editor.model.*;
import de.sgl.editor.service.EditorState;
import de.sgl.editor.service.ProjectService;
import de.sgl.editor.service.TileImageCache;

import jakarta.annotation.PreDestroy;
import jakarta.enterprise.context.Dependent;
import jakarta.inject.Inject;

import javafx.animation.AnimationTimer;
import javafx.scene.canvas.Canvas;
import javafx.scene.canvas.GraphicsContext;
import javafx.scene.image.Image;
import javafx.scene.input.KeyCode;
import javafx.scene.input.KeyEvent;
import javafx.scene.layout.StackPane;
import javafx.scene.paint.Color;
import javafx.scene.text.Font;

import java.util.EnumSet;
import java.util.Set;

@Dependent
public class PlaytestPane extends StackPane {
    private final Canvas canvas = new Canvas(800, 600);
    private final Set<KeyCode> keys = EnumSet.noneOf(KeyCode.class);
    private final Playtest playtest = new Playtest();
    private AnimationTimer timer;
    private long lastFrame;
    private Runnable onExit;
    private boolean running;

    @Inject ProjectService projects;
    @Inject EditorState state;
    @Inject TileImageCache images;

    public PlaytestPane() {
        getChildren().add(canvas);
        canvas.widthProperty().bind(widthProperty());
        canvas.heightProperty().bind(heightProperty());
        setVisible(false);
        setFocusTraversable(true);
        canvas.setFocusTraversable(true);
        canvas.setOnKeyPressed(this::onKeyPressed);
        canvas.setOnKeyReleased(event -> keys.remove(event.getCode()));
    }

    public void setOnExit(Runnable callback) {
        onExit = callback;
    }

    public boolean isRunning() {
        return running;
    }

    public void start() {
        final Level level = projects.activeLevel();
        if (level == null) return;
        playtest.reset(level);
        keys.clear();
        running = true;
        setVisible(true);
        toFront();
        canvas.requestFocus();
        lastFrame = System.nanoTime();
        timer = new AnimationTimer() {
            @Override public void handle(long now) {
                final double dt = Math.min((now - lastFrame) / 1_000_000_000.0, 0.05);
                lastFrame = now;
                final double move = (keys.contains(KeyCode.A) || keys.contains(KeyCode.LEFT) ? -1 : 0)
                        + (keys.contains(KeyCode.D) || keys.contains(KeyCode.RIGHT) ? 1 : 0);
                final boolean jump = keys.contains(KeyCode.SPACE) || keys.contains(KeyCode.W)
                        || keys.contains(KeyCode.UP);
                playtest.step(level, dt, move, jump);
                draw(level);
            }
        };
        timer.start();
    }

    @PreDestroy
    public void dispose() {
        if (timer != null) timer.stop();
        timer = null;
    }

    public void stop() {
        running = false;
        if (timer != null) timer.stop();
        timer = null;
        setVisible(false);
        keys.clear();
        if (onExit != null) onExit.run();
    }

    private void onKeyPressed(KeyEvent event) {
        if (event.getCode() == KeyCode.ESCAPE || event.getCode() == KeyCode.F5) {
            stop();
            event.consume();
            return;
        }
        keys.add(event.getCode());
        event.consume();
    }

    private void draw(Level level) {
        final GraphicsContext gc = canvas.getGraphicsContext2D();
        final double width = Math.max(canvas.getWidth(), 1);
        final double height = Math.max(canvas.getHeight(), 1);
        gc.setFill(Color.web("#14161b"));
        gc.fillRect(0, 0, width, height);
        final double zoom = 2.0;
        final double cameraX = playtest.x() + playtest.width() * 0.5;
        final double cameraY = playtest.y() + playtest.height() * 0.5 - 80;
        final double seconds = System.nanoTime() / 1_000_000_000.0;
        final double left = cameraX - width * 0.5 / zoom;
        final double top = cameraY - height * 0.5 / zoom;
        final int x0 = Math.max((int) Math.floor(left / level.tileW) - 2, 0);
        final int y0 = Math.max((int) Math.floor(top / level.tileH) - 2, 0);
        final int x1 = Math.min((int) Math.ceil((left + width / zoom) / level.tileW) + 2, level.width);
        final int y1 = Math.min((int) Math.ceil((top + height / zoom) / level.tileH) + 2, level.height);
        for (final TileLayer layer : level.tileLayers) {
            if (!layer.visible) continue;
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
                    final double dx = width * 0.5 + (x * (double) level.tileW - cameraX) * zoom;
                    final double dy = height * 0.5 + (y * (double) level.tileH - cameraY) * zoom;
                    gc.drawImage(image, sx, sy, tileSet.tileW, tileSet.tileH, dx, dy,
                            level.tileW * zoom, level.tileH * zoom);
                }
            }
        }
        for (final ObjectLayer layer : level.objectLayers) {
            if (!layer.visible) continue;
            for (final Entity entity : layer.entities) {
                final double dx = width * 0.5 + (entity.x - cameraX) * zoom;
                final double dy = height * 0.5 + (entity.y - cameraY) * zoom;
                Color color = Color.web("#7896be");
                final var project = projects.currentProject();
                if (project != null) {
                    final var def = project.findEntityDef(entity.def);
                    if (def != null) {
                        try {
                            color = Color.web(def.color);
                        } catch (IllegalArgumentException ignored) {
                        }
                    }
                }
                gc.setFill(new Color(color.getRed(), color.getGreen(), color.getBlue(), 0.3));
                gc.fillRect(dx, dy, entity.w * zoom, entity.h * zoom);
                gc.setStroke(color);
                gc.setLineWidth(1.5);
                gc.strokeRect(dx, dy, entity.w * zoom, entity.h * zoom);
            }
        }
        final double px = width * 0.5 + (playtest.x() - cameraX) * zoom;
        final double py = height * 0.5 + (playtest.y() - cameraY) * zoom;
        gc.setFill(Color.web("#50c878", 0.9));
        gc.fillRect(px, py, playtest.width() * zoom, playtest.height() * zoom);
        gc.setStroke(Color.web("#d6ffe4"));
        gc.setLineWidth(2);
        gc.strokeRect(px, py, playtest.width() * zoom, playtest.height() * zoom);

        gc.setFill(Color.web("#121419", 0.85));
        gc.fillRect(16, height - 58, width - 32, 42);
        gc.setStroke(Color.web("#373d4b"));
        gc.strokeRect(16, height - 58, width - 32, 42);
        gc.setFill(Color.web("#e0e4ec"));
        gc.setFont(Font.font(14));
        gc.fillText("Vorschau · A/D oder Pfeiltasten laufen · Leertaste/W springen · Esc beendet",
                width * 0.5 - 300, height - 32);
    }
}
