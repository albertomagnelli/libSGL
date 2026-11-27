
//
// Created by ingsto on 14.11.26.
//
// Implementierung von Image auf Basis von stb_image / stb_image_write.
// WICHTIG: Die beiden STB_*_IMPLEMENTATION-Defines duerfen in genau einer
// Uebersetzungseinheit stehen -- das ist diese hier. Alle anderen TUs binden
// nur die Header ein (ohne Define), sonst meldet der Linker
// "multiple definition of stbi__...".
//
#include <sgl/Image.hpp>

#include <sgl/Log.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>
#include <utility>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>

namespace sgl {
namespace {

constexpr auto kPi = 3.14159265358979323846;

ImageFormat FormatFromChannels(const i32 channels) {
  switch (channels) {
  case 1:
    return ImageFormat::Gray;
  case 2:
    return ImageFormat::GrayAlpha;
  case 3:
    return ImageFormat::Rgb;
  case 4:
    return ImageFormat::Rgba;
  default:
    throw ImageError("Bildformat mit " + std::to_string(channels) +
                     " Kanälen wird nicht unterstützt");
  }
}

// Rechnet einen Farbwert in die Zielkanäle des Bildes um.
u8 ToByte(const f32 value) {
  const f32 clamped = std::clamp(value, 0.0f, 1.0f);
  return static_cast<u8>(std::lround(clamped * 255.0f));
}

// Rec. 601 – wie die üblichen Bildwerkzeuge
u8 Luma(const u8 r, const u8 g, const u8 b) {
  const i32 value = (299 * static_cast<i32>(r) + 587 * static_cast<i32>(g) +
                     114 * static_cast<i32>(b)) /
                    1000;
  return static_cast<u8>(std::clamp(value, 0, 255));
}

void AppendToVector(void *context, void *data, const i32 size) {
  auto *out = static_cast<std::vector<u8> *>(context);
  const auto *bytes = static_cast<const u8 *>(data);
  out->insert(out->end(), bytes, bytes + static_cast<std::size_t>(size));
}

std::string FailureReason() {
  const char *reason = stbi_failure_reason();
  return reason != nullptr ? std::string(reason)
                           : std::string("unbekannter Fehler");
}

// Liest einen Texel als RGBA mit Werten 0..1; außerhalb wird an den Rand
// geklemmt.
std::array<f32, 4> ReadTexel(const Image &image, i32 x, i32 y) {
  x = std::clamp(x, 0, image.width() - 1);
  y = std::clamp(y, 0, image.height() - 1);
  const u8 *texel =
      image.scanline(y) + static_cast<std::size_t>(x) *
                              static_cast<std::size_t>(image.channelCount());
  constexpr f32 kScale = 1.0f / 255.0f;
  std::array<f32, 4> rgba{0.0f, 0.0f, 0.0f, 1.0f};
  switch (image.format()) {
  case ImageFormat::Gray:
    rgba[0] = rgba[1] = rgba[2] = static_cast<f32>(texel[0]) * kScale;
    break;
  case ImageFormat::GrayAlpha:
    rgba[0] = rgba[1] = rgba[2] = static_cast<f32>(texel[0]) * kScale;
    rgba[3] = static_cast<f32>(texel[1]) * kScale;
    break;
  case ImageFormat::Rgb:
    rgba[0] = static_cast<f32>(texel[0]) * kScale;
    rgba[1] = static_cast<f32>(texel[1]) * kScale;
    rgba[2] = static_cast<f32>(texel[2]) * kScale;
    break;
  case ImageFormat::Rgba:
    rgba[0] = static_cast<f32>(texel[0]) * kScale;
    rgba[1] = static_cast<f32>(texel[1]) * kScale;
    rgba[2] = static_cast<f32>(texel[2]) * kScale;
    rgba[3] = static_cast<f32>(texel[3]) * kScale;
    break;
  }
  return rgba;
}

// Schreibt RGBA (0..1) in das Zielbild; prämultipliziertes Alpha wird vorher
// zurückgerechnet, damit die gespeicherten Farben unverfälscht sind.
void StoreRgba(Image &image, const i32 x, const i32 y,
               const std::array<f32, 4> &rgba, const bool isPremultiplied) {
  f32 r = rgba[0];
  f32 g = rgba[1];
  f32 b = rgba[2];
  f32 a = rgba[3];
  if (isPremultiplied && a > 0.0f) {
    r /= a;
    g /= a;
    b /= a;
  }
  const u8 red = ToByte(r);
  const u8 green = ToByte(g);
  const u8 blue = ToByte(b);
  const u8 alpha = ToByte(a);

  u8 *texel =
      image.scanline(y) + static_cast<std::size_t>(x) *
                              static_cast<std::size_t>(image.channelCount());
  switch (image.format()) {
  case ImageFormat::Gray:
    texel[0] = Luma(red, green, blue);
    break;
  case ImageFormat::GrayAlpha:
    texel[0] = Luma(red, green, blue);
    texel[1] = alpha;
    break;
  case ImageFormat::Rgb:
    texel[0] = red;
    texel[1] = green;
    texel[2] = blue;
    break;
  case ImageFormat::Rgba:
    texel[0] = red;
    texel[1] = green;
    texel[2] = blue;
    texel[3] = alpha;
    break;
  }
}

// Catmull-Rom, u = Abstand in Texeln (0..2)
f64 CubicWeight(const f64 u) {
  const f64 distance = std::abs(u);
  if (distance < 1.0) {
    return 1.5 * distance * distance * distance - 2.5 * distance * distance +
           1.0;
  }
  if (distance < 2.0) {
    return -0.5 * distance * distance * distance + 2.5 * distance * distance -
           4.0 * distance + 2.0;
  }
  return 0.0;
}

// Filtergewicht für den normierten Abstand 0..1.
f64 FilterWeight(const bool isCubic, const f64 normalizedDistance) {
  if (isCubic) {
    return CubicWeight(normalizedDistance * 2.0);
  }
  return 1.0 - normalizedDistance;
}

// Abtastung mit einer der vier Interpolationen. sx/sy sind Texelkoordinaten
// (Texel i liegt bei i), footprintX/Y das Verhältnis Quelle/Ziel.
std::array<f32, 4> SampleImage(const Image &source, const f64 sx, const f64 sy,
                               const Interpolation interpolation,
                               const f64 footprintX, const f64 footprintY,
                               const bool premultiplyAlpha) {
  if (interpolation == Interpolation::Nearest) {
    const i32 ix = static_cast<i32>(std::lround(sx));
    const i32 iy = static_cast<i32>(std::lround(sy));
    std::array<f32, 4> texel = ReadTexel(source, ix, iy);
    if (premultiplyAlpha) {
      texel[0] *= texel[3];
      texel[1] *= texel[3];
      texel[2] *= texel[3];
    }
    return texel;
  }

  const bool isCubic = interpolation == Interpolation::Bicubic;
  const f64 baseRadius = isCubic ? 2.0 : 1.0;
  // Linear mittelt bewusst nur 2×2 (scharf); Bilinear/Bicubic weiten den
  // Filter beim Verkleinern auf die Quellfläche aus, sonst würden Details
  // ausgelassen und das Ergebnis flimmern.
  const bool widensFootprint = interpolation != Interpolation::Linear;
  const f64 radiusX =
      baseRadius * (widensFootprint ? std::max(1.0, footprintX) : 1.0);
  const f64 radiusY =
      baseRadius * (widensFootprint ? std::max(1.0, footprintY) : 1.0);

  const i32 xBegin = static_cast<i32>(std::floor(sx - radiusX));
  const i32 xEnd = static_cast<i32>(std::ceil(sx + radiusX));
  const i32 yBegin = static_cast<i32>(std::floor(sy - radiusY));
  const i32 yEnd = static_cast<i32>(std::ceil(sy + radiusY));

  std::array<f32, 4> accum{0.0f, 0.0f, 0.0f, 0.0f};
  f64 totalWeight = 0.0;
  for (i32 iy = yBegin; iy <= yEnd; ++iy) {
    const f64 weightY = FilterWeight(isCubic, std::abs(iy - sy) / radiusY);
    if (weightY <= 0.0) {
      continue;
    }
    for (i32 ix = xBegin; ix <= xEnd; ++ix) {
      const f64 weightX = FilterWeight(isCubic, std::abs(ix - sx) / radiusX);
      if (weightX <= 0.0) {
        continue;
      }
      const f64 weight = weightX * weightY;
      std::array<f32, 4> texel = ReadTexel(source, ix, iy);
      if (premultiplyAlpha) {
        texel[0] *= texel[3];
        texel[1] *= texel[3];
        texel[2] *= texel[3];
      }
      for (i32 channel = 0; channel < 4; ++channel) {
        accum[static_cast<std::size_t>(channel)] +=
            static_cast<f32>(weight) * texel[static_cast<std::size_t>(channel)];
      }
      totalWeight += weight;
    }
  }

  if (totalWeight <= 0.0) {
    return ReadTexel(source, static_cast<i32>(std::lround(sx)),
                     static_cast<i32>(std::lround(sy)));
  }
  const f32 inverse = static_cast<f32>(1.0 / totalWeight);
  for (f32 &channel : accum) {
    channel *= inverse;
  }
  return accum;
}

// Wandelt ein Bild so, dass es sich mit dem jeweiligen Kodierer speichern
// lässt.
Image PrepareForEncoding(const Image &image, ImageFileFormat format) {
  switch (format) {
  case ImageFileFormat::Jpeg:
    // JPEG kennt kein Alpha; Graustufen bleiben ein Kanal.
    if (image.format() == ImageFormat::GrayAlpha) {
      return image.converted(ImageFormat::Gray);
    }
    if (image.format() == ImageFormat::Rgba) {
      return image.converted(ImageFormat::Rgb);
    }
    return image;
  case ImageFileFormat::Bmp:
    // BMP kann Graustufen nur als RGB ablegen.
    if (image.format() == ImageFormat::Gray) {
      return image.converted(ImageFormat::Rgb);
    }
    if (image.format() == ImageFormat::GrayAlpha) {
      return image.converted(ImageFormat::Rgba);
    }
    return image;
  default:
    return image;
  }
}

} // namespace

// ─── Formatnamen ─────────────────────────────────────────────────────────────

std::string_view ToString(const ImageFormat format) {
  switch (format) {
  case ImageFormat::Gray:
    return "gray";
  case ImageFormat::GrayAlpha:
    return "grayalpha";
  case ImageFormat::Rgb:
    return "rgb";
  case ImageFormat::Rgba:
    return "rgba";
  }
  return "unbekannt";
}

std::string_view ToString(const Interpolation interpolation) {
  switch (interpolation) {
  case Interpolation::Nearest:
    return "nearest";
  case Interpolation::Linear:
    return "linear";
  case Interpolation::Bilinear:
    return "bilinear";
  case Interpolation::Bicubic:
    return "bicubic";
  }
  return "unbekannt";
}

std::string_view ToString(ImageFileFormat format) {
  switch (format) {
  case ImageFileFormat::Png:
    return "png";
  case ImageFileFormat::Jpeg:
    return "jpeg";
  case ImageFileFormat::Bmp:
    return "bmp";
  case ImageFileFormat::Tga:
    return "tga";
  }
  return "unbekannt";
}

std::string_view FileExtension(ImageFileFormat format) {
  switch (format) {
  case ImageFileFormat::Png:
    return "png";
  case ImageFileFormat::Jpeg:
    return "jpg";
  case ImageFileFormat::Bmp:
    return "bmp";
  case ImageFileFormat::Tga:
    return "tga";
  }
  return "bin";
}

ImageFileFormat FileFormatFromPath(const std::filesystem::path &file) {
  std::string extension = file.extension().string();
  if (!extension.empty() && extension.front() == '.') {
    extension.erase(extension.begin());
  }
  std::ranges::transform(extension, extension.begin(), [](const u8 c) {
    return static_cast<char>(std::tolower(c));
  });
  if (extension == "png") {
    return ImageFileFormat::Png;
  }
  if (extension == "jpg" || extension == "jpeg") {
    return ImageFileFormat::Jpeg;
  }
  if (extension == "bmp") {
    return ImageFileFormat::Bmp;
  }
  if (extension == "tga") {
    return ImageFileFormat::Tga;
  }
  throw ImageError("Unbekannte Bildendung '." + extension +
                   "' (unterstützt: png, jpg, bmp, tga)");
}

// ─── Erzeugen und Zugriff ────────────────────────────────────────────────────

Image::Image(const i32 width, const i32 height, const ImageFormat format,
             const u8 fillValue)
    : m_width(width), m_height(height), m_format(format) {
  if (width <= 0 || height <= 0) {
    throw ImageError("Bildgröße muss positiv sein (" + std::to_string(width) +
                     "x" + std::to_string(height) + ")");
  }
  m_pixels.assign(static_cast<std::size_t>(width) *
                      static_cast<std::size_t>(height) *
                      static_cast<std::size_t>(channelCount()),
                  fillValue);
}

Image::Image(const i32 width, const i32 height, const ImageFormat format,
             const std::span<const u8> pixels)
    : m_width(width), m_height(height), m_format(format) {
  if (width <= 0 || height <= 0) {
    throw ImageError("Bildgröße muss positiv sein (" + std::to_string(width) +
                     "x" + std::to_string(height) + ")");
  }
  const std::size_t expected = static_cast<std::size_t>(width) *
                               static_cast<std::size_t>(height) *
                               static_cast<std::size_t>(channelCount());
  if (pixels.size() != expected) {
    throw ImageError("Pixelpuffer hat " + std::to_string(pixels.size()) +
                     " Bytes, erwartet " + std::to_string(expected) + " (" +
                     std::to_string(width) + "x" + std::to_string(height) +
                     "x" + std::to_string(channelCount()) + ")");
  }
  m_pixels.assign(pixels.begin(), pixels.end());
}

u8 *Image::scanline(const i32 y) {
  if (y < 0 || y >= m_height) {
    throw ImageError("scanline(" + std::to_string(y) + ") außerhalb von 0.." +
                     std::to_string(m_height - 1));
  }
  return m_pixels.data() + static_cast<std::size_t>(y) * rowStride();
}

const u8 *Image::scanline(const i32 y) const {
  if (y < 0 || y >= m_height) {
    throw ImageError("scanline(" + std::to_string(y) + ") außerhalb von 0.." +
                     std::to_string(m_height - 1));
  }
  return m_pixels.data() + static_cast<std::size_t>(y) * rowStride();
}

const u8 *Image::pixel(const i32 x, const i32 y) const {
  if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
    throw ImageError("Pixel (" + std::to_string(x) + ", " + std::to_string(y) +
                     ") liegt außerhalb von " + std::to_string(m_width) + "x" +
                     std::to_string(m_height));
  }
  return scanline(y) +
         static_cast<std::size_t>(x) * static_cast<std::size_t>(channelCount());
}

