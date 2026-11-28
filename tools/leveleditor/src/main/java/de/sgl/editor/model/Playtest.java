package de.sgl.editor.model;

/** Kleine Playtest-Physik: Schwerkraft, Laufen, Springen, Kachelkollision. */
public class Playtest {
    private static final double MOVE_SPEED = 260.0;
    private static final double JUMP_SPEED = 560.0;
    private static final double GRAVITY = 1600.0;
    private static final double MAX_FALL = 1400.0;

    private double x;
    private double y;
    private double width;
    private double height;
    private double vx;
    private double vy;
    private boolean grounded;

    public void reset(Level level) {
        width = Math.max(level.tileW * 0.72, 16);
        height = Math.max(level.tileH * 1.5, 28);
        x = level.tileW * 2.0;
        y = level.tileH * 2.0;
        vx = 0;
        vy = 0;
        grounded = false;
        for (final ObjectLayer layer : level.objectLayers) {
            for (final Entity entity : layer.entities) {
                if (!entity.def.equals("Startpunkt") && !entity.attributes.containsKey("Start")) continue;
                x = entity.x + (entity.w - width) * 0.5;
                y = entity.y + entity.h - height;
                liftOutOfSolid(level);
                return;
            }
        }
        liftOutOfSolid(level);
    }

    /**
     * Startpunkte liegen häufig genau auf der Oberkante einer Kachel. Liegt die
     * Spielfigur dennoch in fester Geometrie, wird sie vorsichtig nach oben
     * geschoben, damit sie nicht in der Wand feststeckt.
     */
    private void liftOutOfSolid(Level level) {
        final int maxSteps = Math.max(1, (int) Math.ceil(height) * 4);
        int steps = 0;
        while (y > 0 && overlapsSolid(level, x, y) && steps++ < maxSteps) {
            y -= 1;
        }
        if (y < 0) y = 0;
    }

    public void step(Level level, double dt, double move, boolean jump) {
        dt = Math.clamp(dt, 0, 0.05);
        vx = Math.clamp(move, -1, 1) * MOVE_SPEED;
        vy = Math.min(vy + GRAVITY * dt, MAX_FALL);
        if (jump && grounded) {
            vy = -JUMP_SPEED;
            grounded = false;
        }
        final double nextX = x + vx * dt;
        if (!overlapsSolid(level, nextX, y)) x = nextX;
        else vx = 0;
        final double nextY = y + vy * dt;
        if (!overlapsSolid(level, x, nextY)) {
            y = nextY;
            grounded = false;
        } else {
            if (vy > 0) {
                final double bottom = Math.floor((nextY + height) / level.tileH) * level.tileH;
                y = bottom - height;
                grounded = true;
            } else if (vy < 0) {
                final double top = (Math.floor(nextY / level.tileH) + 1) * level.tileH;
                y = top;
                grounded = false;
            }
            vy = 0;
        }
        x = Math.clamp(x, 0, Math.max(level.width * level.tileW - width, 0));
        if (y > level.height * level.tileH + 400) reset(level);
    }

    private boolean overlapsSolid(Level level, double px, double py) {
        final int x0 = (int) Math.floor(px / level.tileW);
        final int x1 = (int) Math.floor((px + width - 0.001) / level.tileW);
        final int y0 = (int) Math.floor(py / level.tileH);
        final int y1 = (int) Math.floor((py + height - 0.001) / level.tileH);
        for (final TileLayer layer : level.tileLayers) {
            if (!layer.visible) continue;
            // Reine Hintergrund-/Dekoebenen tragen keine Kollision. Die Prüfung
            // greift nur, wenn es zusätzlich eine echte Kachelebene gibt, damit
            // eine einzelne Ebene namens "Hintergrund" nicht durchlässig wird.
            if (level.tileLayers.size() > 1 && isDecorative(layer)) continue;
            for (int ty = y0; ty <= y1; ty++) {
                for (int tx = x0; tx <= x1; tx++) {
                    if (tx < 0 || ty < 0 || tx >= level.width || ty >= level.height) continue;
                    if (level.tileAt(layer, tx, ty) != 0) return true;
                }
            }
        }
        return false;
    }

    private boolean isDecorative(TileLayer layer) {
        final String name = layer.name == null ? "" : layer.name.toLowerCase(java.util.Locale.ROOT);
        return name.contains("hintergrund") || name.contains("background");
    }

    public double x() { return x; }
    public double y() { return y; }
    public double width() { return width; }
    public double height() { return height; }
    public boolean grounded() { return grounded; }
}
