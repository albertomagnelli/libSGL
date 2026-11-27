#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// Dokumentation mit Beispielen und Screenshots: docs/meshfont.md
//
// Schriften als Dreiecksnetze (Font2d, Font3d). Öffentliche Schnittstelle dieses Headers:
//
//   MeshFontError, Font2d, FromFile, FromMemory, ascent
//   descent, lineHeight, advance, kerning, measure
//   LineCount, Font3d, FindSystemFont
//
// Beispiel:
//
//   Font2d font = Font2d::FromFile("DejaVuSans.ttf");
//   GlyphMesh mesh = font.glyphMesh('A', MeshQuality::High);
//   Font3d font3d = Font3d::FromFile("DejaVuSans.ttf");
//   GlyphMesh3d solid = font3d.glyphMesh3d('A', 0.2f);
// ─────────────────────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────────────────────
// sgl · MeshFont
//
// Schriften als Dreiecksnetze – ohne Rasterung, ohne Atlas. Grundlage ist
// ttf2mesh (mit Kerning-Patch, siehe tools/ttf2mesh-patch): Glyphen werden aus
// der 'glyf'-Tabelle gelesen und tesselliert.
//
//   Font2d  – flache Glyphennetze (Dreiecke in der Ebene)
//   Font3d  – extrudierte Glyphennetze mit Normalen (für 3D-Schrift)
//
// Koordinaten sind EM-Einheiten: 1.0 ist die Geviertbreite, die Grundlinie
// liegt bei y = 0, y zeigt nach oben. Für Pixel gilt
//     x_px = x_em * pixelSize + Versatz,  y_px = baseline - y_em * pixelSize.
//
// Gezeichnet wird nicht hier: 2D-Netze können direkt in einen
// Vertexpuffer übernommen werden, 3D-Netze liefern zusätzlich Normalen.
//
// Unterstützt werden TrueType-Umrissse ('glyf'), also .ttf und .otf mit
// TrueType-Umrissen. Reine CFF-.otf werden abgewiesen (kein 'glyf'); dafür
// vorher konvertieren (otf2ttf).
//
// Konventionen: RAII, Methoden camelCase, bool-Getter "isX()", Member "m_",
// freie Funktionen mit Großbuchstaben.
//
// Beispiel:
//
//   const Font2d font = Font2d::FromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
//   const f32 breite = font.measure("Hallo Welt", 32.0f);
//   const GlyphMesh mesh = font.glyphMesh('A', MeshQuality::High);
//
//   const Font3d font3d = Font3d::FromFile(...);
//   const GlyphMesh3d lit = font3d.glyphMesh3d('A', 0.15f, MeshQuality::Normal);
// ─────────────────────────────────────────────────────────────────────────────

#include "Types.hpp"

#include <filesystem>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// Vorwärtsdeklaration aus third_party/ttf2mesh/ttf2mesh.h (dort global)
struct ttf_file;

namespace sgl {

// ─── Fehler ─────────────────────────────────────────────────────────────────

class MeshFontError : public std::runtime_error {
public:
    explicit MeshFontError(const std::string& message) : std::runtime_error(message) {}
};

// Punktzahl entlang des Umrisses; wirkt nur auf gekrümmte Konturen.
enum class MeshQuality : u8 { Low = 10, Normal = 20, High = 50 };

// ─── Netze ──────────────────────────────────────────────────────────────────

// Flaches Netz in EM-Einheiten. indices enthält je drei Eckpunkte ein Dreieck,
// die Windung ist durchgehend gegen den Uhrzeigersinn (GL-Vorgabe für
// Vorderseiten).
struct GlyphMesh {
    std::vector<vec2> positions;
    std::vector<u32> indices;

    [[nodiscard]] bool isEmpty() const noexcept { return indices.empty(); }
    [[nodiscard]] u32 triangleCount() const noexcept { return static_cast<u32>(indices.size() / 3); }
};

// Extrudiertes Netz: Vorder- und Rückseite plus Wand, Normalen je Eckpunkt.
struct GlyphMesh3d {
    std::vector<vec3> positions;
    std::vector<vec3> normals;
    std::vector<u32> indices;

    [[nodiscard]] bool isEmpty() const noexcept { return indices.empty(); }
    [[nodiscard]] u32 triangleCount() const noexcept { return static_cast<u32>(indices.size() / 3); }
};

// ─── Font2d ─────────────────────────────────────────────────────────────────

class Font2d {
public:
    Font2d() = default;
    ~Font2d();

