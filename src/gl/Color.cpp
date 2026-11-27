#include <sgl/Color.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace sgl {
namespace {

constexpr f32 kEpsilon = 1e-6f;

// Rechenfarbe mit 0..1 je Kanal; Color selbst speichert 8 Bit.
struct Rgba {
    f32 r{0.0f};
    f32 g{0.0f};
    f32 b{0.0f};
    f32 a{1.0f};
};

Rgba FromColor(const Color& color) {
    const vec4 channels = color.toVec4();
    return {channels.r, channels.g, channels.b, channels.a};
}

u8 ToByte(const f32 value) {
    const f32 clamped = std::clamp(value, 0.0f, 1.0f);
    return static_cast<u8>(std::lround(clamped * 255.0f));
}

Color ToColor(const Rgba& rgba) {
    return Color(ToByte(rgba.r), ToByte(rgba.g), ToByte(rgba.b), ToByte(rgba.a));
}

// Farbton auf 0..360 normalisieren; NaN/Inf werden zu 0.
f32 WrapHue(const f32 hue) {
    if (!std::isfinite(hue)) {
        return 0.0f;
    }
    f32 result = std::fmod(hue, 360.0f);
    if (result < 0.0f) {
        result += 360.0f;
    }
    return result;
}

i32 HexDigit(char character, const std::string_view text) {
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'a' && character <= 'f') {
        return character - 'a' + 10;
    }
    if (character >= 'A' && character <= 'F') {
        return character - 'A' + 10;
    }
    throw ColorError("Ungültiges Zeichen '" + std::string(1, character) +
                     "' in Hex-Farbe '" + std::string(text) + "'");
}

// Ein Farbtonpaar ("ff") zu 0..255 umrechnen.
i32 HexPair(const std::string_view text, const std::size_t offset) {
    return HexDigit(text[offset], text) * 16 + HexDigit(text[offset + 1], text);
}

// sRGB-Transferfunktionen
f32 ToLinearChannel(const f32 value) {
    return value <= 0.04045f ? value / 12.92f : std::pow((value + 0.055f) / 1.055f, 2.4f);
}

f32 ToSrgbChannel(const f32 value) {
    const f32 clamped = std::clamp(value, 0.0f, 1.0f);
    return clamped <= 0.0031308f ? clamped * 12.92f
                                 : 1.055f * std::pow(clamped, 1.0f / 2.4f) - 0.055f;
}

}  // namespace

// ─── Erzeugen ───────────────────────────────────────────────────────────────

Color FromHsv(const Hsv& hsv, const u8 alpha) {
    const f32 saturation = std::clamp(hsv.s, 0.0f, 1.0f);
    const f32 value = std::clamp(hsv.v, 0.0f, 1.0f);
    const f32 hue = WrapHue(hsv.h) / 60.0f;
    const i32 sector = static_cast<i32>(std::floor(hue)) % 6;
    const f32 fraction = hue - std::floor(hue);

    const f32 p = value * (1.0f - saturation);
    const f32 q = value * (1.0f - saturation * fraction);
    const f32 t = value * (1.0f - saturation * (1.0f - fraction));
    const f32 unitAlpha = static_cast<f32>(alpha) / 255.0f;

    switch (sector) {
        case 0:
            return ToColor({value, t, p, unitAlpha});
        case 1:
            return ToColor({q, value, p, unitAlpha});
        case 2:
            return ToColor({p, value, t, unitAlpha});
        case 3:
            return ToColor({p, q, value, unitAlpha});
        case 4:
            return ToColor({t, p, value, unitAlpha});
        default:
            return ToColor({value, p, q, unitAlpha});
    }
}

