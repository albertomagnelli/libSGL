#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// Glyphenatlas-Schriften (Bitmap und SDF). Öffentliche Schnittstelle dieses Headers:
//
//   FontError, Font, fromBitmap, fromSdf, load, loadDefault
//   g_defaultFont
//   save, isValid, hasGlyph, kerning, missingAdvance
//   setMetrics, addGlyph, addGlyphs, addKerning, setFallback
//   bounds, lineCount, kerningKey, rebuildIndex, DecodeUtf8
//   EncodeUtf8
//
// Beispiel:
//
//   Font font = Font::fromBitmap(atlas, metrics);
//   font.addGlyphs(glyphs);
//   font.addKerning('A', 'V', -1.5f);
//   f32 w = font.measure("Hallo", 24.0f);
// ─────────────────────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────────────────────
// sgl · Font
//
// Schrift aus einem Atlasbild: entweder als Bitmap (Deckung im Alpha- bzw.
// Graukanal) oder als Signed Distance Field (Distanz im Graukanal, dadurch
// beliebig skalierbar ohne Treppchen).
//
//   * Glyphen mit Atlasrechteck, Anlagepunkt (bearing) und Vorschub (advance)
//   * Schriftmaße (em-Größe, Ascent, Descent, Zeilenhöhe) in Schrifteinheiten
//   * Kerning-Paare, Fallback-Zeichen für unbekannte Codepoints
//   * UTF-8-Texte werden selbst dekodiert (DecodeUtf8), ungültige Bytes -> U+FFFD
//   * Vermessen ohne Zeichnen: measure() und bounds()
//   * Persistenz: Textdatei mit den Maßen, Atlasbild als PNG daneben
//     (save/load), damit Schriften austauschbar sind
//
// RAII: der Atlas ist ein Image im Objekt, kein GL-Objekt, keine rohen Puffer.
// Konventionen: Methoden camelCase, freie Funktionen mit Großbuchstaben,
// bool-Getter mit "is", Member "m_".
//
// Beispiel:
//
//   Font font = Font::load("assets/fonts/inter_16.font");
//   const f32 breite = font.measure("Hallo Welt", 24.0f);
//   const Rectangle flaeche = font.bounds("Hallo Welt", {20.0f, 40.0f}, 24.0f);
// ─────────────────────────────────────────────────────────────────────────────

#include "Types.hpp"

#include "Geometry.hpp"
#include "Image.hpp"

#include <filesystem>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace sgl {

// ─── Fehler ──────────────────────────────────────────────────────────────────

class FontError : public std::runtime_error {
public:
    explicit FontError(const std::string& message) : std::runtime_error(message) {}
};

// ─── Daten ───────────────────────────────────────────────────────────────────

// Ein Zeichen im Atlas. Alle Maße außer dem Atlasrechteck sind in
// Schrifteinheiten (em = FontMetrics::size).
struct Glyph {
    u32 codepoint{0};
    Rectangle atlas{0.0f, 0.0f, 0.0f, 0.0f};  // Ausschnitt im Atlasbild (Pixel)
    // Versatz vom Anlagepunkt (Grundlinie) zur linken oberen Ecke des Zeichens;
    // y zeigt in Textrichtung nach oben
    vec2 bearing{0.0f, 0.0f};
    f32 advance{0.0f};  // Vorschub bis zum nächsten Zeichen
};

struct FontMetrics {
    f32 size{16.0f};        // em-Größe, auf die Maße und Atlas sich beziehen
    f32 ascent{12.0f};      // über der Grundlinie
    f32 descent{4.0f};      // unter der Grundlinie
    f32 lineHeight{19.0f};  // Zeilenabstand
};

enum class FontKind { Bitmap, Sdf };

// ─── Schrift ────────────────────────────────────────────────────────────────

class Font {
public:
    Font() = default;  // ungültig, bis Atlas und Glyphen gesetzt sind

    // Atlas mit Deckung (Bitmap) bzw. Distanzfeld (SDF) übernehmen.
    [[nodiscard]] static Font fromBitmap(Image atlas, FontMetrics metrics);
    [[nodiscard]] static Font fromSdf(Image atlas, FontMetrics metrics, f32 spread = 8.0f);

