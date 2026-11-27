#include <sgl/Font.hpp>

#include <sgl/DefaultFont.hpp>

#include <sgl/Log.hpp>
#include <stb_truetype.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

/* Fuer die eingebettete Standardschrift (Font::loadDefault). */
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

namespace sgl {
namespace {

constexpr auto kReplacementCharacter = 0xFFFDu;
// Vorschub für Zeichen, die in der Schrift fehlen (Anteil der em-Größe)
constexpr auto kMissingAdvanceRatio = 0.25f;

[[noreturn]] void ThrowFontError(const std::string &file, i32 line,
                                 const std::string &text) {
  throw FontError(file + ":" + std::to_string(line) + ": " + text);
}

} // namespace

// ─── Erzeugen ───────────────────────────────────────────────────────────────

Font Font::fromBitmap(Image atlas, const FontMetrics metrics) {
  Font font;
  font.m_atlas = std::move(atlas);
  font.m_metrics = metrics;
  font.m_kind = FontKind::Bitmap;
  return font;
}

Font Font::fromSdf(Image atlas, const FontMetrics metrics, const f32 spread) {
  Font font;
  font.m_atlas = std::move(atlas);
  font.m_metrics = metrics;
  font.m_kind = FontKind::Sdf;
  font.m_spread = std::max(spread, 0.5f);
  return font;
}

// ─── Zugriff ────────────────────────────────────────────────────────────────

void Font::rebuildIndex() {
  m_glyphIndex.clear();
  for (std::size_t index = 0; index < m_glyphs.size(); ++index) {
    m_glyphIndex[m_glyphs[index].codepoint] = index;
  }
}

bool Font::isValid() const noexcept {
  return !m_atlas.isEmpty() && !m_glyphs.empty() && m_metrics.size > 0.0f;
}

const Glyph *Font::glyph(const u32 codepoint) const {
  if (const auto found = m_glyphIndex.find(codepoint);
      found != m_glyphIndex.end()) {
    return &m_glyphs[found->second];
  }
  if (m_fallback.has_value()) {
    if (const auto substitute = m_glyphIndex.find(*m_fallback);
        substitute != m_glyphIndex.end()) {
      return &m_glyphs[substitute->second];
    }
  }
  return nullptr;
}

bool Font::hasGlyph(const u32 codepoint) const {
  return m_glyphIndex.contains(codepoint);
}

f32 Font::missingAdvance() const noexcept {
  return m_metrics.size * kMissingAdvanceRatio;
}

f32 Font::kerning(const u32 left, const u32 right) const {
  const auto found = m_kerning.find(kerningKey(left, right));
  return found != m_kerning.end() ? found->second : 0.0f;
}

void Font::setMetrics(const FontMetrics &metrics) { m_metrics = metrics; }

void Font::addGlyph(const Glyph &glyphValue) {
  m_glyphIndex[glyphValue.codepoint] = m_glyphs.size();
  m_glyphs.push_back(glyphValue);
}

void Font::addGlyphs(std::span<const Glyph> glyphs) {
  for (const Glyph &value : glyphs) {
    addGlyph(value);
  }
}

void Font::addKerning(const u32 left, const u32 right, const f32 adjustment) {
  m_kerning[kerningKey(left, right)] = adjustment;
}

void Font::setFallback(const u32 codepoint) { m_fallback = codepoint; }

// ─── Vermessen ──────────────────────────────────────────────────────────────

f32 Font::measure(const std::string_view utf8, const f32 pixelSize) const {
  if (!isValid() || pixelSize <= 0.0f) {
    return 0.0f;
  }
  const f32 scale = pixelSize / m_metrics.size;
  const f32 missingAdvance = m_metrics.size * kMissingAdvanceRatio;

  auto widest = 0.0f;
  auto current = 0.0f;
  std::optional<u32> previous;
  for (const u32 codepoint : DecodeUtf8(utf8)) {
    if (codepoint == '\n') {
      widest = std::max(widest, current);
      current = 0.0f;
      previous.reset();
      continue;
    }
    if (codepoint == '\r') {
      continue;
    }
    if (codepoint == '\t') {
      const Glyph *space = glyph(' ');
      current +=
          (space != nullptr ? space->advance : missingAdvance) * 4.0f * scale;
      previous.reset();
      continue;
    }
    if (previous.has_value()) {
      current += kerning(*previous, codepoint) * scale;
    }
    const Glyph *entry = glyph(codepoint);
    current += (entry != nullptr ? entry->advance : missingAdvance) * scale;
    previous = codepoint;
  }
  return std::max(widest, current);
}

i32 Font::lineCount(const std::string_view utf8) {
  if (utf8.empty()) {
    return 0;
  }
  auto lines = 1;
  for (const char character : utf8) {
    if (character == '\n') {
      ++lines;
    }
  }
  return lines;
}

Rectangle Font::bounds(const std::string_view utf8, const vec2 &position,
                       const f32 pixelSize) const {
  if (!isValid() || pixelSize <= 0.0f) {
    return Rectangle{position.x, position.y, 0.0f, 0.0f};
  }
  const f32 scale = pixelSize / m_metrics.size;
  const i32 lines = lineCount(utf8);
  const f32 width = measure(utf8, pixelSize);
  const f32 height = lines <= 0
                         ? 0.0f
                         : (static_cast<f32>(lines - 1) * m_metrics.lineHeight +
                            m_metrics.ascent + m_metrics.descent) *
                               scale;
  return Rectangle{position.x, position.y, width, height};
}

// ─── Speichern und Laden ────────────────────────────────────────────────────

void Font::save(const std::filesystem::path &file) const {
  if (!isValid()) {
    throw FontError("save: Schrift enthält keinen Atlas oder keine Glyphen");
  }
  std::filesystem::path atlasName = file.stem();
  atlasName += ".png";
  const std::filesystem::path atlasPath =
      file.parent_path().empty() ? atlasName : file.parent_path() / atlasName;
  ImageIO::save(m_atlas, atlasPath);

  std::ofstream output(file, std::ios::trunc);
  if (!output) {
    throw FontError("Schriftdatei konnte nicht geschrieben werden: " +
                    file.string());
  }
  output.precision(6);
  output << "sgl-font 1\n";
  output << "kind " << (isSdf() ? "sdf" : "bitmap") << "\n";
  output << "size " << m_metrics.size << "\n";
  output << "ascent " << m_metrics.ascent << "\n";
  output << "descent " << m_metrics.descent << "\n";
  output << "lineHeight " << m_metrics.lineHeight << "\n";
  output << "spread " << m_spread << "\n";
  output << "atlas " << atlasName.string() << "\n";
  for (const Glyph &entry : m_glyphs) {
    output << "glyph " << entry.codepoint << ' ' << entry.atlas.x << ' '
           << entry.atlas.y << ' ' << entry.atlas.width << ' '
           << entry.atlas.height << ' ' << entry.bearing.x << ' '
           << entry.bearing.y << ' ' << entry.advance << "\n";
  }
  for (const auto &[key, adjustment] : m_kerning) {
    output << "kerning " << static_cast<u32>(key >> 32) << ' '
           << static_cast<u32>(key & 0xFFFFFFFFu) << ' ' << adjustment << "\n";
  }
  if (m_fallback.has_value()) {
    output << "fallback " << *m_fallback << "\n";
  }
  if (!output) {
    throw FontError("Schreiben der Schriftdatei fehlgeschlagen: " +
                    file.string());
  }
  SLOGD << "Schrift gespeichert: " << file.string() << " (" << m_glyphs.size()
       << " Glyphen)";
}

Font Font::load(const std::filesystem::path &file) {
  std::ifstream input(file);
  if (!input) {
    throw FontError("Schriftdatei konnte nicht geöffnet werden: " +
                    file.string());
  }

  Font font;
  std::string line;
  std::string atlasName;
  auto headerSeen = false;
  auto lineNumber = 0;

  while (std::getline(input, line)) {
    ++lineNumber;
    if (line.empty() || line.front() == '#') {
      continue;
    }
    std::istringstream stream(line);
    std::string keyword;
    stream >> keyword;

    if (keyword == "sgl-font" || keyword == "libray-font") {
      auto version = 0;
      stream >> version;
      if (stream.fail() || version != 1) {
        ThrowFontError(file.string(), lineNumber, "unbekannte Formatversion");
      }
      headerSeen = true;
      continue;
    }
    if (!headerSeen) {
      ThrowFontError(file.string(), lineNumber,
                     "Kopfzeile 'sgl-font 1' fehlt");
    }

    if (keyword == "kind") {
      std::string kind;
      stream >> kind;
      if (kind == "sdf") {
        font.m_kind = FontKind::Sdf;
      } else if (kind == "bitmap") {
        font.m_kind = FontKind::Bitmap;
      } else {
        ThrowFontError(file.string(), lineNumber,
                       "kind muss 'bitmap' oder 'sdf' sein");
      }
    } else if (keyword == "size") {
      stream >> font.m_metrics.size;
    } else if (keyword == "ascent") {
      stream >> font.m_metrics.ascent;
    } else if (keyword == "descent") {
      stream >> font.m_metrics.descent;
    } else if (keyword == "lineHeight") {
      stream >> font.m_metrics.lineHeight;
    } else if (keyword == "spread") {
      stream >> font.m_spread;
    } else if (keyword == "atlas") {
      stream >> atlasName;
    } else if (keyword == "fallback") {
      u32 codepoint = 0;
      stream >> codepoint;
      font.m_fallback = codepoint;
    } else if (keyword == "glyph") {
      Glyph entry;
      auto atlasX = 0.0f;
      auto atlasY = 0.0f;
      auto atlasWidth = 0.0f;
      auto atlasHeight = 0.0f;
      stream >> entry.codepoint >> atlasX >> atlasY >> atlasWidth >>
          atlasHeight >> entry.bearing.x >> entry.bearing.y >> entry.advance;
      entry.atlas = Rectangle{atlasX, atlasY, atlasWidth, atlasHeight};
      if (stream.fail()) {
        ThrowFontError(
            file.string(), lineNumber,
            "glyph erwartet: codepoint x y breite hoehe bearingX bearingY "
            "vorschub");
      }
      font.m_glyphs.push_back(entry);
    } else if (keyword == "kerning") {
      u32 left = 0;
      u32 right = 0;
      auto adjustment = 0.0f;
      stream >> left >> right >> adjustment;
      if (stream.fail()) {
        ThrowFontError(file.string(), lineNumber,
                       "kerning erwartet: linke rechte anpassung");
      }
      font.m_kerning[kerningKey(left, right)] = adjustment;
    } else {
      ThrowFontError(file.string(), lineNumber,
                     "unbekannter Schlüssel '" + keyword + "'");
    }
  }

  if (!headerSeen) {
    throw FontError("Schriftdatei ohne Kopfzeile: " + file.string());
  }
  if (atlasName.empty()) {
    throw FontError("Schriftdatei ohne Atlasangabe: " + file.string());
  }

  const std::filesystem::path atlasPath = file.parent_path().empty()
                                              ? std::filesystem::path(atlasName)
                                              : file.parent_path() / atlasName;
  try {
    font.m_atlas = ImageIO::load(atlasPath);
  } catch (const ImageError &error) {
    throw FontError(std::string("Atlas konnte nicht geladen werden: ") +
                    error.what());
  }

  font.rebuildIndex();
  if (!font.isValid()) {
    throw FontError("Schrift enthält keine Glyphen: " + file.string());
  }
  SLOGD << "Schrift geladen: " << file.string() << " (" << font.m_glyphs.size()
       << " Glyphen)";
  return font;
}

// ─── Eingebettete Standardschrift ───────────────────────────────────────────

namespace {

// Rasterhöhe des Atlas: die Zeichen werden mit 64 px gerastert und beim
// Zeichnen auf die gewünschte Größe skaliert. Kleinere Größen werden dadurch
// weich verkleinert, größere leicht unscharf – für eine Bildschirmschrift ein
// guter Kompromiss aus Qualität und Speicherbedarf.
constexpr auto kDefaultRasterHeight = 64.0f;
constexpr auto kDefaultAtlasWidth = 1024;
constexpr auto kDefaultAtlasPadding = 1;
constexpr auto kDefaultAtlasMaxHeight = 4096;

std::vector<u32> DefaultCodepoints() {
  std::vector<u32> codepoints;
  codepoints.reserve(detail::DefaultFontGlyphCount);
  for (u32 codepoint = 32; codepoint <= 126; ++codepoint) {
    codepoints.push_back(codepoint);
  }
  for (u32 codepoint = 160; codepoint <= 255; ++codepoint) {
    codepoints.push_back(codepoint);
  }
  return codepoints;
}

struct RasterGlyph {
  u32 codepoint{0};
  i32 width{0};
  i32 height{0};
  i32 xOffset{0};
  i32 yOffset{0};
  f32 advance{0.0f};
  std::vector<u8> bitmap{};
  i32 x{0};
  i32 y{0};
};

} // namespace

Font Font::loadDefault() {
  stbtt_fontinfo info{};
  if (const i32 offset = stbtt_GetFontOffsetForIndex(detail::g_defaultFontData, 0);
      stbtt_InitFont(&info, detail::g_defaultFontData, offset) == 0) {
    SLOGE << "Standardschrift: eingebettete Daten konnten nicht gelesen werden";
    return Font{};
  }

  auto ascent = 0;
  auto descent = 0;
  auto lineGap = 0;
  stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);

