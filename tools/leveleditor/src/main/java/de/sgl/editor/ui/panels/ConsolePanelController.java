package de.sgl.editor.ui.panels;

import de.sgl.editor.service.LogService;

import jakarta.enterprise.context.Dependent;
import jakarta.inject.Inject;

import javafx.fxml.FXML;
import javafx.scene.control.ListView;

@Dependent
public class ConsolePanelController {
    @Inject LogService log;

    @FXML private ListView<String> lines;

    @FXML
    public void initialize() {
        lines.setItems(log.lines());
        lines.scrollTo(log.lines().size());
        log.lines().addListener((javafx.collections.ListChangeListener<String>) change ->
                lines.scrollTo(log.lines().size() - 1));
    }

    @FXML
    private void onClear() {
        log.clear();
    }
}