u8 *Image::mutablePixel(const i32 x, const i32 y) const {
  return const_cast<u8 *>(pixel(x, y));
}

u8 Image::at(const i32 x, const i32 y, const i32 channel) const {
  if (channel < 0 || channel >= channelCount()) {
    throw ImageError("Kanal " + std::to_string(channel) +
                     " existiert nicht (Format " +
                     std::string(ToString(m_format)) + ")");
  }
  return pixel(x, y)[channel];
}

void Image::setAt(const i32 x, const i32 y, const i32 channel,
                  const u8 value) const {
  if (channel < 0 || channel >= channelCount()) {
    throw ImageError("Kanal " + std::to_string(channel) +
                     " existiert nicht (Format " +
                     std::string(ToString(m_format)) + ")");
  }
  mutablePixel(x, y)[channel] = value;
}

std::array<u8, 4> Image::pixelAt(const i32 x, const i32 y) const {
  const u8 *texel = pixel(x, y);
  std::array<u8, 4> result{0, 0, 0, 0};
  switch (m_format) {
  case ImageFormat::Gray:
    result[0] = texel[0];
    break;
  case ImageFormat::GrayAlpha:
    result[0] = texel[0];
    result[3] = texel[1];
    break;
  case ImageFormat::Rgb:
    result[0] = texel[0];
    result[1] = texel[1];
    result[2] = texel[2];
    break;
  case ImageFormat::Rgba:
    result[0] = texel[0];
    result[1] = texel[1];
    result[2] = texel[2];
    result[3] = texel[3];
    break;
  }
  return result;
}