  // FontMetrics und die Glyphenmetriken rechnen in Atlaspixeln: metrics.size
  // ist die em-Höhe des gerasterten Atlas (hier 64 px), bearing und advance
  // sind Pixel bei dieser Größe. DrawText skaliert später mit
  // pixelSize / metrics.size.
  const f32 rasterScale =
      stbtt_ScaleForPixelHeight(&info, kDefaultRasterHeight);
  if (rasterScale <= 0.0f) {
    SLOGE << "Standardschrift: ungültige Vertikalmetriken";
    return Font{};
  }

  FontMetrics metrics;
  metrics.size = kDefaultRasterHeight;
  metrics.ascent = static_cast<f32>(ascent) * rasterScale;
  metrics.descent = static_cast<f32>(-descent) * rasterScale;
  metrics.lineHeight =
      static_cast<f32>(ascent - descent + lineGap) * rasterScale;
  const std::vector<u32> codepoints = DefaultCodepoints();

  std::vector<RasterGlyph> raster;
  raster.reserve(codepoints.size());

  for (const u32 codepoint : codepoints) {
    const i32 glyphIndex =
        stbtt_FindGlyphIndex(&info, static_cast<int>(codepoint));
    if (glyphIndex == 0) {
      continue; // nicht in der Teilmenge enthalten
    }

    RasterGlyph entry;
    entry.codepoint = codepoint;

    auto advance = 0;
    auto leftSideBearing = 0;
    stbtt_GetGlyphHMetrics(&info, glyphIndex, &advance, &leftSideBearing);
    entry.advance = static_cast<f32>(advance) * rasterScale;

    auto width = 0;
    auto height = 0;
    u8 *bitmap =
        stbtt_GetGlyphBitmap(&info, rasterScale, rasterScale, glyphIndex,
                             &width, &height, &entry.xOffset, &entry.yOffset);

    entry.width = width;
    entry.height = height;
    if (bitmap != nullptr && width > 0 && height > 0) {
      entry.bitmap.assign(bitmap,
                          bitmap + static_cast<std::size_t>(width) *
                                       static_cast<std::size_t>(height));
    }
    stbtt_FreeBitmap(bitmap, info.userdata);

    raster.push_back(std::move(entry));
  }

