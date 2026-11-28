package de.sgl.editor.ui.docking;

public enum PanelId {
    PROJECT("project", "Projekt"),
    PALETTE("palette", "Kacheln"),
    ENTITIES("entities", "Entities"),
    INSPECTOR("inspector", "Inspector"),
    LAYERS("layers", "Ebenen"),
    CONSOLE("console", "Konsole"),
    ASSETS("assets", "Assets");

    private final String key;
    private final String title;

    PanelId(String key, String title) {
        this.key = key;
        this.title = title;
    }

    public String key() { return key; }
    public String title() { return title; }

    public static PanelId fromKey(String key) {
        for (final PanelId id : values()) if (id.key.equals(key)) return id;
        return null;
    }
}
