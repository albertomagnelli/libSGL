#include <sgl/MeshFont.hpp>

#include <sgl/Font.hpp>

#include <glm/geometric.hpp>
#include <sgl/Log.hpp>

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>

extern "C" {
#include "ttf2mesh.h"
}

namespace sgl {
namespace {

// Fehlertext zu einem TTF_XXX-Code, mit Hinweis auf den häufigsten Grund.
std::string ErrorText(const i32 code, const std::string& file) {
    std::string reason;
    switch (code) {
        case TTF_ERR_NOMEM:
            reason = "kein Speicher";
            break;
        case TTF_ERR_SIZE:
            reason = "Datei ist größer als 32 MB";
            break;
        case TTF_ERR_OPEN:
            reason = "Datei nicht lesbar";
            break;
        case TTF_ERR_VER:
            reason = "unbekannte sfnt-Version (erwartet TrueType oder 'OTTO')";
            break;
        case TTF_ERR_FMT:
            reason = "beschädigte Dateistruktur";
            break;
        case TTF_ERR_NOTAB:
            reason = "keine 'glyf'-Tabelle – reine CFF-Schrift (.otf), bitte nach TTF wandeln";
            break;
        case TTF_ERR_CSUM:
            reason = "Prüfsumme stimmt nicht";
            break;
        case TTF_ERR_UTAB:
            reason = "nicht unterstütztes Tabellenformat";
            break;
        case TTF_ERR_NO_OUTLINE:
            reason = "Zeichen ohne Umriss";
            break;
        default:
            reason = "Fehler " + std::to_string(code);
            break;
    }
    return "Schrift '" + file + "' konnte nicht geladen werden: " + reason;
}

std::string FamilyOf(const ttf_t* font) {
    if (font == nullptr || font->names.family == nullptr) {
        return {};
    }
    return font->names.family;
}

f32 AdvanceOf(const ttf_t* font, const u32 codepoint) {
    const i32 index = ttf_find_glyph(font, codepoint);
    return index < 0 ? 0.0f : font->glyphs[index].advance;
}

f32 KerningOf(const ttf_t* font, const u32 left, const u32 right) {
    if (font == nullptr || font->em_scale <= 0.0f) {
        return 0.0f;
    }
    // ttf_kern liefert Font-Einheiten, die Metriken sind schon in EM
    return static_cast<f32>(ttf_kern(font, left, right)) * font->em_scale;
}

// Breite der breitesten Zeile in EM-Einheiten
f32 MeasureEm(const ttf_t* font, const std::string_view text) {
    f32 widest = 0.0f;
    f32 current = 0.0f;
    bool pendingKerning = false;
    u32 previous = 0;

    for (const u32 codepoint : DecodeUtf8(text)) {
        if (codepoint == '\n') {
            widest = std::max(widest, current);
            current = 0.0f;
            pendingKerning = false;
            continue;
        }
        if (codepoint == '\t') {
            current += AdvanceOf(font, ' ') * 4.0f;
            pendingKerning = false;
            continue;
        }
        if (pendingKerning) {
            current += KerningOf(font, previous, codepoint);
        }
        current += AdvanceOf(font, codepoint);
        previous = codepoint;
        pendingKerning = true;
    }
    return std::max(widest, current);
}

}  // namespace

// ─── Font2d ─────────────────────────────────────────────────────────────────

Font2d::~Font2d() {
    if (m_font != nullptr) {
        ttf_free(m_font);
    }
}

Font2d::Font2d(Font2d&& other) noexcept : m_font(other.m_font), m_family(std::move(other.m_family)) {
    other.m_font = nullptr;
}

Font2d& Font2d::operator=(Font2d&& other) noexcept {
    if (this != &other) {
        if (m_font != nullptr) {
            ttf_free(m_font);
        }
        m_font = other.m_font;
        m_family = std::move(other.m_family);
        other.m_font = nullptr;
    }
    return *this;
}

Font2d Font2d::fromFile(const std::filesystem::path& file) {
    Font2d font;
    ttf_t* raw = nullptr;
    const i32 result = ttf_load_from_file(file.string().c_str(), &raw, false);
    if (result != TTF_DONE || raw == nullptr) {
        throw MeshFontError(ErrorText(result, file.string()));
    }
    font.m_font = raw;
    font.m_family = FamilyOf(raw);
    SLOGD << "Font2d geladen: " << font.m_family << " (" << raw->nglyphs << " Glyphen)";
    return font;
}

Font2d Font2d::fromMemory(std::span<const u8> data) {
    if (data.empty()) {
        throw MeshFontError("Font2d: leerer Puffer");
    }
    Font2d font;
    ttf_t* raw = nullptr;
    if (const i32 result = ttf_load_from_mem(
            data.data(), static_cast<int>(data.size()), &raw, false);
        result != TTF_DONE || raw == nullptr) {
        throw MeshFontError(ErrorText(result, "<Speicher>"));
    }
    font.m_font = raw;
    font.m_family = FamilyOf(raw);
    return font;
}

u32 Font2d::glyphCount() const noexcept {
    return m_font == nullptr ? 0 : static_cast<u32>(m_font->nglyphs);
}

f32 Font2d::ascent() const noexcept {
    return m_font == nullptr ? 0.0f : m_font->hhea.ascender;
}

f32 Font2d::descent() const noexcept {
    return m_font == nullptr ? 0.0f : -m_font->hhea.descender;
}

f32 Font2d::lineHeight() const noexcept {
    if (m_font == nullptr) {
        return 0.0f;
    }
    return m_font->hhea.ascender - m_font->hhea.descender + m_font->hhea.lineGap;
}

bool Font2d::hasGlyph(const u32 codepoint) const {
    return m_font != nullptr && ttf_find_glyph(m_font, codepoint) >= 0;
}

f32 Font2d::advance(const u32 codepoint) const {
    return m_font == nullptr ? 0.0f : AdvanceOf(m_font, codepoint);
}

f32 Font2d::kerning(const u32 left, const u32 right) const {
    return KerningOf(m_font, left, right);
}

f32 Font2d::measure(const std::string_view text, const f32 pixelSize) const {
    if (m_font == nullptr) {
        return 0.0f;
    }
    return MeasureEm(m_font, text) * pixelSize;
}

u32 Font2d::lineCount(const std::string_view text) {
    u32 lines = 1;
    for (const char character : text) {
        if (character == '\n') {
            ++lines;
        }
    }
    return lines;
}

GlyphMesh Font2d::glyphMesh(const u32 codepoint, const MeshQuality quality) const {
    GlyphMesh result;
    if (m_font == nullptr) {
        return result;
    }
    const i32 index = ttf_find_glyph(m_font, codepoint);
    if (index < 0) {
        return result;
    }

    ttf_mesh_t* mesh = nullptr;
    const i32 status = ttf_glyph2mesh(&m_font->glyphs[index], &mesh,
                                      static_cast<u8>(quality), TTF_FEATURES_DFLT);
    if (status == TTF_ERR_NO_OUTLINE || mesh == nullptr) {
        ttf_free_mesh(mesh);
        return result;  // Leerzeichen und Ähnliches: nur Vorschub
    }
    if (status != TTF_DONE) {
        ttf_free_mesh(mesh);
        throw MeshFontError("Font2d: Netz für U+" + std::to_string(codepoint) + " fehlgeschlagen (" +
                            std::to_string(status) + ")");
    }

    result.positions.reserve(static_cast<std::size_t>(mesh->nvert));
    for (auto i = 0; i < mesh->nvert; ++i) {
        result.positions.emplace_back(mesh->vert[i].x, mesh->vert[i].y);
    }
    result.indices.reserve(static_cast<std::size_t>(mesh->nfaces) * 3);
    for (auto i = 0; i < mesh->nfaces; ++i) {
        result.indices.push_back(static_cast<u32>(mesh->faces[i].v1));
        result.indices.push_back(static_cast<u32>(mesh->faces[i].v2));
        result.indices.push_back(static_cast<u32>(mesh->faces[i].v3));
    }
    ttf_free_mesh(mesh);
    return result;
}

bool Font2d::glyphBounds(const u32 codepoint, vec2& minimum, vec2& maximum) const {
    if (m_font == nullptr) {
        return false;
    }
    const i32 index = ttf_find_glyph(m_font, codepoint);
    if (index < 0) {
        return false;
    }
    const ttf_glyph_t* glyph = &m_font->glyphs[index];
    minimum = vec2(glyph->xbounds[0], glyph->ybounds[0]);
    maximum = vec2(glyph->xbounds[1], glyph->ybounds[1]);
    return glyph->xbounds[1] > glyph->xbounds[0];
}

// ─── Font3d ─────────────────────────────────────────────────────────────────

Font3d::~Font3d() {
    if (m_font != nullptr) {
        ttf_free(m_font);
    }
}

Font3d::Font3d(Font3d&& other) noexcept : m_font(other.m_font), m_family(std::move(other.m_family)) {
    other.m_font = nullptr;
}

Font3d& Font3d::operator=(Font3d&& other) noexcept {
    if (this != &other) {
        if (m_font != nullptr) {
            ttf_free(m_font);
        }
        m_font = other.m_font;
        m_family = std::move(other.m_family);
        other.m_font = nullptr;
    }
    return *this;
}

Font3d Font3d::fromFile(const std::filesystem::path& file) {
    Font3d font;
    ttf_t* raw = nullptr;
    const i32 result = ttf_load_from_file(file.string().c_str(), &raw, false);
    if (result != TTF_DONE || raw == nullptr) {
        throw MeshFontError(ErrorText(result, file.string()));
    }
    font.m_font = raw;
    font.m_family = FamilyOf(raw);
    SLOGD << "Font3d geladen: " << font.m_family;
    return font;
}

Font3d Font3d::fromMemory(std::span<const u8> data) {
    if (data.empty()) {
        throw MeshFontError("Font3d: leerer Puffer");
    }
    Font3d font;
    ttf_t* raw = nullptr;
    const i32 result = ttf_load_from_mem(data.data(), static_cast<int>(data.size()), &raw, false);
    if (result != TTF_DONE || raw == nullptr) {
        throw MeshFontError(ErrorText(result, "<Speicher>"));
    }
    font.m_font = raw;
    font.m_family = FamilyOf(raw);
    return font;
}

u32 Font3d::glyphCount() const noexcept {
    return m_font == nullptr ? 0 : static_cast<u32>(m_font->nglyphs);
}

f32 Font3d::ascent() const noexcept {
    return m_font == nullptr ? 0.0f : m_font->hhea.ascender;
}

f32 Font3d::descent() const noexcept {
    return m_font == nullptr ? 0.0f : -m_font->hhea.descender;
}

f32 Font3d::lineHeight() const noexcept {
    if (m_font == nullptr) {
        return 0.0f;
    }
    return m_font->hhea.ascender - m_font->hhea.descender + m_font->hhea.lineGap;
}

bool Font3d::hasGlyph(const u32 codepoint) const {
    return m_font != nullptr && ttf_find_glyph(m_font, codepoint) >= 0;
}

f32 Font3d::advance(const u32 codepoint) const {
    return m_font == nullptr ? 0.0f : AdvanceOf(m_font, codepoint);
}

f32 Font3d::kerning(const u32 left, const u32 right) const {
    return KerningOf(m_font, left, right);
}

f32 Font3d::measure(const std::string_view text, const f32 pixelSize) const {
    if (m_font == nullptr) {
        return 0.0f;
    }
    return MeasureEm(m_font, text) * pixelSize;
}

GlyphMesh3d Font3d::glyphMesh3d(const u32 codepoint, const f32 depth,
                                const MeshQuality quality) const {
    GlyphMesh3d result;
    if (m_font == nullptr || depth <= 0.0f) {
        return result;
    }
    const i32 index = ttf_find_glyph(m_font, codepoint);
    if (index < 0) {
        return result;
    }

    ttf_mesh3d_t* mesh = nullptr;
    const i32 status = ttf_glyph2mesh3d(&m_font->glyphs[index], &mesh, static_cast<u8>(quality),
                                        TTF_FEATURES_DFLT, depth);
    if (status == TTF_ERR_NO_OUTLINE || mesh == nullptr) {
        ttf_free_mesh3d(mesh);
        return result;
    }
    if (status != TTF_DONE) {
        ttf_free_mesh3d(mesh);
        throw MeshFontError("Font3d: Netz für U+" + std::to_string(codepoint) + " fehlgeschlagen (" +
                            std::to_string(status) + ")");
    }

    result.positions.reserve(static_cast<std::size_t>(mesh->nvert));
    for (auto i = 0; i < mesh->nvert; ++i) {
        result.positions.emplace_back(mesh->vert[i].x, mesh->vert[i].y, mesh->vert[i].z);
    }
    if (mesh->normals != nullptr) {
        // ttf2mesh legt die Normalen pro Eckpunkt an (nvert Einträge), nicht
        // pro Dreieck. Die Zuordnung erfolgt über dieselben Vertexindizes.
        result.normals.reserve(static_cast<std::size_t>(mesh->nvert));
        for (int i = 0; i < mesh->nvert; ++i) {
            // ttf2mesh liefert die Wandnormalen nicht zwingend normiert
            vec3 normal(mesh->normals[i].x, mesh->normals[i].y, mesh->normals[i].z);
            const f32 length = glm::length(normal);
            result.normals.push_back(length > 1e-6f ? normal / length : vec3(0.0f, 0.0f, 1.0f));
        }
    }
    result.indices.reserve(static_cast<std::size_t>(mesh->nfaces) * 3);
    for (int i = 0; i < mesh->nfaces; ++i) {
        result.indices.push_back(static_cast<u32>(mesh->faces[i].v1));
        result.indices.push_back(static_cast<u32>(mesh->faces[i].v2));
        result.indices.push_back(static_cast<u32>(mesh->faces[i].v3));
    }
    ttf_free_mesh3d(mesh);
    return result;
}

// ─── Schriftsuche ───────────────────────────────────────────────────────────

std::filesystem::path FindSystemFont(const std::string& preferred) {
    for (const char* variable : {"SGL_FONT", "LIBRAY_FONT"}) {
        if (const char* fromEnvironment = std::getenv(variable);
            fromEnvironment != nullptr && *fromEnvironment != '\0') {
            if (std::filesystem::exists(fromEnvironment)) {
                return fromEnvironment;
            }
        }
    }
    const char* directories[] = {"/usr/share/fonts/truetype/dejavu/", "/usr/share/fonts/TTF/",
                                 "/usr/share/fonts/truetype/", "/usr/local/share/fonts/"};
    for (const char* directory : directories) {
        const std::filesystem::path candidate = std::filesystem::path(directory) / preferred;
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }
    return {};
}

}  // namespace sgl