  if (raster.empty()) {
    SLOGE << "Standardschrift: keine Glyphen gerastert";
    return Font{};
  }

  // Einfache Regalpackung: Zeilen von links nach rechts, Atlas wächst nach
  // unten.
  i32 penX = kDefaultAtlasPadding;
  i32 penY = kDefaultAtlasPadding;
  auto rowHeight = 0;

  for (RasterGlyph &entry : raster) {
    if (entry.width > kDefaultAtlasWidth - (2 * kDefaultAtlasPadding)) {
      SLOGE << "Standardschrift: Glyphe " << entry.codepoint << " ist zu breit";
      return Font{};
    }
    if (penX + entry.width + kDefaultAtlasPadding > kDefaultAtlasWidth) {
      penX = kDefaultAtlasPadding;
      penY += rowHeight + kDefaultAtlasPadding;
      rowHeight = 0;
    }
    entry.x = penX;
    entry.y = penY;
    penX += entry.width + kDefaultAtlasPadding;
    rowHeight = std::max(rowHeight, entry.height);
  }

  const i32 atlasHeight = penY + rowHeight + kDefaultAtlasPadding;
  if (atlasHeight <= 0 || atlasHeight > kDefaultAtlasMaxHeight) {
    SLOGE << "Standardschrift: Atlas passt nicht (" << kDefaultAtlasWidth << 'x'
          << atlasHeight << ')';
    return Font{};
  }