    // Textdatei lesen/schreiben; das Atlasbild liegt daneben und wird
    // beim Speichern mitgeschrieben (gleicher Dateiname mit .png).
    [[nodiscard]] static Font load(const std::filesystem::path& file);
    // Eingebettete DejaVu-Teilmenge aus DefaultFont.cpp. Die Glyphen werden
    // beim Aufruf aus dem TrueType-Programm in einen Graustufenatlas gerastert
    // (191 Zeichen, ASCII 32..126 und Latin-1 160..255).
    [[nodiscard]] static Font loadDefault();
    void save(const std::filesystem::path& file) const;

    // ── Zustand ──────────────────────────────────────────────────────────────
    [[nodiscard]] bool isValid() const noexcept;
    [[nodiscard]] bool isSdf() const noexcept { return m_kind == FontKind::Sdf; }
    [[nodiscard]] FontKind kind() const noexcept { return m_kind; }
    [[nodiscard]] const Image& atlas() const noexcept { return m_atlas; }
    [[nodiscard]] const FontMetrics& metrics() const noexcept { return m_metrics; }
    [[nodiscard]] f32 spread() const noexcept { return m_spread; }
    [[nodiscard]] const std::vector<Glyph>& glyphs() const noexcept { return m_glyphs; }

    // nullptr, wenn der Codepoint nicht vorhanden ist
    [[nodiscard]] const Glyph* glyph(u32 codepoint) const;
    [[nodiscard]] bool hasGlyph(u32 codepoint) const;
    // Anpassung des Vorschubs für ein Paar (0, wenn kein Kerning hinterlegt ist)
    [[nodiscard]] f32 kerning(u32 left, u32 right) const;
    // Vorschub für Zeichen, die in der Schrift fehlen (Anteil der em-Größe)
    [[nodiscard]] f32 missingAdvance() const noexcept;

    // ── Aufbau ───────────────────────────────────────────────────────────────
    void setMetrics(const FontMetrics& metrics);
    void addGlyph(const Glyph &glyph);
    void addGlyphs(std::span<const Glyph> glyphs);
    void addKerning(u32 left, u32 right, f32 adjustment);
    // Zeichen für unbekannte Codepoints (z. B. '?')
    void setFallback(u32 codepoint);
    [[nodiscard]] std::optional<u32> fallback() const noexcept { return m_fallback; }

    // ── Vermessen (pixelSize = gewünschte em-Größe in Pixeln) ────────────────
    // Breite der breitesten Zeile
    [[nodiscard]] f32 measure(std::string_view utf8, f32 pixelSize) const;
    // Umschließendes Rechteck ab position (linke obere Ecke der ersten Zeile)
    [[nodiscard]] Rectangle bounds(std::string_view utf8, const vec2& position, f32 pixelSize) const;
    static i32 lineCount(std::string_view utf8) ;

private:
    [[nodiscard]] static u64 kerningKey(u32 left, u32 right) {
        return (static_cast<u64>(left) << 32) | static_cast<u64>(right);
    }
    void rebuildIndex();

    Image m_atlas{};
    FontMetrics m_metrics{};
    FontKind m_kind{FontKind::Bitmap};
    f32 m_spread{8.0f};
    std::vector<Glyph> m_glyphs{};
    std::unordered_map<u32, std::size_t> m_glyphIndex{};
    std::unordered_map<u64, f32> m_kerning{};
    std::optional<u32> m_fallback{};
};

// ─── Freie Helfer ───────────────────────────────────────────────────────────

// Standardschrift der Bibliothek. Wird beim ersten Window::create einmalig
// geladen und ist danach global verfügbar; ohne Fenster ist sie ungültig.
extern Font g_defaultFont;

// UTF-8 -> Codepoints; ungültige Sequenzen werden zu U+FFFD.
std::vector<u32> DecodeUtf8(std::string_view text);
// Umgekehrt, für Meldungen und Dateien.
std::string EncodeUtf8(u32 codepoint);

}  // namespace sgl
