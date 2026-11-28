package de.sgl.editor.model;

import java.util.ArrayList;
import java.util.List;

public class ObjectLayer {
    public String name = "Objekte";
    public boolean visible = true;
    public boolean locked = false;
    public final List<Entity> entities = new ArrayList<>();

    public ObjectLayer copy() {
        final ObjectLayer copy = new ObjectLayer();
        copy.name = name;
        copy.visible = visible;
        copy.locked = locked;
        for (final Entity entity : entities) {
            copy.entities.add(entity.copy());
        }
        return copy;
    }
}
