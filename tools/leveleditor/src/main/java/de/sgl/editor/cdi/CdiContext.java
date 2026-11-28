package de.sgl.editor.cdi;

import jakarta.enterprise.context.ApplicationScoped;
import jakarta.enterprise.inject.se.SeContainer;

/** Hält den CDI-Container für FXMLLoader und Controller-Factories. */
@ApplicationScoped
public class CdiContext {
    private SeContainer container;

    public void set(SeContainer value) {
        this.container = value;
    }

    public SeContainer container() {
        return container;
    }
}