Color FromHsl(const Hsl& hsl, const u8 alpha) {
    const f32 saturation = std::clamp(hsl.s, 0.0f, 1.0f);
    const f32 lightness = std::clamp(hsl.l, 0.0f, 1.0f);
    const f32 hue = WrapHue(hsl.h) / 360.0f;
    const f32 unitAlpha = static_cast<f32>(alpha) / 255.0f;

    if (saturation <= kEpsilon) {
        return ToColor({lightness, lightness, lightness, unitAlpha});
    }
    const f32 chroma = (1.0f - std::fabs(2.0f * lightness - 1.0f)) * saturation;
    const f32 sector = hue * 6.0f;
    const f32 secondary = chroma * (1.0f - std::fabs(std::fmod(sector, 2.0f) - 1.0f));

    f32 r = 0.0f;
    f32 g = 0.0f;
    f32 b = 0.0f;
    switch (static_cast<i32>(std::floor(sector)) % 6) {
        case 0:
            r = chroma;
            g = secondary;
            break;
        case 1:
            r = secondary;
            g = chroma;
            break;
        case 2:
            g = chroma;
            b = secondary;
            break;
        case 3:
            g = secondary;
            b = chroma;
            break;
        case 4:
            r = secondary;
            b = chroma;
            break;
        default:
            r = chroma;
            b = secondary;
            break;
    }
    const f32 match = lightness - chroma * 0.5f;
    return ToColor({r + match, g + match, b + match, unitAlpha});
}

Color FromHwb(const Hwb& hwb, const u8 alpha) {
    const f32 white = std::clamp(hwb.w, 0.0f, 1.0f);
    const f32 black = std::clamp(hwb.b, 0.0f, 1.0f);
    const f32 sum = white + black;
    if (sum >= 1.0f) {
        // Nur noch Grau übrig
        const f32 gray = white / sum;
        return ToColor({gray, gray, gray, static_cast<f32>(alpha) / 255.0f});
    }
    const Rgba pure = FromColor(FromHsv({hwb.h, 1.0f, 1.0f}, 255));
    const f32 factor = 1.0f - sum;
    return ToColor({pure.r * factor + white, pure.g * factor + white, pure.b * factor + white,
                    static_cast<f32>(alpha) / 255.0f});
}

Color FromCmyk(const Cmyk& cmyk, const u8 alpha) {
    const f32 k = std::clamp(cmyk.k, 0.0f, 1.0f);
    const f32 scale = 1.0f - k;
    const f32 c = std::clamp(cmyk.c, 0.0f, 1.0f);
    const f32 m = std::clamp(cmyk.m, 0.0f, 1.0f);
    const f32 y = std::clamp(cmyk.y, 0.0f, 1.0f);
    return ToColor({(1.0f - c) * scale, (1.0f - m) * scale, (1.0f - y) * scale,
                    static_cast<f32>(alpha) / 255.0f});
}

Color FromHex(const std::string_view hex) {
    std::string_view text = hex;
    if (!text.empty() && text.front() == '#') {
        text.remove_prefix(1);
    }
    if (text.size() != 3 && text.size() != 4 && text.size() != 6 && text.size() != 8) {
        throw ColorError("Hex-Farbe muss 3, 4, 6 oder 8 Stellen haben: '" + std::string(hex) + "'");
    }
    if (text.size() <= 4) {
        // Kurzform "#rgb" / "#rgba": jede Stelle wird verdoppelt
        const auto nibble = [&text](std::size_t index) {
            return static_cast<u8>(HexDigit(text[index], text) * 17);
        };
        return Color(nibble(0), nibble(1), nibble(2), text.size() == 4 ? nibble(3) : 255);
    }
    return Color(static_cast<u8>(HexPair(text, 0)), static_cast<u8>(HexPair(text, 2)),
                 static_cast<u8>(HexPair(text, 4)),
                 static_cast<u8>(text.size() == 8 ? HexPair(text, 6) : 255));
}

Color FromGray(const f32 value, const u8 alpha) {
    const u8 gray = ToByte(value);
    return Color(gray, gray, gray, alpha);
}

Color FromGray8(const u8 value) {
    return Color(value, value, value, 255);
}

Color FromPackedRgba(const u32 packed) {
    return Color(static_cast<u8>((packed >> 24) & 0xFFu), static_cast<u8>((packed >> 16) & 0xFFu),
                 static_cast<u8>((packed >> 8) & 0xFFu), static_cast<u8>(packed & 0xFFu));
}

