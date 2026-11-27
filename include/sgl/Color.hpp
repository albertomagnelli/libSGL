#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// Farbmodelle (RGB/HSV/HSL/HWB/CMYK) und Farboperationen. Öffentliche Schnittstelle dieses Headers:
//
//   ColorError, Color, FromVec4, FromHsv, FromHsl
//   FromHwb, FromCmyk, FromHex, FromGray, FromGray8
//   FromPackedRgba, FromLinear, ToHsv, ToHsl, ToHwb
//   ToCmyk, ToHex, ToPackedRgba, ToLinear, Luminance
//   PerceivedLuminance, Luminance8, WithAlpha, Mix, Lighten
//   Darken, Saturate, Desaturate, RotateHue, Invert
//   ToGrayscale, Premultiply, Unpremultiply, IsOpaque, IsTransparent
//   IsGray, IsNearlyEqual
//
// Beispiel:
//
//   Color c = FromHsv({210.0f, 0.65f, 0.9f});
//   Color m = Mix(RED, BLUE, 0.5f);
//   u32 packed = ToPackedRgba(c);
// ─────────────────────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────────────────────
// sgl · Color
//
// Farbmodelle und Rechenfunktionen auf dem Bibliothekstyp Color (8 Bit je
// Kanal, definiert in dieser Datei – dieselben Werte wie Textur und Bild).
//
//   * Erzeugen aus HSV, HSL, HWB, CMYK, Hex-Text, Grauwert, gepacktem
//     0xRRGGBBAA und aus linearem Licht
//   * Ausgabe in all diese Räume zurück (ToHsv/ToHsl/ToHwb/ToCmyk/ToHex/…)
//   * Helligkeit: Luminance (Rec. 709 auf linearem Licht) und
//     PerceivedLuminance (Rec. 601 auf den gespeicherten sRGB-Werten)
//   * Ableitungen als neue Farbe: Mix, Lighten, Darken, Saturate, Desaturate,
//     RotateHue, Invert, ToGrayscale, Premultiply, Unpremultiply, WithAlpha
//   * Zustandsabfragen: IsOpaque, IsTransparent, IsGray, IsNearlyEqual
//
// Alle Funktionen sind frei und beginnen mit einem Großbuchstaben; Color
// selbst (u8 r,g,b,a) steht oben in dieser Datei.
//
// Beispiel:
//
//   const Color orange = sgl::FromHex("#ff8800");
//   const Color weich  = sgl::Mix(orange, WHITE, 0.25f);
//   const Hsv   hsv    = sgl::ToHsv(orange);            // {h: 32, s: 1, v: 1}
//   bild.fill(sgl::Darken(orange, 0.3f));
// ─────────────────────────────────────────────────────────────────────────────

#include "Types.hpp"

#include <cmath>
#include <stdexcept>
#include <string>
#include <string_view>

namespace sgl {

// ─── Fehler ──────────────────────────────────────────────────────────────────

class ColorError : public std::runtime_error {
public:
    explicit ColorError(const std::string& message) : std::runtime_error(message) {}
};

// ─── Farbmodelle ─────────────────────────────────────────────────────────────

// Farbton 0..360 (Grad), Sättigung/Wert 0..1
struct Hsv {
    f32 h{0.0f};
    f32 s{0.0f};
    f32 v{0.0f};
};

// Farbton 0..360, Sättigung 0..1, Helligkeit 0..1 (0.5 = reines Grau)
struct Hsl {
    f32 h{0.0f};
    f32 s{0.0f};
    f32 l{0.0f};
};

// Anteil Weiß/Schwarz 0..1; w + b <= 1
struct Hwb {
    f32 h{0.0f};
    f32 w{0.0f};
    f32 b{0.0f};
};

// Druckfarben 0..1; k ist der Schwarzanteil
struct Cmyk {
    f32 c{0.0f};
    f32 m{0.0f};
    f32 y{0.0f};
    f32 k{0.0f};
};

    // Farbe mit 8 Bit je Kanal (wie in raylib)
    struct Color {
        u8 r{255};
        u8 g{255};
        u8 b{255};
        u8 a{255};

        constexpr Color() = default;
        constexpr Color(const u8 red, const u8 green, const u8 blue, const u8 alpha = 255)
            : r(red), g(green), b(blue), a(alpha) {}

        // Kanäle als 0..1-Float (sRGB-Werte, wie die Textur sie liefert)
        [[nodiscard]] vec4 toVec4() const {
            return {static_cast<f32>(r) / 255.0f, static_cast<f32>(g) / 255.0f, static_cast<f32>(b) / 255.0f,
                    static_cast<f32>(a) / 255.0f};
        }

