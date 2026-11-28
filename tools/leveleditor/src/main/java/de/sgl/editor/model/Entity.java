package de.sgl.editor.model;

import java.util.LinkedHashMap;
import java.util.Map;

public class Entity {
    public int id;
    public String def = "Objekt";
    public double x;
    public double y;
    public double w = 32;
    public double h = 32;
    public double rotation;
    public String templatePath = "";
    public final Map<String, String> attributes = new LinkedHashMap<>();

    public Entity copy() {
        final Entity copy = new Entity();
        copy.id = id;
        copy.def = def;
        copy.x = x;
        copy.y = y;
        copy.w = w;
        copy.h = h;
        copy.rotation = rotation;
        copy.templatePath = templatePath;
        copy.attributes.putAll(attributes);
        return copy;
    }
}
