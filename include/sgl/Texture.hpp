#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// Texture2d und CubeMap über Direct State Access. Öffentliche Schnittstelle dieses Headers:
//
//   TextureError, Texture2d, fromImage, fromMemory, unbind
//   ScopedBind, upload, updateRegion, generateMipmaps, setWrap/setFilter,
//   wrapS, wrapT, minFilter, magFilter, mipmaps, anisotropy, borderColor,
//   setAnisotropy, setBorderColor, readBack, applyParameters
//   release, CubeMap, fromImages, fromCrossLayout, uploadFace
//   setSeamless, faceImage, InternalFormatFor, PixelFormatFor
//
// Beispiel:
//
//   Texture2d tex = Texture2d::fromImage(image, TextureOptions{.anisotropy = 8.0f});
//   tex.use(0);
//   CubeMap sky = CubeMap::fromCrossLayout(cross);
//   sky.use(1);
// ─────────────────────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────────────────────
// sgl · Texture
//
// GL-Texturen als RAII-Klassen: Texture2d und CubeMap.
//
//   * Erzeugen aus einem Image (Gray, GrayAlpha, Rgb, Rgba) oder aus rohen
//     Pixeln; das Bildformat bestimmt das interne GL-Format, die Mipmap-Ebenen
//     werden von Anfang an mitgespeichert (unveränderlicher Speicher)
//   * Wrap-Modi (Repeat, MirroredRepeat, ClampToEdge, ClampToBorder mit
//     Randfarbe), Min-/Mag-Filter, Mipmaps, Anisotropie, sRGB
//   * Binden mit Textureinheit, ScopedBind stellt Bindung und aktive Einheit
//     wieder her
//   * Nachladen: upload() für die ganze Ebene, updateRegion() für einen
//     Teilbereich, generateMipmaps()
//   * CubeMap aus sechs Einzelbildern oder aus einem Kreuz-Layout
//     (3x4-Raster), Face-Reihenfolge wie in GL (+X, -X, +Y, -Y, +Z, -Z);
//     nahtlose Filterung an den Kanten ist voreingestellt
//   * readBack()/faceImage() lesen den Inhalt zurück (Tests, Screenshots)
//   * Residency (bindless) für Treiber mit GL_ARB_bindless_texture: Handle
//     erzeugen, resident machen, im Shader als sampler2D/samplerCube nutzen
//
// Alle Aufrufe brauchen einen aktuellen GL-Kontext (OpenGL 4.5 wegen DSA).
// Die Klasse arbeitet durchgehend mit Direct State Access: glCreateTextures,
// glTextureStorage2D, glTextureSubImage2D/3D, glTextureParameteri/fv,
// glGenerateTextureMipmap, glGetTextureSubImage. Es wird also nie eine Textur
// gebunden, um sie zu beschreiben oder auszulesen; glBindTextureUnit erscheint
// nur in use()/ScopedBind() – das ist Sampler-Zustand fürs Zeichnen.
// RAII: der Texturname wird im Destruktor gelöscht, Kopieren ist verboten,
// Verschieben erlaubt.
//
// Konventionen: Methoden camelCase, freie Funktionen mit Großbuchstaben,
// bool-Getter mit "is", Member "m_".
//
// Beispiel:
//
//   const Image bild = ImageIO::load("assets/tex/dirt.png");
//   Texture2d boden = Texture2d::fromImage(bild, TextureOptions{.mipmaps = MipmapMode::Linear});
//   boden.use(0);                       // an Einheit 0 binden
//   shader.setTexture("uAlbedo", boden.id(), 0);
//
//   const CubeMap himmel = CubeMap::fromCrossLayout(ImageIO::load("sky_cross.png"));
//   {
//       const CubeMap::ScopedBind bind(himmel, 3);   // stellt danach alles zurück
//       ...
//   }
// ─────────────────────────────────────────────────────────────────────────────

#include "Types.hpp"
#include "GL.hpp"
#include "Image.hpp"

#include <array>
#include <span>
#include <stdexcept>
#include <string>