void Image::fill(const std::array<u8, 4> color) {
  for (auto y = 0; y < m_height; ++y) {
    for (auto x = 0; x < m_width; ++x) {
      StoreRgba(*this, x, y,
                {static_cast<f32>(color[0]) / 255.0f,
                 static_cast<f32>(color[1]) / 255.0f,
                 static_cast<f32>(color[2]) / 255.0f,
                 static_cast<f32>(color[3]) / 255.0f},
                false);
    }
  }
}


void Image::fill(const u8 value) { std::ranges::fill(m_pixels, value); }

bool Image::isPowerOfTwo() const noexcept {
  const auto isPower = [](const i32 value) {
    return value > 0 && (value & (value - 1)) == 0;
  };
  return isPower(m_width) && isPower(m_height);
}

void Image::swap(Image &other) noexcept {
  std::swap(m_width, other.m_width);
  std::swap(m_height, other.m_height);
  std::swap(m_format, other.m_format);
  m_pixels.swap(other.m_pixels);
}

void Swap(Image &first, Image &second) noexcept { first.swap(second); }

// ─── Laden und Speichern ────────────────────────────────────────────────────

std::expected<Image, ImageError>
ImageIO::TryLoad(const std::filesystem::path &file, const LoadOptions &options) {
  std::ifstream input(file, std::ios::binary);
  if (!input) {
    return std::unexpected(ImageError(
        "Bilddatei konnte nicht geöffnet werden: " + file.string()));
  }
  const std::vector<u8> bytes{std::istreambuf_iterator<char>(input),
                              std::istreambuf_iterator<char>()};
  if (bytes.empty()) {
    return std::unexpected(
        ImageError("Bilddatei ist leer: " + file.string()));
  }
  return ImageIO::TryLoadFromMemory(bytes, options);
}