  Image atlas(kDefaultAtlasWidth, atlasHeight, ImageFormat::Gray, 0);
  for (const RasterGlyph &entry : raster) {
    for (i32 row = 0; row < entry.height; ++row) {
      const u8 *source =
          entry.bitmap.data() +
          static_cast<std::size_t>(row) * static_cast<std::size_t>(entry.width);
      u8 *target = atlas.scanline(entry.y + row) + entry.x;
      std::copy_n(source, entry.width, target);
    }
  }

  Font font = Font::fromBitmap(std::move(atlas), metrics);
  for (const RasterGlyph &entry : raster) {
    Glyph glyph;
    glyph.codepoint = entry.codepoint;
    glyph.atlas = Rectangle{
        static_cast<f32>(entry.x), static_cast<f32>(entry.y),
        static_cast<f32>(entry.width), static_cast<f32>(entry.height)};
    // bearing in Font-Einheiten, y zeigt nach oben (stbtt zählt nach unten)
    glyph.bearing =
        vec2(static_cast<f32>(entry.xOffset), -static_cast<f32>(entry.yOffset));
    glyph.advance = entry.advance;
    font.addGlyph(glyph);
  }
  font.setFallback('?');

  for (const u32 left : codepoints) {
    for (const u32 right : codepoints) {
      const auto adjustment = static_cast<f32>(stbtt_GetCodepointKernAdvance(
          &info, static_cast<i32>(left), static_cast<i32>(right)));
      ;
      if (adjustment != 0.0f) {
        font.addKerning(left, right, adjustment);
      }
    }
  }