namespace sgl {

// ─── Fehler ──────────────────────────────────────────────────────────────────

class TextureError : public std::runtime_error {
public:
    explicit TextureError(const std::string& message) : std::runtime_error(message) {}
};

// ─── Parameter ──────────────────────────────────────────────────────────────

enum class TextureWrap { Repeat, MirroredRepeat, ClampToEdge, ClampToBorder };
enum class TextureFilter { Nearest, Linear };
enum class MipmapMode { None, Nearest, Linear };

struct TextureOptions {
    TextureWrap wrapS{TextureWrap::Repeat};
    TextureWrap wrapT{TextureWrap::Repeat};
    TextureWrap wrapR{TextureWrap::Repeat};  // nur für CubeMap
    TextureFilter minFilter{TextureFilter::Linear};
    TextureFilter magFilter{TextureFilter::Linear};
    MipmapMode mipmaps{MipmapMode::Linear};
    // sRGB: Farbkanäle werden beim Lesen in der Textur linearisiert.
    // Graustufen haben in GL kein sRGB-Format und bleiben linear.
    bool isSrgb{false};
    // HDR: RGBA16F als internes Format (Werte über 1.0 bleiben erhalten).
    // Nur für RGBA-Ziele sinnvoll; readBack() liefert dann keine 8-Bit-Werte.
    bool isHdr{false};
    // 0 = aus; ansonsten Wunschwert, wird auf GL_MAX_TEXTURE_MAX_ANISOTROPY geklemmt
    f32 anisotropy{0.0f};
    // Nahtlose Filterung an den Würfelkanten (nur CubeMap). Wird als
    // Textureigenschaft gesetzt (GL_TEXTURE_CUBE_MAP_SEAMLESS, GL 4.5), nicht als
    // globaler Zustand – andere Texturen bleiben davon unberührt.
    bool isSeamless{true};
    std::array<u8, 4> borderColor{0, 0, 0, 0};  // nur bei ClampToBorder
};

// ─── 2D-Textur ──────────────────────────────────────────────────────────────

class Texture2d {
public:
    Texture2d() = default;  // ungültig, ohne GL-Objekt
    ~Texture2d();

    Texture2d(Texture2d&& other) noexcept;
    Texture2d& operator=(Texture2d&& other) noexcept;
    Texture2d(const Texture2d&) = delete;
    Texture2d& operator=(const Texture2d&) = delete;

    // Bild hochladen (Ebene 0); Mipmaps werden bei Bedarf erzeugt.
    [[nodiscard]] static Texture2d fromImage(const Image& image, const TextureOptions& options = {});
    // Rohdaten; die Puffergröße muss exakt zu Breite*Höhe*Kanälen passen.
    // Leere Textur mit unveränderlichem Speicher – als Renderziel eines
    // Framebuffers, ohne dass Bilddaten nötig wären.
    [[nodiscard]] static Texture2d createStorage(i32 width, i32 height, ImageFormat format,
                                                const TextureOptions& options = {});
    [[nodiscard]] static Texture2d fromMemory(i32 width, i32 height, ImageFormat format,
                                std::span<const u8> pixels,
                                const TextureOptions& options = {});

    // ── Zustand ──────────────────────────────────────────────────────────────
    [[nodiscard]] bool isValid() const noexcept { return m_id != 0; }
    [[nodiscard]] GLuint id() const noexcept { return m_id; }
    [[nodiscard]] i32 width() const noexcept { return m_width; }
    [[nodiscard]] i32 height() const noexcept { return m_height; }
    [[nodiscard]] ImageFormat format() const noexcept { return m_format; }
    [[nodiscard]] const TextureOptions& options() const noexcept { return m_options; }
    [[nodiscard]] TextureWrap wrapS() const noexcept { return m_options.wrapS; }
    [[nodiscard]] TextureWrap wrapT() const noexcept { return m_options.wrapT; }
    [[nodiscard]] TextureFilter minFilter() const noexcept { return m_options.minFilter; }
    [[nodiscard]] TextureFilter magFilter() const noexcept { return m_options.magFilter; }
    [[nodiscard]] MipmapMode mipmaps() const noexcept { return m_options.mipmaps; }
    [[nodiscard]] f32 anisotropy() const noexcept { return m_options.anisotropy; }
    [[nodiscard]] std::array<u8, 4> borderColor() const noexcept { return m_options.borderColor; }
    [[nodiscard]] bool hasMipmaps() const noexcept { return m_options.mipmaps != MipmapMode::None; }
    [[nodiscard]] bool isSrgb() const noexcept { return m_options.isSrgb; }

