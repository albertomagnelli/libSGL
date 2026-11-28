package de.sgl.editor.service;

import jakarta.enterprise.context.ApplicationScoped;

import javafx.collections.FXCollections;
import javafx.collections.ObservableList;

import java.time.LocalTime;
import java.time.format.DateTimeFormatter;
import java.util.List;

@ApplicationScoped
public class LogService {
    private static final DateTimeFormatter FORMAT = DateTimeFormatter.ofPattern("HH:mm:ss");
    private final ObservableList<String> lines = FXCollections.observableArrayList();

    public ObservableList<String> lines() {
        return lines;
    }

    public void log(String message) {
        lines.add(FORMAT.format(LocalTime.now()) + "  " + message);
        if (lines.size() > 250) lines.remove(0, 50);
    }

    public void clear() {
        lines.clear();
    }

    public List<String> copy() {
        return List.copyOf(lines);
    }
}
