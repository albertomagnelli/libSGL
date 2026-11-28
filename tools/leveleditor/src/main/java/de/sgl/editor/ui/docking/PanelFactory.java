package de.sgl.editor.ui.docking;

import de.sgl.editor.ui.FxLoader;

import jakarta.enterprise.context.ApplicationScoped;
import jakarta.inject.Inject;

import java.util.EnumMap;
import java.util.Map;

@ApplicationScoped
public class PanelFactory {
    @Inject
    FxLoader loader;

    private final Map<PanelId, PanelHandle> handles = new EnumMap<>(PanelId.class);

    public void loadAll() {
        if (!handles.isEmpty()) return;
        add(PanelId.PROJECT, "/fxml/ProjectPanel.fxml");
        add(PanelId.PALETTE, "/fxml/PalettePanel.fxml");
        add(PanelId.ENTITIES, "/fxml/EntitiesPanel.fxml");
        add(PanelId.INSPECTOR, "/fxml/InspectorPanel.fxml");
        add(PanelId.LAYERS, "/fxml/LayersPanel.fxml");
        add(PanelId.CONSOLE, "/fxml/ConsolePanel.fxml");
        add(PanelId.ASSETS, "/fxml/AssetsPanel.fxml");
    }

    private void add(PanelId id, String resource) {
        final FxLoader.View<?> view = loader.load(resource);
        handles.put(id, new PanelHandle(id, view.root(), view.controller()));
    }

    public PanelHandle handle(PanelId id) {
        return handles.get(id);
    }

    public Map<PanelId, PanelHandle> handles() {
        return handles;
    }
}