Color FromVec4(const vec4& rgba) {
    return ToColor({rgba.r, rgba.g, rgba.b, rgba.a});
}

Color FromLinear(const vec3& linear, const u8 alpha) {
    return ToColor({ToSrgbChannel(linear.r), ToSrgbChannel(linear.g), ToSrgbChannel(linear.b),
                    static_cast<f32>(alpha) / 255.0f});
}

// ─── Ausgeben ───────────────────────────────────────────────────────────────

Hsv ToHsv(const Color& color) {
    const Rgba rgba = FromColor(color);
    const f32 maximum = std::max({rgba.r, rgba.g, rgba.b});
    const f32 minimum = std::min({rgba.r, rgba.g, rgba.b});
    const f32 delta = maximum - minimum;

    Hsv hsv;
    hsv.v = maximum;
    hsv.s = maximum <= kEpsilon ? 0.0f : delta / maximum;
    if (delta <= kEpsilon) {
        return hsv;
    }
    f32 hue = 0.0f;
    if (maximum == rgba.r) {
        hue = 60.0f * std::fmod((rgba.g - rgba.b) / delta, 6.0f);
    } else if (maximum == rgba.g) {
        hue = 60.0f * ((rgba.b - rgba.r) / delta + 2.0f);
    } else {
        hue = 60.0f * ((rgba.r - rgba.g) / delta + 4.0f);
    }
    hsv.h = WrapHue(hue);
    return hsv;
}

Hsl ToHsl(const Color& color) {
    const Rgba rgba = FromColor(color);
    const f32 maximum = std::max({rgba.r, rgba.g, rgba.b});
    const f32 minimum = std::min({rgba.r, rgba.g, rgba.b});
    const f32 delta = maximum - minimum;

    Hsl hsl;
    hsl.l = (maximum + minimum) * 0.5f;
    if (delta <= kEpsilon) {
        return hsl;
    }
    const f32 denominator = 1.0f - std::fabs(2.0f * hsl.l - 1.0f);
    hsl.s = denominator <= kEpsilon ? 0.0f : delta / denominator;
    hsl.h = ToHsv(color).h;
    return hsl;
}

Hwb ToHwb(const Color& color) {
    const Hsv hsv = ToHsv(color);
    Hwb hwb;
    hwb.h = hsv.h;
    hwb.w = (1.0f - hsv.s) * hsv.v;
    hwb.b = 1.0f - hsv.v;
    return hwb;
}

Cmyk ToCmyk(const Color& color) {
    const Rgba rgba = FromColor(color);
    const f32 maximum = std::max({rgba.r, rgba.g, rgba.b});
    Cmyk cmyk;
    cmyk.k = 1.0f - maximum;
    if (cmyk.k >= 1.0f - kEpsilon) {
        return cmyk;
    }
    const f32 scale = 1.0f / (1.0f - cmyk.k);
    cmyk.c = (1.0f - rgba.r - cmyk.k) * scale;
    cmyk.m = (1.0f - rgba.g - cmyk.k) * scale;
    cmyk.y = (1.0f - rgba.b - cmyk.k) * scale;
    return cmyk;
}

std::string ToHex(const Color& color, const bool withAlpha) {
    char text[10] = {};
    if (withAlpha) {
        std::snprintf(text, sizeof(text), "#%02x%02x%02x%02x", color.r, color.g, color.b, color.a);
    } else {
        std::snprintf(text, sizeof(text), "#%02x%02x%02x", color.r, color.g, color.b);
    }
    return std::string(text);
}

u32 ToPackedRgba(const Color& color) {
    return (static_cast<u32>(color.r) << 24) | (static_cast<u32>(color.g) << 16) |
           (static_cast<u32>(color.b) << 8) | static_cast<u32>(color.a);
}

vec3 ToLinear(const Color& color) {
    const Rgba rgba = FromColor(color);
    return {ToLinearChannel(rgba.r), ToLinearChannel(rgba.g), ToLinearChannel(rgba.b)};
}

// ─── Helligkeit ─────────────────────────────────────────────────────────────