std::expected<Image, ImageError> ImageIO::TryLoadFromMemory(
    std::span<const u8> data, const LoadOptions &options) {
  if (data.empty()) {
    return std::unexpected(
        ImageError("LoadFromMemory: keine Daten übergeben"));
  }
  i32 width = 0;
  i32 height = 0;
  i32 sourceChannels = 0;
  const i32 desired = options.forceFormat.has_value()
                          ? sgl::ChannelCount(*options.forceFormat)
                          : 0;

  stbi_uc *decoded =
      stbi_load_from_memory(data.data(), static_cast<i32>(data.size()), &width,
                            &height, &sourceChannels, desired);
  if (decoded == nullptr) {
    return std::unexpected(ImageError(
        "Bild konnte nicht dekodiert werden: " + FailureReason()));
  }

  const ImageFormat format =
      options.forceFormat.value_or(FormatFromChannels(sourceChannels));
  const std::size_t size =
      static_cast<std::size_t>(width) * static_cast<std::size_t>(height) *
      static_cast<std::size_t>(sgl::ChannelCount(format));

  Image image;
  try {
    image = Image(width, height, format, std::span<const u8>(decoded, size));
  } catch (const std::exception &error) {
    stbi_image_free(decoded);
    return std::unexpected(ImageError(error.what()));
  }
  stbi_image_free(decoded);

  if (options.flipVertically) {
    image.flipVertically();
  }
  SLOGD << "Bild geladen: " << image.width() << "x" << image.height() << " "
       << ToString(image.format());
  return image;
}

