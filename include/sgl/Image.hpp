#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// Bildpuffer mit Transformationen sowie separater Datei-Ein-/-Ausgabe.
//
//   ImageError, ChannelCount, HasAlpha, ToString, FileExtension
//   FileFormatFromPath, Image, ImageIO
//   Image: getScanline, getChannel, setChannel, getPixel, fill, flip..., rotate..., scale...
//   ImageIO: Load, LoadFromMemory, Save, Encode, SaveToMemory sowie die
//            nicht werfenden Varianten TryLoad, TryLoadFromMemory, TrySave,
//            TryEncode und TrySaveToMemory
//
// Beispiel:
//
//   Image img = ImageIO::Load("pfad.png");
//   img.convertTo(ImageFormat::Rgba);
//   Image half = img.scaled(0.5, Interpolation::Bilinear);
//   ImageIO::Save(img, "out.png");
// ─────────────────────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────────────────────
// sgl · Image
//
// 2D-Bild im Hauptspeicher: Laden, Speichern, Spiegeln, Drehen, Skalieren.
//
//   * Laden über stb_image (PNG/JPG/BMP/TGA/…), Speichern über stb_image_write
//     (PNG/JPG/BMP/TGA); das Dateiformat ergibt sich aus der Endung
//   * Laden und Speichern auch direkt aus/in den Speicher (loadFromMemory,
//     encode) – praktisch für Tests und eingebettete Assets
//   * Formate: Graustufen, Graustufen+Alpha, RGB, RGBA (8 Bit je Kanal)
//   * Spiegeln waagerecht/senkrecht – pixelgenau, kein Resampling
//   * Drehen: exakte Vierteldrehungen (90/180/270, verlustfrei) oder beliebiger
//     Winkel mit Interpolation und optional erweitertem Canvas
//   * Skalieren mit Nearest, Linear, Bilinear (flächenkorrekt) und Bicubic
//   * Filterung in prämultipliziertem Alpha, damit transparente Pixel keine
//     dunklen Säume erzeugen
//   * RAII: Pixelpuffer gehört dem Objekt (std::vector), Kopie = tiefe Kopie,
//     kein GL-Objekt, keine manuellen free-Aufrufe
//   * Fehler als ImageError (Datei fehlt, Format unbekannt, Größe 0, …)
//
// Konventionen des Projekts: camelCase, bool-Getter mit "is", Member "m_".
//
// Beispiel:
//
//   sgl::Image image = sgl::ImageIO::Load("assets/tex/dirt.png");
//   sgl::ImageIO::Save(image, "out/dirt_klein.png");
//
//   const sgl::Image klein   = image.scaled(256, 256, sgl::Interpolation::Bicubic);
//   const sgl::Image gedreht = image.rotated(90);                 // exakt
//   const sgl::Image schief  = image.rotated(17, {sgl::Interpolation::Bilinear});
//   const sgl::Image spiegel = image.flippedHorizontally();
//
//   image.flipVertically();   // direkt im Objekt
// ─────────────────────────────────────────────────────────────────────────────

#include "Types.hpp"

#include <array>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace sgl {

// ─── Fehler ──────────────────────────────────────────────────────────────────

class ImageError : public std::runtime_error {
public:
    explicit ImageError(const std::string& message) : std::runtime_error(message) {}
};

// ─── Formate ─────────────────────────────────────────────────────────────────

// Der Zahlenwert ist zugleich die Kanalzahl.
enum class ImageFormat : u8 { Gray = 1, GrayAlpha = 2, Rgb = 3, Rgba = 4 };

[[nodiscard]] constexpr i32 ChannelCount(ImageFormat format) noexcept {
    return static_cast<i32>(format);
}
[[nodiscard]] constexpr bool HasAlpha(ImageFormat format) noexcept {
    return format == ImageFormat::GrayAlpha || format == ImageFormat::Rgba;
}

// "gray", "grayalpha", "rgb", "rgba"
std::string_view ToString(ImageFormat format);

// ─── Interpolation ──────────────────────────────────────────────────────────

// Nearest  : nächster Texel, keine Mischung (Pixel-Art, harte Kanten)
// Linear   : bilineares 2×2-Tent um die Quellposition (schnell, scharf)
// Bilinear : dasselbe Tent, aber beim Verkleinern auf die Quellfläche
//            aufgeweitet – mittelt statt zu aliasen (Qualitätsstufe)
// Bicubic  : Catmull-Rom über 4×4 Texel, beim Verkleinern ebenfalls
//            aufgeweitet; lineare Verläufe bleiben exakt
enum class Interpolation { Nearest, Linear, Bilinear, Bicubic };