  SLOGD << "Standardschrift geladen: " << font.glyphs().size()
       << " Glyphen, Atlas " << kDefaultAtlasWidth << 'x' << atlasHeight;
  return font;
}

Font g_defaultFont{};

// ─── UTF-8 ──────────────────────────────────────────────────────────────────

std::vector<u32> DecodeUtf8(const std::string_view text) {
  std::vector<u32> codepoints;
  codepoints.reserve(text.size());

  std::size_t index = 0;
  while (index < text.size()) {
    const auto lead = static_cast<u8>(text[index]);
    u32 value = 0;
    i32 continuationCount = 0;
    u32 minimum = 0;

    if (lead < 0x80) {
      value = lead;
    } else if ((lead & 0xE0) == 0xC0) {
      value = lead & 0x1Fu;
      continuationCount = 1;
      minimum = 0x80u;
    } else if ((lead & 0xF0) == 0xE0) {
      value = lead & 0x0Fu;
      continuationCount = 2;
      minimum = 0x800u;
    } else if ((lead & 0xF8) == 0xF0) {
      value = lead & 0x07u;
      continuationCount = 3;
      minimum = 0x10000u;
    } else {
      codepoints.push_back(kReplacementCharacter);
      ++index;
      continue;
    }

    auto valid = true;
    for (auto offset = 1; offset <= continuationCount; ++offset) {
      const std::size_t position = index + static_cast<std::size_t>(offset);
      if (position >= text.size()) {
        valid = false;
        break;
      }
      const auto continuation = static_cast<u8>(text[position]);
      if ((continuation & 0xC0) != 0x80) {
        valid = false;
        break;
      }
      value = (value << 6) | (continuation & 0x3Fu);
    }
    if (!valid || (continuationCount > 0 && value < minimum) ||
        value > 0x10FFFFu || (value >= 0xD800u && value <= 0xDFFFu)) {
      codepoints.push_back(kReplacementCharacter);
      ++index;
      continue;
    }
    codepoints.push_back(value);
    index += static_cast<std::size_t>(continuationCount) + 1;
  }
  return codepoints;
}

std::string EncodeUtf8(u32 codepoint) {
  std::string text;
  if (codepoint > 0x10FFFFu || (codepoint >= 0xD800u && codepoint <= 0xDFFFu)) {
    codepoint = kReplacementCharacter;
  }
  if (codepoint < 0x80u) {
    text.push_back(static_cast<char>(codepoint));
  } else if (codepoint < 0x800u) {
    text.push_back(static_cast<char>(0xC0u | (codepoint >> 6)));
    text.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
  } else if (codepoint < 0x10000u) {
    text.push_back(static_cast<char>(0xE0u | (codepoint >> 12)));
    text.push_back(static_cast<char>(0x80u | ((codepoint >> 6) & 0x3Fu)));
    text.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
  } else {
    text.push_back(static_cast<char>(0xF0u | (codepoint >> 18)));
    text.push_back(static_cast<char>(0x80u | ((codepoint >> 12) & 0x3Fu)));
    text.push_back(static_cast<char>(0x80u | ((codepoint >> 6) & 0x3Fu)));
    text.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
  }
  return text;
}

} // namespace sgl
