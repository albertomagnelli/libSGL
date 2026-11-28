package de.sgl.editor;

import java.nio.file.Path;

public record EditorOptions(Path project, Path frame, String level, int tab, boolean demo, boolean play,
                            String floatPanel) {
    public static EditorOptions parse(java.util.List<String> args) {
        Path project = null;
        Path frame = null;
        String level = null;
        int tab = -1;
        boolean demo = false;
        boolean play = false;
        String floatPanel = null;
        for (int i = 0; i < args.size(); i++) {
            final String argument = args.get(i);
            switch (argument) {
                case "--project" -> { if (i + 1 < args.size()) project = Path.of(args.get(++i)); }
                case "--frame" -> { if (i + 1 < args.size()) frame = Path.of(args.get(++i)); }
                case "--level" -> { if (i + 1 < args.size()) level = args.get(++i); }
                case "--tab" -> { if (i + 1 < args.size()) tab = Integer.parseInt(args.get(++i)); }
                case "--demo" -> demo = true;
                case "--play" -> play = true;
                case "--float" -> { if (i + 1 < args.size()) floatPanel = args.get(++i); }
                default -> { }
            }
        }
        return new EditorOptions(project, frame, level, tab, demo, play, floatPanel);
    }
}
