#pragma once

// Kleiner JSON-Writer für Projekt-, Level- und Tiled-Dateien.
// Der vorhandene sgl-Json-Leser bleibt unverändert; dieses Modul schreibt.

#include <sgl/Color.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace editor {

class JsonWriter {
public:
    JsonWriter() = default;

    [[nodiscard]] std::string take();

    void beginObject();
    void endObject();
    void beginArray();
    void endArray();

    void key(std::string_view name);
    void value(std::string_view text);
    void value(const char* text) { value(std::string_view(text)); }
    void value(double number);
    void value(int number) { value(static_cast<double>(number)); }
    void value(bool flag);
    void color(const sgl::Color& value);
    void null();
    // Fügt bereits serialisiertes JSON ein (z. B. unveränderte Tiled-Metadaten).
    void raw(std::string_view json);

private:
    void prefix();
    static std::string escape(std::string_view text);

    struct Scope {
        bool object = true;
        bool empty = true;
    };

    std::vector<Scope> m_scopes;
    std::string m_out;
    bool m_afterKey = false;
};

} // namespace editor