        [[nodiscard]] static Color fromVec4(const vec4 &color) {
            const auto toByte = [](const f32 value) -> u8 {
                const f32 clamped = (value < 0.0f) ? 0.0f : ((value > 1.0f) ? 1.0f : value);

                return static_cast<u8>(std::lround(clamped * 255.0f));
            };

            return Color{toByte(color.r), toByte(color.g), toByte(color.b), toByte(color.a)};
        }

        bool operator==(const Color &other) const {
            return (r == other.r) && (g == other.g) && (b == other.b) && (a == other.a);
        }
    };

    // gebraeuchliche Farben
    inline constexpr Color WHITE{255, 255, 255, 255};
    inline constexpr Color BLACK{0, 0, 0, 255};
    inline constexpr Color RED{230, 41, 55, 255};
    inline constexpr Color GREEN{0, 228, 48, 255};
    inline constexpr Color BLUE{0, 121, 241, 255};
    inline constexpr Color YELLOW{253, 249, 0, 255};
    inline constexpr Color ORANGE{255, 161, 0, 255};
    inline constexpr Color MAGENTA{255, 0, 255, 255};
    inline constexpr Color GRAY{130, 130, 130, 255};
    inline constexpr Color LIGHTGRAY{200, 200, 200, 255};
    inline constexpr Color DARKGRAY{80, 80, 80, 255};
    inline constexpr Color BLANK{0, 0, 0, 0};
// ─── Erzeugen ───────────────────────────────────────────────────────────────

[[nodiscard]] Color FromHsv(const Hsv& hsv, u8 alpha = 255);
[[nodiscard]] Color FromHsl(const Hsl& hsl, u8 alpha = 255);
[[nodiscard]] Color FromHwb(const Hwb& hwb, u8 alpha = 255);
[[nodiscard]] Color FromCmyk(const Cmyk& cmyk, u8 alpha = 255);
// "#rgb", "#rgba", "#rrggbb", "#rrggbbaa" – '#' ist optional
[[nodiscard]] Color FromHex(std::string_view hex);
// 0..1 als Grauwert
[[nodiscard]] Color FromGray(f32 value, u8 alpha = 255);
[[nodiscard]] Color FromGray8(u8 value);
// 0xRRGGBBAA
[[nodiscard]] Color FromPackedRgba(u32 packed);
// RGBA als 0..1 (wie Color::toVec4)
[[nodiscard]] Color FromVec4(const vec4& rgba);
// lineares Licht (0..1 je Kanal) zurück nach sRGB
[[nodiscard]] Color FromLinear(const vec3& linear, u8 alpha = 255);

// ─── Ausgeben ───────────────────────────────────────────────────────────────

[[nodiscard]] Hsv ToHsv(const Color& color);
[[nodiscard]] Hsl ToHsl(const Color& color);
[[nodiscard]] Hwb ToHwb(const Color& color);
[[nodiscard]] Cmyk ToCmyk(const Color& color);
std::string ToHex(const Color& color, bool withAlpha = false);
[[nodiscard]] u32 ToPackedRgba(const Color& color);
// sRGB-Kanäle als lineares Licht (für Beleuchtung/Mischen)
[[nodiscard]] vec3 ToLinear(const Color& color);

// ─── Helligkeit ─────────────────────────────────────────────────────────────

// Rec. 709 auf linearem Licht (0..1)
[[nodiscard]] f32 Luminance(const Color& color);
// Rec. 601 auf den gespeicherten Werten – das, was Bildwerkzeuge anzeigen
[[nodiscard]] f32 PerceivedLuminance(const Color& color);
[[nodiscard]] u8 Luminance8(const Color& color);

// ─── Ableitungen ────────────────────────────────────────────────────────────

[[nodiscard]] Color WithAlpha(const Color& color, u8 alpha);
// t = 0 -> from, t = 1 -> to
[[nodiscard]] Color Mix(const Color& from, const Color& to, f32 t);
[[nodiscard]] Color Lighten(const Color& color, f32 amount);
[[nodiscard]] Color Darken(const Color& color, f32 amount);
[[nodiscard]] Color Saturate(const Color& color, f32 amount);
[[nodiscard]] Color Desaturate(const Color& color, f32 amount);
[[nodiscard]] Color RotateHue(const Color& color, f32 degrees);
[[nodiscard]] Color Invert(const Color& color);
[[nodiscard]] Color ToGrayscale(const Color& color);
// rgb * a bzw. rgb / a für Filterung und Alpha-Blending
[[nodiscard]] Color Premultiply(const Color& color);
[[nodiscard]] Color Unpremultiply(const Color& color);

// ─── Zustand ────────────────────────────────────────────────────────────────

[[nodiscard]] bool IsOpaque(const Color& color);
[[nodiscard]] bool IsTransparent(const Color& color);
[[nodiscard]] bool IsGray(const Color& color);
// Vergleich je Kanal mit Toleranz in 8-Bit-Stufen
[[nodiscard]] bool IsNearlyEqual(const Color& first, const Color& second, u8 tolerance = 1);

}  // namespace sgl