f32 Luminance(const Color& color) {
    const vec3 linear = ToLinear(color);
    return 0.2126f * linear.r + 0.7152f * linear.g + 0.0722f * linear.b;
}

f32 PerceivedLuminance(const Color& color) {
    const Rgba rgba = FromColor(color);
    return 0.299f * rgba.r + 0.587f * rgba.g + 0.114f * rgba.b;
}

u8 Luminance8(const Color& color) {
    return ToByte(PerceivedLuminance(color));
}

// ─── Ableitungen ────────────────────────────────────────────────────────────

Color WithAlpha(const Color& color, const u8 alpha) {
    return Color(color.r, color.g, color.b, alpha);
}

Color Mix(const Color& from, const Color& to, const f32 t) {
    const f32 amount = std::clamp(t, 0.0f, 1.0f);
    const auto blend = [amount](u8 first, u8 second) {
        const f32 value =
            static_cast<f32>(first) + (static_cast<f32>(second) - static_cast<f32>(first)) * amount;
        return static_cast<u8>(std::lround(std::clamp(value, 0.0f, 255.0f)));
    };
    return Color(blend(from.r, to.r), blend(from.g, to.g), blend(from.b, to.b),
                 blend(from.a, to.a));
}

Color Lighten(const Color& color, const f32 amount) {
    Hsl hsl = ToHsl(color);
    hsl.l = std::clamp(hsl.l + amount, 0.0f, 1.0f);
    return FromHsl(hsl, color.a);
}

Color Darken(const Color& color, const f32 amount) {
    return Lighten(color, -amount);
}

Color Saturate(const Color& color, const f32 amount) {
    Hsl hsl = ToHsl(color);
    hsl.s = std::clamp(hsl.s + amount, 0.0f, 1.0f);
    return FromHsl(hsl, color.a);
}

Color Desaturate(const Color& color, const f32 amount) {
    return Saturate(color, -amount);
}

Color RotateHue(const Color& color, const f32 degrees) {
    Hsl hsl = ToHsl(color);
    hsl.h = WrapHue(hsl.h + degrees);
    return FromHsl(hsl, color.a);
}

Color Invert(const Color& color) {
    return Color(static_cast<u8>(255 - color.r), static_cast<u8>(255 - color.g),
                 static_cast<u8>(255 - color.b), color.a);
}

Color ToGrayscale(const Color& color) {
    const u8 gray = Luminance8(color);
    return Color(gray, gray, gray, color.a);
}

Color Premultiply(const Color& color) {
    const f32 alpha = static_cast<f32>(color.a) / 255.0f;
    const auto scaled = [alpha](u8 channel) {
        return static_cast<u8>(std::lround(static_cast<f32>(channel) * alpha));
    };
    return Color(scaled(color.r), scaled(color.g), scaled(color.b), color.a);
}

Color Unpremultiply(const Color& color) {
    if (color.a == 0) {
        return Color(0, 0, 0, 0);
    }
    const f32 scale = 255.0f / static_cast<f32>(color.a);
    const auto restore = [scale](u8 channel) {
        return static_cast<u8>(
            std::lround(std::clamp(static_cast<f32>(channel) * scale, 0.0f, 255.0f)));
    };
    return Color(restore(color.r), restore(color.g), restore(color.b), color.a);
}

// ─── Zustand ────────────────────────────────────────────────────────────────

bool IsOpaque(const Color& color) {
    return color.a == 255;
}

bool IsTransparent(const Color& color) {
    return color.a == 0;
}

bool IsGray(const Color& color) {
    return color.r == color.g && color.g == color.b;
}

bool IsNearlyEqual(const Color& first, const Color& second, const u8 tolerance) {
    const auto within = [tolerance](u8 left, u8 right) {
        return std::abs(static_cast<i32>(left) - static_cast<i32>(right)) <=
               static_cast<i32>(tolerance);
    };
    return within(first.r, second.r) && within(first.g, second.g) && within(first.b, second.b) &&
           within(first.a, second.a);
}

}  // namespace sgl