std::expected<std::vector<u8>, ImageError>
ImageIO::TryEncode(const Image &source, const ImageFileFormat format,
                   const SaveOptions &options) {
  try {
    if (source.isEmpty()) {
      return std::unexpected(ImageError("Encode: Bild ist leer"));
    }
    const Image prepared = PrepareForEncoding(source, format);
    const i32 components = prepared.channelCount();
    std::vector<u8> bytes;
    bytes.reserve(prepared.byteSize() / 2 + 64);

    i32 written = 0;
    switch (format) {
    case ImageFileFormat::Png:
      written = stbi_write_png_to_func(
          AppendToVector, &bytes, prepared.width(), prepared.height(),
          components, prepared.data(),
          static_cast<i32>(prepared.rowStride()));
      break;
    case ImageFileFormat::Jpeg:
      written = stbi_write_jpg_to_func(
          AppendToVector, &bytes, prepared.width(), prepared.height(),
          components, prepared.data(),
          std::clamp(options.jpegQuality, 1, 100));
      break;
    case ImageFileFormat::Bmp:
      written =
          stbi_write_bmp_to_func(AppendToVector, &bytes, prepared.width(),
                                 prepared.height(), components, prepared.data());
      break;
    case ImageFileFormat::Tga:
      written =
          stbi_write_tga_to_func(AppendToVector, &bytes, prepared.width(),
                                 prepared.height(), components, prepared.data());
      break;
    }
    if (written == 0 || bytes.empty()) {
      return std::unexpected(ImageError(
          "Bild konnte nicht als " + std::string(ToString(format)) +
          " kodiert werden"));
    }
    return bytes;
  } catch (const std::exception &error) {
    return std::unexpected(ImageError(error.what()));
  }
}

std::expected<void, ImageError>
ImageIO::TrySave(const Image &image, const std::filesystem::path &file,
                 const SaveOptions &options) {
  if (image.isEmpty()) {
    return std::unexpected(ImageError("Speichern: Bild ist leer"));
  }

  ImageFileFormat format;
  try {
    format = FileFormatFromPath(file);
  } catch (const std::exception &error) {
    return std::unexpected(ImageError(error.what()));
  }

  const auto bytes = ImageIO::TryEncode(image, format, options);
  if (!bytes) {
    return std::unexpected(bytes.error());
  }

  const std::filesystem::path parent = file.parent_path();
  if (!parent.empty()) {
    std::error_code ec;
    std::filesystem::create_directories(parent, ec);
    if (ec) {
      return std::unexpected(ImageError(
          "Verzeichnis konnte nicht angelegt werden: " + parent.string() +
          " (" + ec.message() + ")"));
    }
  }
  std::ofstream output(file, std::ios::binary | std::ios::trunc);
  if (!output) {
    return std::unexpected(ImageError(
        "Bilddatei konnte nicht geschrieben werden: " + file.string()));
  }
  output.write(reinterpret_cast<const char *>(bytes->data()),
               static_cast<std::streamsize>(bytes->size()));
  if (!output) {
    return std::unexpected(ImageError(
        "Schreiben der Bilddatei fehlgeschlagen: " + file.string()));
  }
  SLOGD << "Bild gespeichert: " << file.string() << " (" << bytes->size()
       << " Bytes)";
  return {};
}

