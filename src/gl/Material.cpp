#include <sgl/Material.hpp>

#include <algorithm>
#include <array>
#include <cmath>

#include <glm/geometric.hpp>

namespace sgl {

namespace {

u8 ToByte(const f32 value) {
  const f32 clamped = std::clamp(value, 0.0f, 1.0f);
  return static_cast<u8>(std::lround(clamped * 255.0f));
}

f32 FromByte(const u8 value) {
  return static_cast<f32>(value) / 255.0f;
}

Image MakeColorImage(const Color& color) {
  Image image(1, 1, ImageFormat::Rgb);
  image.fill({color.r, color.g, color.b, 255});
  return image;
}

Image MakeScalarImage(const f32 value) {
  Image image(1, 1, ImageFormat::Gray);
  image.fill(ToByte(value));
  return image;
}

Image MakeNormalImage(const vec3& normal) {
  Image image(1, 1, ImageFormat::Rgb);
  image.fill({ToByte((normal.x * 0.5f) + 0.5f),
              ToByte((normal.y * 0.5f) + 0.5f),
              ToByte((normal.z * 0.5f) + 0.5f), 255});
  return image;
}

Image ToRgb(const Image& image, const char* name) {
  if (image.isEmpty()) {
    throw MaterialError(std::string("Material: ") + name + " ist leer");
  }

  return image.converted(ImageFormat::Rgb);
}

Image ToGray(const Image& image, const i32 channel, const char* name) {
  if (image.isEmpty()) {
    throw MaterialError(std::string("Material: ") + name + " ist leer");
  }

  if (channel < 0 || channel >= image.getChannelCount()) {
    throw MaterialError(std::string("Material: Kanal ") + std::to_string(channel) +
                        " liegt ausserhalb von " + name);
  }

  Image result(image.getWidth(), image.getHeight(), ImageFormat::Gray);

  for (i32 y = 0; y < image.getHeight(); ++y) {
    for (i32 x = 0; x < image.getWidth(); ++x) {
      result.setChannel(x, y, 0, image.getChannel(x, y, channel));
    }
  }

  return result;
}

bool IsConstant(const Image& image) noexcept {
  return !image.isEmpty() && image.getWidth() == 1 && image.getHeight() == 1;
}

f32 FirstValue(const Image& image) noexcept {
  if (image.isEmpty()) return 0.0f;

  return FromByte(image.getChannel(0, 0, 0));
}

}  // namespace

Material::Material()
    : m_albedoMap(MakeColorImage(WHITE)),
      m_metallicMap(MakeScalarImage(0.0f)),
      m_roughnessMap(MakeScalarImage(0.5f)),
      m_ambientOcclusionMap(MakeScalarImage(1.0f)),
      m_normalMap(MakeNormalImage(vec3(0.0f, 0.0f, 1.0f))),
      m_heightMap(MakeScalarImage(0.0f)),
      m_emissiveMap(MakeColorImage(BLACK)) {
  updateSize();
}

// ─── Zustand ─────────────────────────────────────────────────────────────────

bool Material::isValid() const noexcept {
  return !m_albedoMap.isEmpty() && !m_metallicMap.isEmpty() &&
         !m_roughnessMap.isEmpty() && !m_ambientOcclusionMap.isEmpty() &&
         !m_normalMap.isEmpty() && !m_heightMap.isEmpty() && !m_emissiveMap.isEmpty();
}

bool Material::hasConsistentTextureSize() const noexcept {
  const std::array<const Image*, 7> maps{
      &m_albedoMap,  &m_metallicMap, &m_roughnessMap, &m_ambientOcclusionMap,
      &m_normalMap,  &m_heightMap,   &m_emissiveMap};

  i32 width  = 0;
  i32 height = 0;

  for (const Image* image : maps) {
    if (image->isEmpty()) return false;
    if (image->getWidth() == 1 && image->getHeight() == 1) continue;

    if (width == 0) {
      width  = image->getWidth();
      height = image->getHeight();
    } else if (image->getWidth() != width || image->getHeight() != height) {
      return false;
    }
  }

  return true;
}

i32 Material::getWidth() const noexcept { return m_width; }

i32 Material::getHeight() const noexcept { return m_height; }

void Material::updateSize() noexcept {
  m_width  = m_albedoMap.getWidth();
  m_height = m_albedoMap.getHeight();
}

// ─── Albedo ──────────────────────────────────────────────────────────────────

void Material::setAlbedo(const Color& color) {
  m_albedoMap = MakeColorImage(color);
  updateSize();
}

void Material::setAlbedo(const Image& image) {
  m_albedoMap = ToRgb(image, "Albedo");
  updateSize();
}

bool Material::isAlbedoConstant() const noexcept { return IsConstant(m_albedoMap); }

// ─── Metallic ────────────────────────────────────────────────────────────────

void Material::setMetallic(const f32 value) {
  m_metallicMap = MakeScalarImage(value);
}

void Material::setMetallic(const Image& image, const i32 channel) {
  m_metallicMap = ToGray(image, channel, "Metallic");
}

f32 Material::getMetallicValue() const noexcept { return FirstValue(m_metallicMap); }

bool Material::isMetallicConstant() const noexcept { return IsConstant(m_metallicMap); }

// ─── Roughness ───────────────────────────────────────────────────────────────

void Material::setRoughness(const f32 value) {
  m_roughnessMap = MakeScalarImage(value);
}

void Material::setRoughness(const Image& image, const i32 channel) {
  m_roughnessMap = ToGray(image, channel, "Roughness");
}

f32 Material::getRoughnessValue() const noexcept { return FirstValue(m_roughnessMap); }

bool Material::isRoughnessConstant() const noexcept { return IsConstant(m_roughnessMap); }

// ─── Ambient Occlusion ───────────────────────────────────────────────────────

void Material::setAmbientOcclusion(const f32 value) {
  m_ambientOcclusionMap = MakeScalarImage(value);
}

void Material::setAmbientOcclusion(const Image& image, const i32 channel) {
  m_ambientOcclusionMap = ToGray(image, channel, "AmbientOcclusion");
}

f32 Material::getAmbientOcclusionValue() const noexcept { return FirstValue(m_ambientOcclusionMap); }

bool Material::isAmbientOcclusionConstant() const noexcept { return IsConstant(m_ambientOcclusionMap); }

// ─── Normale ─────────────────────────────────────────────────────────────────

void Material::setNormal(const Image& image) {
  m_normalMap = ToRgb(image, "Normal");
}

void Material::setNormal(const vec3& normal) {
  const f32 length = glm::length(normal);
  const vec3 unit = length > 0.0f ? normal / length : vec3(0.0f, 0.0f, 1.0f);

  m_normalMap = MakeNormalImage(unit);
}

bool Material::isNormalConstant() const noexcept { return IsConstant(m_normalMap); }

// ─── Height ──────────────────────────────────────────────────────────────────

void Material::setHeight(const f32 value) {
  m_heightMap = MakeScalarImage(value);
}

void Material::setHeight(const Image& image, const i32 channel) {
  m_heightMap = ToGray(image, channel, "Height");
}

f32 Material::getHeightValue() const noexcept { return FirstValue(m_heightMap); }

bool Material::isHeightConstant() const noexcept { return IsConstant(m_heightMap); }

// ─── Emissive ────────────────────────────────────────────────────────────────

void Material::setEmissive(const Color& color) {
  m_emissiveMap = MakeColorImage(color);
}

void Material::setEmissive(const Image& image) {
  m_emissiveMap = ToRgb(image, "Emissive");
}

bool Material::isEmissiveConstant() const noexcept { return IsConstant(m_emissiveMap); }

}  // namespace sgl
