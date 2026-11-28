package de.sgl.editor.ui;

import de.sgl.editor.cdi.CdiContext;

import jakarta.enterprise.context.ApplicationScoped;
import jakarta.inject.Inject;

import javafx.fxml.FXMLLoader;
import javafx.scene.Parent;

import java.io.IOException;
import java.io.UncheckedIOException;
import java.net.URL;
import java.util.Locale;
import java.util.ResourceBundle;

/** Lädt FXML mit CDI-Controllern, deutschem ResourceBundle und CSS. */
@ApplicationScoped
public class FxLoader {
    public record View<T>(Parent root, T controller) {}

    @Inject
    CdiContext context;

    public <T> View<T> load(String resource) {
        final URL url = FxLoader.class.getResource(resource);
        if (url == null) {
            throw new IllegalArgumentException("FXML nicht gefunden: " + resource);
        }
        final ResourceBundle bundle = ResourceBundle.getBundle("i18n.messages", Locale.GERMAN);
        final FXMLLoader loader = new FXMLLoader(url, bundle);
        loader.setControllerFactory(type -> context.container().select(type).get());
        try {
            final Parent root = loader.load();
            return new View<>(root, loader.getController());
        } catch (IOException exception) {
            throw new UncheckedIOException("FXML konnte nicht geladen werden: " + resource, exception);
        }
    }

    public Parent loadRoot(String resource) {
        return load(resource).root();
    }
}
