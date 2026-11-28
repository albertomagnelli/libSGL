package de.sgl.editor.ui.docking;

import javafx.scene.Parent;

public final class PanelHandle {
    private final PanelId id;
    private final Parent content;
    private final Object controller;
    private int lastDock;

    public PanelHandle(PanelId id, Parent content, Object controller) {
        this.id = id;
        this.content = content;
        this.controller = controller;
    }

    public PanelId id() { return id; }
    public String title() { return id.title(); }
    public Parent content() { return content; }
    public Object controller() { return controller; }
    public int lastDock() { return lastDock; }
    public void lastDock(int dock) { lastDock = dock; }
}