    // ── Binden ───────────────────────────────────────────────────────────────
    void use(i32 unit = 0) const;
    static void unbind(i32 unit = 0);

    // Bindet auf einer Einheit für die Lebenszeit des Objekts und stellt danach
    // die vorher gebundene Textur und die aktive Einheit wieder her.
    class ScopedBind {
    public:
        explicit ScopedBind(const Texture2d& texture, i32 unit = 0);
        ~ScopedBind();
        ScopedBind(const ScopedBind&) = delete;
        ScopedBind& operator=(const ScopedBind&) = delete;

    private:
        i32 m_unit{0};
        GLint m_previousTexture{0};
        GLint m_previousUnit{GL_TEXTURE0};
    };

    // ── Daten nachladen ──────────────────────────────────────────────────────
    // Ersetzt die Ebene 0; Format und Maße dürfen sich ändern.
    void upload(const Image& image);
    // Schreibt einen Teilbereich (Bildkoordinaten, Pixel zählen).
    void updateRegion(const Rectangle& region, const Image& image);
    void generateMipmaps();

    // ── Parameter ändern ────────────────────────────────────────────────────
    void setWrap(TextureWrap s, TextureWrap t);
    void setFilter(TextureFilter minFilter, TextureFilter magFilter, MipmapMode mipmaps);
    void setAnisotropy(f32 maxAnisotropy);
    void setBorderColor(std::array<u8, 4> color);

    // Inhalt zurücklesen (Ebene 0). Bei sRGB-Texturen liefert GL linearisierte
    // Werte; für Tests besser ohne sRGB arbeiten.
    [[nodiscard]] Image readBack() const;

    // ── Residency (bindless, GL_ARB_bindless_texture) ────────────────────────
    // Ohne die Erweiterung melden diese Funktionen false bzw. werfen eine
    // TextureError. Voraussetzung ist unveränderlicher Speicher (haben beide
    // Klassen immer). Ein residenter Texturhandle muss vor dem Zerstören der
    // Textur freigegeben werden — der Destruktor erledigt das selbst.
    // Im Shader: Handle als uvec2 übergeben und sampler2D(uHandle) bauen;
    // glUniformHandleui64ARB auf ein sampler2D lehnt Mesa 25.2.8 ab.
    [[nodiscard]] static bool isResidencySupported();
    // 64-Bit-Handle der Textur (wird einmal erzeugt und gemerkt)
    [[nodiscard]] u64 textureHandle() const;
    [[nodiscard]] bool makeResident();
    [[nodiscard]] bool makeNonResident();
    [[nodiscard]] bool isResident() const;

private:
    void applyParameters();
    void release() noexcept;

    GLuint m_id{0};
    i32 m_width{0};
    i32 m_height{0};
    ImageFormat m_format{ImageFormat::Rgba};
    TextureOptions m_options{};
    mutable u64 m_handle{0};          // bindless-Handle, 0 = noch nicht erzeugt
    bool m_isResident{false};
};

// ─── CubeMap ────────────────────────────────────────────────────────────────

enum class CubeFace : u32 {
    PositiveX = 0,
    NegativeX = 1,
    PositiveY = 2,
    NegativeY = 3,
    PositiveZ = 4,
    NegativeZ = 5
};

class CubeMap {
public:
    CubeMap() = default;
    ~CubeMap();

    CubeMap(CubeMap&& other) noexcept;
    CubeMap& operator=(CubeMap&& other) noexcept;
    CubeMap(const CubeMap&) = delete;
    CubeMap& operator=(const CubeMap&) = delete;

    // Sechs Gesichter in GL-Reihenfolge; gleiche Maße und gleiches Format.
    [[nodiscard]] static CubeMap fromImages(const std::array<Image, 6>& faces,
                              const TextureOptions& options = {});
    // Kreuz-Layout 3x4 oder 4x3 (je Zelle ein Quadrat). Erkannt werden die
    // üblichen Anordnungen: waagerecht (+X, -X, +Y, -Y) mit +Z/-Z darüber oder
    // darunter.
    [[nodiscard]] static CubeMap fromCrossLayout(const Image& cross, const TextureOptions& options = {});