Image ImageIO::Load(const std::filesystem::path &file,
                    const LoadOptions &options) {
  auto result = ImageIO::TryLoad(file, options);
  if (!result) {
    throw result.error();
  }
  return std::move(*result);
}

Image ImageIO::LoadFromMemory(std::span<const u8> data,
                              const LoadOptions &options) {
  auto result = ImageIO::TryLoadFromMemory(data, options);
  if (!result) {
    throw result.error();
  }
  return std::move(*result);
}

std::vector<u8> ImageIO::Encode(const Image &source,
                               const ImageFileFormat format,
                               const SaveOptions &options) {
  auto result = ImageIO::TryEncode(source, format, options);
  if (!result) {
    throw result.error();
  }
  return std::move(*result);
}

void ImageIO::Save(const Image &image, const std::filesystem::path &file,
                   const SaveOptions &options) {
  auto result = ImageIO::TrySave(image, file, options);
  if (!result) {
    throw result.error();
  }
}

// ─── Umwandeln und Ausschneiden ─────────────────────────────────────────────

Image Image::converted(ImageFormat format) const {
  if (format == m_format) {
    return *this;
  }
  if (isEmpty()) {
    throw ImageError("Umwandeln: Bild ist leer");
  }
  Image result(m_width, m_height, format);
  for (i32 y = 0; y < m_height; ++y) {
    for (i32 x = 0; x < m_width; ++x) {
      const u8 *texel = pixel(x, y);
      u8 red = 0;
      u8 green = 0;
      u8 blue = 0;
      u8 alpha = 255;
      switch (m_format) {
      case ImageFormat::Gray:
        red = green = blue = texel[0];
        break;
      case ImageFormat::GrayAlpha:
        red = green = blue = texel[0];
        alpha = texel[1];
        break;
      case ImageFormat::Rgb:
        red = texel[0];
        green = texel[1];
        blue = texel[2];
        break;
      case ImageFormat::Rgba:
        red = texel[0];
        green = texel[1];
        blue = texel[2];
        alpha = texel[3];
        break;
      }
      u8 *target = result.mutablePixel(x, y);
      switch (format) {
      case ImageFormat::Gray:
        target[0] = Luma(red, green, blue);
        break;
      case ImageFormat::GrayAlpha:
        target[0] = Luma(red, green, blue);
        target[1] = alpha;
        break;
      case ImageFormat::Rgb:
        target[0] = red;
        target[1] = green;
        target[2] = blue;
        break;
      case ImageFormat::Rgba:
        target[0] = red;
        target[1] = green;
        target[2] = blue;
        target[3] = alpha;
        break;
      }
    }
  }
  return result;
}

void Image::convertTo(const ImageFormat format) { *this = converted(format); }

Image Image::cropped(const i32 x, const i32 y, const i32 width,
                     const i32 height) const {
  if (width <= 0 || height <= 0) {
    throw ImageError("Ausschneiden: Größe muss positiv sein (" +
                     std::to_string(width) + "x" + std::to_string(height) +
                     ")");
  }
  if (x < 0 || y < 0 || x + width > m_width || y + height > m_height) {
    throw ImageError("Ausschneiden: Bereich " + std::to_string(width) + "x" +
                     std::to_string(height) + " bei (" + std::to_string(x) +
                     ", " + std::to_string(y) + ") passt nicht in " +
                     std::to_string(m_width) + "x" + std::to_string(m_height));
  }
  Image result(width, height, m_format);
  for (i32 row = 0; row < height; ++row) {
    std::memcpy(result.scanline(row),
                scanline(y + row) +
                    static_cast<std::size_t>(x) *
                        static_cast<std::size_t>(channelCount()),
                static_cast<std::size_t>(width) *
                    static_cast<std::size_t>(channelCount()));
  }
  return result;
}

// ─── Spiegeln ───────────────────────────────────────────────────────────────

void Image::flipHorizontally() {
  if (isEmpty()) {
    return;
  }
  const i32 channels = channelCount();
  for (i32 y = 0; y < m_height; ++y) {
    u8 *row = scanline(y);
    for (i32 x = 0; x < m_width / 2; ++x) {
      u8 *left = row + static_cast<std::size_t>(x) * channels;
      u8 *right = row + static_cast<std::size_t>(m_width - 1 - x) * channels;
      for (i32 channel = 0; channel < channels; ++channel) {
        std::swap(left[channel], right[channel]);
      }
    }
  }
}

