package de.sgl.editor.ui.panels;

import de.sgl.editor.model.Document;
import de.sgl.editor.model.Level;
import de.sgl.editor.model.TileSet;
import de.sgl.editor.service.EditorState;
import de.sgl.editor.service.ProjectService;
import de.sgl.editor.service.TileImageCache;
import de.sgl.editor.ui.dialogs.FileDialogs;

import jakarta.enterprise.context.Dependent;
import jakarta.inject.Inject;

import javafx.fxml.FXML;
import javafx.scene.canvas.Canvas;
import javafx.scene.canvas.GraphicsContext;
import javafx.scene.control.ComboBox;
import javafx.scene.control.Label;
import javafx.scene.control.ListCell;
import javafx.scene.control.ScrollPane;
import javafx.scene.image.Image;
import javafx.scene.paint.Color;

import java.util.List;

@Dependent
public class PalettePanelController {
    private static final double CELL = 40;

    @Inject ProjectService projects;
    @Inject EditorState state;
    @Inject TileImageCache images;
    @Inject FileDialogs dialogs;

    @FXML private ComboBox<TileSet> tilesetChoice;
    @FXML private Canvas canvas;
    @FXML private ScrollPane scroll;
    @FXML private Label wangLabel;
    @FXML private Label infoLabel;

    @FXML
    public void initialize() {
        tilesetChoice.setCellFactory(list -> new ListCell<>() {
            @Override protected void updateItem(TileSet item, boolean empty) {
                super.updateItem(item, empty);
                setText(empty || item == null ? "" : item.name);
            }
        });
        tilesetChoice.setButtonCell(new ListCell<>() {
            @Override protected void updateItem(TileSet item, boolean empty) {
                super.updateItem(item, empty);
                setText(empty || item == null ? "" : item.name);
            }
        });
        tilesetChoice.valueProperty().addListener((observable, oldValue, newValue) -> draw());
        projects.activeDocument().addListener((observable, oldValue, newValue) -> refresh());
        projects.project().addListener((observable, oldValue, newValue) -> refresh());
        canvas.setOnMousePressed(event -> {
            final TileSet tileSet = tilesetChoice.getValue();
            if (tileSet == null) return;
            final int columns = Math.max(tileSet.columns, 1);
            final int column = (int) (event.getX() / CELL);
            final int row = (int) (event.getY() / CELL);
            final int index = row * columns + column;
            if (column < 0 || row < 0 || column >= columns || index >= tileSet.tileCount) return;
            state.selectedTile().set(tileSet.firstGid + index);
            draw();
        });
        refresh();
    }

    @FXML
    private void onImport() {
        dialogs.importTileset();
    }

    private void refresh() {
        final Level level = projects.activeLevel();
        tilesetChoice.getItems().clear();
        if (level != null) tilesetChoice.getItems().addAll(level.tilesets);
        if (!tilesetChoice.getItems().isEmpty()) {
            final int index = Math.clamp(tilesetChoice.getSelectionModel().getSelectedIndex(), 0,
                    tilesetChoice.getItems().size() - 1);
            tilesetChoice.getSelectionModel().select(index);
            if (state.selectedTile().get() == 0) {
                state.selectedTile().set(tilesetChoice.getItems().get(index).firstGid);
            }
        }
        draw();
    }

    private void draw() {
        final GraphicsContext gc = canvas.getGraphicsContext2D();
        final TileSet tileSet = tilesetChoice.getValue();
        gc.setFill(Color.web("#1f222a"));
        gc.fillRect(0, 0, Math.max(canvas.getWidth(), 1), Math.max(canvas.getHeight(), 1));
        if (tileSet == null) {
            canvas.setWidth(Math.max(scroll.getViewportBounds().getWidth() - 4, 220));
            canvas.setHeight(180);
            gc.setFill(Color.web("#646b7a"));
            gc.fillText("Kein Tileset importiert.", 10, 22);
            infoLabel.setText("");
            wangLabel.setText("");
            return;
        }
        final Image image = images.image(tileSet);
        final int columns = Math.max(tileSet.columns, 1);
        final int count = Math.max(tileSet.tileCount, 0);
        final int rows = Math.max((count + columns - 1) / columns, 1);
        canvas.setWidth(Math.max(columns * CELL, scroll.getViewportBounds().getWidth() - 4));
        canvas.setHeight(Math.max(rows * CELL + 4, 120));
        for (int i = 0; i < count; i++) {
            final int column = i % columns;
            final int row = i / columns;
            final double x = column * CELL + 2;
            final double y = row * CELL + 2;
            gc.setFill(Color.web("#262a34"));
            gc.fillRect(x, y, CELL - 4, CELL - 4);
            if (image != null) {
                final double sx = tileSet.margin + column * (tileSet.tileW + tileSet.spacing);
                final double sy = tileSet.margin + row * (tileSet.tileH + tileSet.spacing);
                gc.drawImage(image, sx, sy, tileSet.tileW, tileSet.tileH, x + 1, y + 1, CELL - 6, CELL - 6);
            }
            if (tileSet.firstGid + i == state.selectedTile().get()) {
                gc.setStroke(Color.web("#68a4f0"));
                gc.setLineWidth(2);
                gc.strokeRect(x - 1, y - 1, CELL - 2, CELL - 2);
            }
        }
        infoLabel.setText("GID " + state.selectedTile().get() + " · " + tileSet.tileW + "×" + tileSet.tileH
                + " · " + tileSet.animations.size() + " Animationen");
        if (tileSet.wangsets != null && tileSet.wangsets.isArray()) {
            final StringBuilder names = new StringBuilder();
            for (int i = 0; i < Math.min(tileSet.wangsets.size(), 3); i++) {
                if (!names.isEmpty()) names.append(", ");
                names.append(tileSet.wangsets.get(i).path("name").asText("?"));
            }
            wangLabel.setText("Wang: " + names);
        } else {
            wangLabel.setText("");
        }
    }
}
