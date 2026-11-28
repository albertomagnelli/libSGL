package de.sgl.editor.model;

import java.util.ArrayList;
import java.util.List;

public class TileAnimation {
    public record Frame(int tileId, int durationMs) {}

    public List<Frame> frames = new ArrayList<>();
}