    Font2d(Font2d&& other) noexcept;
    Font2d& operator=(Font2d&& other) noexcept;
    Font2d(const Font2d&) = delete;
    Font2d& operator=(const Font2d&) = delete;

    [[nodiscard]] static Font2d FromFile(const std::filesystem::path& file);
    [[nodiscard]] static Font2d FromMemory(std::span<const u8> data);

    // ── Zustand ──────────────────────────────────────────────────────────────
    [[nodiscard]] bool isValid() const noexcept { return m_font != nullptr; }
    [[nodiscard]] u32 glyphCount() const noexcept;
    // Maße in EM-Einheiten
    [[nodiscard]] f32 ascent() const noexcept;
    [[nodiscard]] f32 descent() const noexcept;
    [[nodiscard]] f32 lineHeight() const noexcept;
    [[nodiscard]] const std::string& familyName() const noexcept { return m_family; }

    [[nodiscard]] bool hasGlyph(u32 codepoint) const;

    // ── Metriken ─────────────────────────────────────────────────────────────
    // Vorschub in EM-Einheiten (0, wenn das Zeichen fehlt)
    [[nodiscard]] f32 advance(u32 codepoint) const;
    // Unterschneidung in EM-Einheiten (aus 'GPOS'/'kern')
    [[nodiscard]] f32 kerning(u32 left, u32 right) const;
    // Breite der breitesten Zeile in Pixeln
    [[nodiscard]] f32 measure(std::string_view text, f32 pixelSize) const;
    // Anzahl der Zeilen (Trennung an '\n')
    [[nodiscard]] static u32 LineCount(std::string_view text);

    // ── Netze ────────────────────────────────────────────────────────────────
    [[nodiscard]] GlyphMesh glyphMesh(u32 codepoint, MeshQuality quality = MeshQuality::Normal) const;
    // Umschließendes Rechteck eines Zeichens in EM (leer, wenn es fehlt)
    [[nodiscard]] bool glyphBounds(u32 codepoint, vec2& minimum, vec2& maximum) const;

protected:
    ::ttf_file* m_font{nullptr};
    std::string m_family;
};

// ─── Font3d ─────────────────────────────────────────────────────────────────

class Font3d {
public:
    Font3d() = default;
    ~Font3d();

    Font3d(Font3d&& other) noexcept;
    Font3d& operator=(Font3d&& other) noexcept;
    Font3d(const Font3d&) = delete;
    Font3d& operator=(const Font3d&) = delete;

    [[nodiscard]] static Font3d FromFile(const std::filesystem::path& file);
    [[nodiscard]] static Font3d FromMemory(std::span<const u8> data);

    [[nodiscard]] bool isValid() const noexcept { return m_font != nullptr; }
    [[nodiscard]] u32 glyphCount() const noexcept;
    [[nodiscard]] f32 ascent() const noexcept;
    [[nodiscard]] f32 descent() const noexcept;
    [[nodiscard]] f32 lineHeight() const noexcept;
    [[nodiscard]] const std::string& familyName() const noexcept { return m_family; }

    [[nodiscard]] bool hasGlyph(u32 codepoint) const;
    [[nodiscard]] f32 advance(u32 codepoint) const;
    [[nodiscard]] f32 kerning(u32 left, u32 right) const;
    // Breite der breitesten Zeile in Pixeln
    [[nodiscard]] f32 measure(std::string_view text, f32 pixelSize) const;

    // depth ist die Gesamttiefe in EM-Einheiten, z läuft von -depth/2 bis +depth/2
    [[nodiscard]] GlyphMesh3d glyphMesh3d(u32 codepoint, f32 depth,
                            MeshQuality quality = MeshQuality::Normal) const;

protected:
    ::ttf_file* m_font{nullptr};
    std::string m_family;
};

// ─── Freie Helfer ───────────────────────────────────────────────────────────

// Sucht eine TrueType-Schrift; liefert einen leeren Pfad, wenn keine gefunden wird.
// Reihenfolge: Umgebungsvariable LIBRAY_FONT, dann die üblichen Systempfade.
std::filesystem::path FindSystemFont(const std::string& preferred = "DejaVuSans.ttf");

}  // namespace sgl
