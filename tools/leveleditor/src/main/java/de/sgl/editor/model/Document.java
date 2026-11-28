package de.sgl.editor.model;

import java.nio.file.Path;

public class Document {
    public Path path;
    public String title;
    public Level level;
    public final UndoManager undo = new UndoManager();
    public boolean dirty;

    public Document(Path path, String title, Level level) {
        this.path = path;
        this.title = title;
        this.level = level;
        undo.reset(level);
    }
}