std::string_view ToString(Interpolation interpolation);

// ─── Dateiformate ───────────────────────────────────────────────────────────

enum class ImageFileFormat { Png, Jpeg, Bmp, Tga };

std::string_view ToString(ImageFileFormat format);
// "png", "jpg", "bmp", "tga"
std::string_view FileExtension(ImageFileFormat format);
// Endung der Datei, sonst ImageError
[[nodiscard]] ImageFileFormat FileFormatFromPath(const std::filesystem::path& file);

// ─── Optionen ───────────────────────────────────────────────────────────────

struct LoadOptions {
    // Zielformat; ohne Angabe bleibt die Kanalzahl der Datei erhalten
    std::optional<ImageFormat> forceFormat{};
    // stb_image liefert die erste Zeile oben – für GL-Texturen oft unerwünscht
    bool flipVertically{false};
};

struct SaveOptions {
    // Nur für JPEG ausgewertet (1..100)
    i32 jpegQuality{90};
};

struct RotateOptions {
    Interpolation interpolation{Interpolation::Bilinear};
    // Ausgabegröße so wählen, dass das gedrehte Rechteck vollständig hineinpasst
    bool expandCanvas{true};
    // Hintergrund für Flächen außerhalb des Quellbildes (RGBA, auf das
    // Zielformat abgebildet)
    std::array<u8, 4> fill{0, 0, 0, 0};
    bool premultiplyAlpha{true};
};

// ─── Bild ────────────────────────────────────────────────────────────────────

class Image {
public:
    Image() = default;

    // Wirft ImageError bei Breite/Höhe <= 0.
    Image(i32 width, i32 height, ImageFormat format, u8 fill = 0);
    // Übernimmt eine Kopie der Pixel; die Puffergröße muss exakt passen.
    Image(i32 width, i32 height, ImageFormat format, std::span<const u8> pixels);

    // ── Zustand ──────────────────────────────────────────────────────────────
    [[nodiscard]] bool isEmpty() const noexcept { return m_pixels.empty(); }
    [[nodiscard]] bool isPowerOfTwo() const noexcept;
    [[nodiscard]] i32 getWidth() const noexcept { return m_width; }
    [[nodiscard]] i32 getHeight() const noexcept { return m_height; }
    [[nodiscard]] ImageFormat getFormat() const noexcept { return m_format; }
    [[nodiscard]] i32 getChannelCount() const noexcept { return sgl::ChannelCount(m_format); }
    [[nodiscard]] bool hasAlpha() const noexcept { return sgl::HasAlpha(m_format); }
    [[nodiscard]] std::size_t getByteSize() const noexcept { return m_pixels.size(); }
    [[nodiscard]] std::size_t getRowStride() const noexcept {
        return static_cast<std::size_t>(m_width) * static_cast<std::size_t>(getChannelCount());
    }

    u8* getData() noexcept { return m_pixels.data(); }
    [[nodiscard]] const u8* getData() const noexcept { return m_pixels.data(); }
    // Zeiger auf den Zeilenanfang; Zeile 0 ist oben.
    u8* getScanline(i32 y);
    [[nodiscard]] const u8* getScanline(i32 y) const;

    // Geprüfte Zugriffe (werfen ImageError außerhalb des Bildes).
    [[nodiscard]] u8 getChannel(i32 x, i32 y, i32 channel) const;
    void setChannel(i32 x, i32 y, i32 channel, u8 value);
    // Kanäle als {R,G,B,A} bzw. {Grau,0,0,0}; fehlende Kanäle sind 0.
    [[nodiscard]] std::array<u8, 4> getPixel(i32 x, i32 y) const;
    void fill(std::array<u8, 4> color);
    void fill(u8 value);

    // ── Spiegeln (exakt, ohne Resampling) ────────────────────────────────────
    [[nodiscard]] Image flippedHorizontally() const;
    [[nodiscard]] Image flippedVertically() const;
    void flipHorizontally();
    void flipVertically();

