package de.sgl.editor.model;

import java.util.ArrayList;
import java.util.List;

public class EntityDef {
    public String name = "Objekt";
    public String color = "#78a0c8";
    public final List<AttributeDef> attributes = new ArrayList<>();

    public EntityDef() {}

    public EntityDef(String name, String color) {
        this.name = name;
        this.color = color;
    }

    public EntityDef copy() {
        final EntityDef copy = new EntityDef(name, color);
        for (final AttributeDef attribute : attributes) {
            copy.attributes.add(attribute.copy());
        }
        return copy;
    }
}
