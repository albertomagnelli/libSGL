package de.sgl.editor.model;

import java.util.ArrayList;
import java.util.List;

public class AttributeDef {
    public String name = "Attribut";
    public String type = "string";
    public String defaultValue = "";
    public final List<String> values = new ArrayList<>();

    public AttributeDef() {}

    public AttributeDef(String name, String type, String defaultValue) {
        this.name = name;
        this.type = type;
        this.defaultValue = defaultValue;
    }

    public AttributeDef copy() {
        final AttributeDef copy = new AttributeDef(name, type, defaultValue);
        copy.values.addAll(values);
        return copy;
    }
}
