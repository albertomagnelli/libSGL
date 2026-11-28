package de.sgl.editor.service;

import jakarta.enterprise.context.ApplicationScoped;

import javafx.stage.Stage;

import java.util.ArrayList;
import java.util.List;

/** Hauptfenster und schwebende Panel-Fenster. */
@ApplicationScoped
public class StageService {
    private Stage main;
    private final List<Stage> floating = new ArrayList<>();
    private boolean shuttingDown;

    public void setMain(Stage stage) {
        this.main = stage;
    }

    public Stage main() {
        return main;
    }

    public void addFloating(Stage stage) {
        floating.add(stage);
    }

    public void removeFloating(Stage stage) {
        floating.remove(stage);
    }

    public List<Stage> floating() {
        return List.copyOf(floating);
    }

    public boolean isShuttingDown() {
        return shuttingDown;
    }

    public void closeFloating() {
        shuttingDown = true;
        for (Stage stage : List.copyOf(floating)) {
            stage.close();
        }
        floating.clear();
    }
}