void Image::flipVertically() {
  if (isEmpty()) {
    return;
  }
  const std::size_t stride = rowStride();
  for (i32 y = 0; y < m_height / 2; ++y) {
    u8 *top = m_pixels.data() + static_cast<std::size_t>(y) * stride;
    u8 *bottom =
        m_pixels.data() + static_cast<std::size_t>(m_height - 1 - y) * stride;
    std::swap_ranges(top, top + stride, bottom);
  }
}

Image Image::flippedHorizontally() const {
  Image copy(*this);
  copy.flipHorizontally();
  return copy;
}

Image Image::flippedVertically() const {
  Image copy(*this);
  copy.flipVertically();
  return copy;
}

// ─── Drehen ─────────────────────────────────────────────────────────────────

void Image::rotate90Clockwise() {
  if (isEmpty()) {
    return;
  }
  const i32 channels = channelCount();
  const i32 newWidth = m_height;
  const i32 newHeight = m_width;
  std::vector<u8> rotated(byteSize());
  for (i32 y = 0; y < newHeight; ++y) {
    for (i32 x = 0; x < newWidth; ++x) {
      // Ziel (x, y) stammt aus Quelle (y, Höhe-1-x)
      const u8 *source = pixel(y, m_height - 1 - x);
      std::memcpy(rotated.data() + (static_cast<std::size_t>(y) *
                                        static_cast<std::size_t>(newWidth) +
                                    static_cast<std::size_t>(x)) *
                                       static_cast<std::size_t>(channels),
                  source, static_cast<std::size_t>(channels));
    }
  }
  m_width = newWidth;
  m_height = newHeight;
  m_pixels.swap(rotated);
}

void Image::rotate180() {
  if (isEmpty()) {
    return;
  }
  flipVertically();
  flipHorizontally();
}

void Image::rotate270Clockwise() {
  if (isEmpty()) {
    return;
  }
  const i32 channels = channelCount();
  const i32 newWidth = m_height;
  const i32 newHeight = m_width;
  std::vector<u8> rotated(byteSize());
  for (i32 y = 0; y < newHeight; ++y) {
    for (i32 x = 0; x < newWidth; ++x) {
      // Ziel (x, y) stammt aus Quelle (Breite-1-y, x)
      const u8 *source = pixel(m_width - 1 - y, x);
      std::memcpy(rotated.data() + (static_cast<std::size_t>(y) *
                                        static_cast<std::size_t>(newWidth) +
                                    static_cast<std::size_t>(x)) *
                                       static_cast<std::size_t>(channels),
                  source, static_cast<std::size_t>(channels));
    }
  }
  m_width = newWidth;
  m_height = newHeight;
  m_pixels.swap(rotated);
}

Image Image::rotated(const i32 clockwiseDegrees,
                     const RotateOptions &options) const {
  if (isEmpty()) {
    throw ImageError("Drehen: Bild ist leer");
  }
  const i32 normalized = ((clockwiseDegrees % 360) + 360) % 360;
  if (normalized == 0) {
    return *this;
  }
  // Vierteldrehungen pixelgenau, ohne Filterverluste
  if (normalized == 90) {
    Image result(*this);
    result.rotate90Clockwise();
    return result;
  }
  if (normalized == 180) {
    Image result(*this);
    result.rotate180();
    return result;
  }
  if (normalized == 270) {
    Image result(*this);
    result.rotate270Clockwise();
    return result;
  }

  const f64 angle = static_cast<f64>(normalized) * kPi / 180.0;
  const f64 cosine = std::cos(angle);
  const f64 sine = std::sin(angle);

  i32 outWidth = m_width;
  i32 outHeight = m_height;
  if (options.expandCanvas) {
    const f64 width = m_width;
    const f64 height = m_height;
    outWidth = std::max(
        1, static_cast<i32>(std::ceil(std::abs(width * cosine) +
                                      std::abs(height * sine) - 1e-9)));
    outHeight = std::max(
        1, static_cast<i32>(std::ceil(std::abs(width * sine) +
                                      std::abs(height * cosine) - 1e-9)));
  }

  const std::array<f32, 4> fillColor{static_cast<f32>(options.fill[0]) / 255.0f,
                                     static_cast<f32>(options.fill[1]) / 255.0f,
                                     static_cast<f32>(options.fill[2]) / 255.0f,
                                     static_cast<f32>(options.fill[3]) /
                                         255.0f};
  Image result(outWidth, outHeight, m_format);
  const f64 sourceCenterX = m_width * 0.5;
  const f64 sourceCenterY = m_height * 0.5;
  const f64 targetCenterX = outWidth * 0.5;
  const f64 targetCenterY = outHeight * 0.5;

  for (i32 y = 0; y < outHeight; ++y) {
    for (i32 x = 0; x < outWidth; ++x) {
      // Rückwärtstransformation: Zielpixel -> Quellkoordinate
      const f64 relativeX = (x + 0.5) - targetCenterX;
      const f64 relativeY = (y + 0.5) - targetCenterY;
      const f64 sourceX =
          relativeX * cosine + relativeY * sine + sourceCenterX - 0.5;
      const f64 sourceY =
          -relativeX * sine + relativeY * cosine + sourceCenterY - 0.5;

      if (sourceX < -0.5 || sourceY < -0.5 || sourceX > m_width - 0.5 ||
          sourceY > m_height - 0.5) {
        StoreRgba(result, x, y, fillColor, false);
        continue;
      }
      const std::array<f32, 4> sampled =
          SampleImage(*this, sourceX, sourceY, options.interpolation, 1.0, 1.0,
                      options.premultiplyAlpha);
      StoreRgba(result, x, y, sampled, options.premultiplyAlpha);
    }
  }
  return result;
}