    // ── Zustand ──────────────────────────────────────────────────────────────
    [[nodiscard]] bool isValid() const noexcept { return m_id != 0; }
    [[nodiscard]] GLuint id() const noexcept { return m_id; }
    [[nodiscard]] u32 size() const noexcept { return m_size; }
    [[nodiscard]] ImageFormat format() const noexcept { return m_format; }
    [[nodiscard]] const TextureOptions& options() const noexcept { return m_options; }
    [[nodiscard]] TextureWrap wrapS() const noexcept { return m_options.wrapS; }
    [[nodiscard]] TextureWrap wrapT() const noexcept { return m_options.wrapT; }
    [[nodiscard]] TextureWrap wrapR() const noexcept { return m_options.wrapR; }
    [[nodiscard]] TextureFilter minFilter() const noexcept { return m_options.minFilter; }
    [[nodiscard]] TextureFilter magFilter() const noexcept { return m_options.magFilter; }
    [[nodiscard]] MipmapMode mipmaps() const noexcept { return m_options.mipmaps; }
    [[nodiscard]] f32 anisotropy() const noexcept { return m_options.anisotropy; }
    [[nodiscard]] std::array<u8, 4> borderColor() const noexcept { return m_options.borderColor; }
    [[nodiscard]] bool hasMipmaps() const noexcept { return m_options.mipmaps != MipmapMode::None; }
    [[nodiscard]] bool isSrgb() const noexcept { return m_options.isSrgb; }
    // nahtlose Filterung an den Kanten (siehe TextureOptions::isSeamless)
    [[nodiscard]] bool isSeamless() const noexcept { return m_options.isSeamless; }

    // ── Binden ───────────────────────────────────────────────────────────────
    void use(i32 unit = 0) const;
    static void unbind(i32 unit = 0);

    class ScopedBind {
    public:
        explicit ScopedBind(const CubeMap& cubeMap, i32 unit = 0);
        ~ScopedBind();
        ScopedBind(const ScopedBind&) = delete;
        ScopedBind& operator=(const ScopedBind&) = delete;

    private:
        i32 m_unit{0};
        GLint m_previousTexture{0};
        GLint m_previousUnit{GL_TEXTURE0};
    };

    // ── Daten ────────────────────────────────────────────────────────────────
    // Ersetzt ein Gesicht (Maße und Format müssen passen).
    void uploadFace(CubeFace face, const Image& image);
    void generateMipmaps();
    void setWrap(TextureWrap s, TextureWrap t, TextureWrap r);
    void setFilter(TextureFilter minFilter, TextureFilter magFilter, MipmapMode mipmaps);
    void setAnisotropy(f32 maxAnisotropy);
    // blendet die Würfelkanten ineinander, statt harte Nähte zu zeigen
    void setSeamless(bool seamless);

    // Gesicht zurücklesen (Ebene 0).
    [[nodiscard]] Image faceImage(CubeFace face) const;

    // ── Residency (bindless, siehe Texture2d) ────────────────────────────────
    [[nodiscard]] static bool isResidencySupported();
    [[nodiscard]] u64 textureHandle() const;
    [[nodiscard]] bool makeResident();
    [[nodiscard]] bool makeNonResident();
    [[nodiscard]] bool isResident() const;

private:
    void applyParameters();
    void release() noexcept;

    GLuint m_id{0};
    u32 m_size{0};
    ImageFormat m_format{ImageFormat::Rgba};
    TextureOptions m_options{};
    mutable u64 m_handle{0};
    bool m_isResident{false};
};

// ─── Freie Helfer ───────────────────────────────────────────────────────────

// GL-Format zu einem Bildformat (für Diagnose und Tests).
[[nodiscard]] GLenum InternalFormatFor(ImageFormat format, bool isSrgb);
[[nodiscard]] GLenum PixelFormatFor(ImageFormat format);

}  // namespace sgl