    // ── Drehen ───────────────────────────────────────────────────────────────
    // Exakt für 90/180/270 (verlustfrei), sonst interpoliert. Der Winkel läuft
    // im Uhrzeigersinn; negative Werte drehen gegen den Uhrzeigersinn.
    [[nodiscard]] Image rotated(i32 clockwiseDegrees, const RotateOptions& options = {}) const;
    void rotate90Clockwise();
    void rotate180();
    void rotate270Clockwise();

    // ── Skalieren ────────────────────────────────────────────────────────────
    [[nodiscard]] Image scaled(i32 newWidth, i32 newHeight,
                Interpolation interpolation = Interpolation::Bilinear) const;
    [[nodiscard]] Image scaled(f64 factor, Interpolation interpolation = Interpolation::Bilinear) const;
    // Behält das Seitenverhältnis, keine Vergrößerung über maxWidth/maxHeight.
    [[nodiscard]] Image scaledToFit(i32 maxWidth, i32 maxHeight,
                      Interpolation interpolation = Interpolation::Bilinear) const;
    void resize(i32 newWidth, i32 newHeight,
                Interpolation interpolation = Interpolation::Bilinear);

    // ── Ausschneiden / Umwandeln ─────────────────────────────────────────────
    [[nodiscard]] Image cropped(i32 x, i32 y, i32 width, i32 height) const;
    [[nodiscard]] Image converted(ImageFormat format) const;
    void convertTo(ImageFormat format);

    // Tauscht Inhalt und Metadaten (Kopie-und-Tausch, damit Ausnahmen sicher sind).
    void swap(Image& other) noexcept;

private:
    [[nodiscard]] u8* pixel(i32 x, i32 y);
    [[nodiscard]] const u8* pixel(i32 x, i32 y) const;

    i32 m_width{0};
    i32 m_height{0};
    ImageFormat m_format{ImageFormat::Rgba};
    std::vector<u8> m_pixels{};
};

// ─── Datei-Ein- und -Ausgabe ────────────────────────────────────────────────

class ImageIO {
public:
    /// Lädt ein Bild aus einer Datei und wirft bei Fehlern ImageError.
    [[nodiscard]] static Image Load(const std::filesystem::path& file,
                                    const LoadOptions& options = {});
    /// Dekodiert ein Bild aus einem Speicherpuffer und wirft bei Fehlern
    /// ImageError.
    [[nodiscard]] static Image LoadFromMemory(std::span<const u8> data,
                                              const LoadOptions& options = {});
    /// Speichert ein Bild; Zielformat und Elternverzeichnisse werden abgeleitet
    /// bzw. angelegt. Wirft bei Fehlern ImageError.
    static void Save(const Image& image, const std::filesystem::path& file,
                     const SaveOptions& options = {});
    /// Kodiert ein Bild in einen Speicherpuffer und wirft bei Fehlern
    /// ImageError.
    [[nodiscard]] static std::vector<u8> Encode(const Image& image, ImageFileFormat format,
                                                const SaveOptions& options = {});
    /// Wie Encode(), nur als sprechender Name für das Kodieren in den Speicher.
    [[nodiscard]] static std::vector<u8> SaveToMemory(const Image& image, ImageFileFormat format,
                                                      const SaveOptions& options = {}) {
        return Encode(image, format, options);
    }

    /// Nicht werfende Variante von Load().
    [[nodiscard]] static std::expected<Image, ImageError> TryLoad(
        const std::filesystem::path& file, const LoadOptions& options = {});
    /// Nicht werfende Variante von LoadFromMemory().
    [[nodiscard]] static std::expected<Image, ImageError> TryLoadFromMemory(
        std::span<const u8> data, const LoadOptions& options = {});
    /// Nicht werfende Variante von Save().
    [[nodiscard]] static std::expected<void, ImageError> TrySave(
        const Image& image, const std::filesystem::path& file,
        const SaveOptions& options = {});
    /// Nicht werfende Variante von Encode().
    [[nodiscard]] static std::expected<std::vector<u8>, ImageError> TryEncode(
        const Image& image, ImageFileFormat format, const SaveOptions& options = {});
    /// Nicht werfende Variante von SaveToMemory().
    [[nodiscard]] static std::expected<std::vector<u8>, ImageError> TrySaveToMemory(
        const Image& image, ImageFileFormat format, const SaveOptions& options = {}) {
        return TryEncode(image, format, options);
    }
};

void Swap(Image& first, Image& second) noexcept;

}  // namespace sgl