// ─── Skalieren ──────────────────────────────────────────────────────────────

Image Image::scaled(i32 newWidth, i32 newHeight,
                    const Interpolation interpolation) const {
  if (isEmpty()) {
    throw ImageError("Skalieren: Bild ist leer");
  }
  if (newWidth <= 0 || newHeight <= 0) {
    throw ImageError("Skalieren: Zielgröße muss positiv sein (" +
                     std::to_string(newWidth) + "x" +
                     std::to_string(newHeight) + ")");
  }
  if (newWidth == m_width && newHeight == m_height) {
    return *this;
  }

  Image result(newWidth, newHeight, m_format);
  const f64 scaleX = static_cast<f64>(m_width) / static_cast<f64>(newWidth);
  const f64 scaleY = static_cast<f64>(m_height) / static_cast<f64>(newHeight);

  for (i32 y = 0; y < newHeight; ++y) {
    // Pixelmitte auf die Quelle abbilden: (y + 0.5) * scale - 0.5
    const f64 sourceY = (y + 0.5) * scaleY - 0.5;
    for (i32 x = 0; x < newWidth; ++x) {
      const f64 sourceX = (x + 0.5) * scaleX - 0.5;
      const std::array<f32, 4> sampled = SampleImage(
          *this, sourceX, sourceY, interpolation, scaleX, scaleY, true);
      StoreRgba(result, x, y, sampled, true);
    }
  }
  SLOGD << "Bild skaliert: " << m_width << "x" << m_height << " -> " << newWidth
       << "x" << newHeight << " (" << ToString(interpolation) << ")";
  return result;
}

Image Image::scaled(const f64 factor, const Interpolation interpolation) const {
  if (isEmpty()) {
    throw ImageError("Skalieren: Bild ist leer");
  }
  if (!(factor > 0.0)) {
    throw ImageError("Skalieren: Faktor muss positiv sein");
  }
  const i32 newWidth =
      std::max(1, static_cast<i32>(std::lround(m_width * factor)));
  const i32 newHeight =
      std::max(1, static_cast<i32>(std::lround(m_height * factor)));
  return scaled(newWidth, newHeight, interpolation);
}

Image Image::scaledToFit(const i32 maxWidth, const i32 maxHeight,
                         const Interpolation interpolation) const {
  if (maxWidth <= 0 || maxHeight <= 0) {
    throw ImageError("scaledToFit: Grenzen müssen positiv sein (" +
                     std::to_string(maxWidth) + "x" +
                     std::to_string(maxHeight) + ")");
  }
  if (isEmpty()) {
    throw ImageError("scaledToFit: Bild ist leer");
  }
  const f64 factor = std::min(static_cast<f64>(maxWidth) / m_width,
                              static_cast<f64>(maxHeight) / m_height);
  if (factor >= 1.0) {
    return *this; // nicht vergrößern
  }
  const i32 newWidth =
      std::max(1, static_cast<i32>(std::lround(m_width * factor)));
  const i32 newHeight =
      std::max(1, static_cast<i32>(std::lround(m_height * factor)));
  return scaled(newWidth, newHeight, interpolation);
}

void Image::resize(const i32 newWidth, const i32 newHeight,
                   const Interpolation interpolation) {
  *this = scaled(newWidth, newHeight, interpolation);
}

} // namespace sgl
