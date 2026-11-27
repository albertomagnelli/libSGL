//
// Created by ingsto on 14.11.26.
//
// CollisionShape: Deckungsmaske in lokalen Koordinaten, Abfragen mit Rotation.
//
#include <sgl/CollisionShape.hpp>

#include <algorithm>
#include <cmath>

#include <sgl/Log.hpp>

#include <glm/common.hpp>
#include <glm/geometric.hpp>

namespace sgl {

namespace {
  constexpr f32 DegreesToRadians = 3.14159265358979323846f / 180.0f;

  // Rotationsmatrix (y nach unten, positiver Winkel = gegen den Uhrzeigersinn)
  vec2 Rotate(const vec2 &value, const f32 radians) {
    const f32 c = std::cos(radians);
    const f32 s = std::sin(radians);

    return vec2((value.x * c) + (value.y * s), (-value.x * s) + (value.y * c));
  }

  // Trennachsentest fuer zwei Rechtecke (Ecken in der Welt)
  bool BoxesOverlap(const vec2 a[4], const vec2 b[4]) {
    for (const vec2 *box : {a, b}) {
      // Achsen des jeweiligen Rechtecks
      const vec2 edges[2] = {box[1] - box[0], box[3] - box[0]};

      for (const vec2 &edge : edges) {
        const vec2 axis = glm::normalize(vec2(-edge.y, edge.x));

        f32 minA = glm::dot(axis, a[0]);
        f32 maxA = minA;
        for (i32 i = 1; i < 4; ++i) {
          const f32 projection = glm::dot(axis, a[i]);
          minA = std::min(minA, projection);
          maxA = std::max(maxA, projection);
        }

        f32 minB = glm::dot(axis, b[0]);
        f32 maxB = minB;
        for (i32 i = 1; i < 4; ++i) {
          const f32 projection = glm::dot(axis, b[i]);
          minB = std::min(minB, projection);
          maxB = std::max(maxB, projection);
        }

        if ((maxA < minB) || (maxB < minA)) return false; // trennende Achse
      }
    }

    return true;
  }
}  // namespace

CollisionShape::LocalFrame::LocalFrame(const vec2 &position, const f32 rotationDegrees)
    : m_position(position) {
  const f32 radians = rotationDegrees * DegreesToRadians;

  m_cos = std::cos(radians);
  m_sin = std::sin(radians);
}

vec2 CollisionShape::LocalFrame::ToLocal(const vec2 &world) const {
  const vec2 delta = world - m_position;

  return vec2((delta.x * m_cos) - (delta.y * m_sin), (delta.x * m_sin) + (delta.y * m_cos));
}

vec2 CollisionShape::LocalFrame::ToWorld(const vec2 &local) const {
  return m_position + vec2((local.x * m_cos) + (local.y * m_sin), (-local.x * m_sin) + (local.y * m_cos));
}

vec2 CollisionShape::LocalFrame::Position() const { return m_position; }

CollisionShape CollisionShape::FromAlphaMask(const u8 *mask, const i32 width, const i32 height, const u8 threshold) {
  CollisionShape shape;

  if ((mask == nullptr) || (width <= 0) || (height <= 0)) {
    SLOGE << "CollisionShape: ungueltige Maske (" << width << "x" << height << ")";
    return shape;
  }

  shape.m_width  = width;
  shape.m_height = height;
  shape.m_solid.resize(static_cast<std::size_t>(width) * height);

  for (std::size_t i = 0; i < shape.m_solid.size(); ++i) {
    const bool solid = mask[i] >= threshold;

    shape.m_solid[i] = solid ? 1 : 0;
    if (solid) ++shape.m_opaque;
  }

  shape.m_pivot = vec2(static_cast<f32>(width) * 0.5f, static_cast<f32>(height) * 0.5f);

  return shape;
}

CollisionShape CollisionShape::FromImage(const Image &image, const u8 threshold) {
  if (image.isEmpty()) {
    SLOGE << "CollisionShape: Bild ist ungueltig";
    return {};
  }

  const i32 width    = image.width();
  const i32 height   = image.height();
  const i32 channels = image.channelCount();

  if (channels == 1) {
    // reine Deckungsmaske: die Werte sind bereits die Deckung
    return FromAlphaMask(image.data(), width, height, threshold);
  }

  // Alpha-Kanal herausschneiden; ohne Alpha (3 Kanaele) gilt alles als deckend
  std::vector<u8> mask(static_cast<std::size_t>(width) * height, 255);

  if ((channels == 4) || (channels == 2)) {
    const u8 *pixels = image.data();
    const auto stride = static_cast<std::size_t>(channels);

    for (std::size_t i = 0; i < mask.size(); ++i) mask[i] = pixels[(i * stride) + (stride - 1)];
  }

  return FromAlphaMask(mask.data(), width, height, threshold);
}

CollisionShape CollisionShape::FromImage(const Image &image, const u8 threshold, const i32 x, const i32 y,
                                         const i32 width, const i32 height) {
  if (image.isEmpty()) {
    SLOGE << "CollisionShape: Bild ist ungueltig";

    return {};
  }

  if ((width <= 0) || (height <= 0) || (x < 0) || (y < 0) || ((x + width) > image.width()) ||
      ((y + height) > image.height())) {
    SLOGE << "CollisionShape: Ausschnitt " << width << "x" << height << " bei " << x << "," << y << " passt nicht in "
          << image.width() << "x" << image.height();

    return {};
  }

  const i32 channels = image.channelCount();
  const u8 *pixels   = image.data();

  std::vector<u8> mask(static_cast<std::size_t>(width) * height, 255);

  if ((channels == 4) || (channels == 2)) {
    const i32 alpha = channels - 1;

    for (i32 row = 0; row < height; ++row) {
      const u8 *source = pixels + ((static_cast<std::size_t>(y + row) * image.width() + x) * channels);

      for (i32 column = 0; column < width; ++column) {
        mask[(static_cast<std::size_t>(row) * width) + column] = source[(column * channels) + alpha];
      }
    }
  } else if (channels != 1) {
    // RGB ohne Alpha: alles deckend
  } else {
    for (i32 row = 0; row < height; ++row) {
      const u8 *source = pixels + (static_cast<std::size_t>(y + row) * image.width() + x);

      std::copy_n(source, static_cast<std::size_t>(width), mask.begin() + (static_cast<std::size_t>(row) * width));
    }
  }

  return FromAlphaMask(mask.data(), width, height, threshold);
}

bool CollisionShape::IsValid() const { return (m_width > 0) && (m_height > 0) && !m_solid.empty(); }

i32 CollisionShape::Width() const { return m_width; }

i32 CollisionShape::Height() const { return m_height; }

i32 CollisionShape::OpaqueCount() const { return m_opaque; }

f32 CollisionShape::Coverage() const {
  if (!IsValid()) return 0.0f;

  return static_cast<f32>(m_opaque) / static_cast<f32>(static_cast<std::size_t>(m_width) * m_height);
}

void CollisionShape::SetPivot(const vec2 &pivot) { m_pivot = pivot; }

const vec2 &CollisionShape::Pivot() const { return m_pivot; }

vec2 CollisionShape::BoundsMin() const { return -m_pivot; }

vec2 CollisionShape::BoundsMax() const { return vec2(static_cast<f32>(m_width), static_cast<f32>(m_height)) - m_pivot; }

f32 CollisionShape::BoundingRadius() const {
  const vec2 min = BoundsMin();
  const vec2 max = BoundsMax();

  // groesster Abstand Pivot -> Ecke
  return std::max(glm::length(min), glm::length(max));
}

bool CollisionShape::SolidAtPixel(const i32 x, const i32 y) const {
  if ((x < 0) || (y < 0) || (x >= m_width) || (y >= m_height)) return false;

  return m_solid[(static_cast<std::size_t>(y) * m_width) + x] != 0;
}

bool CollisionShape::IsSolidAt(const i32 x, const i32 y) const { return SolidAtPixel(x, y); }

bool CollisionShape::ContainsPoint(const vec2 &local) const {
  if (!IsValid()) return false;

  // lokale Koordinate -> Pixelindex (Ursprung = Pivot)
  const auto x = static_cast<i32>(std::floor(local.x + m_pivot.x));
  const auto y = static_cast<i32>(std::floor(local.y + m_pivot.y));

  return SolidAtPixel(x, y);
}

vec2 CollisionShape::LocalToWorld(const vec2 &local, const vec2 &position, const f32 rotationDegrees) {
  return position + Rotate(local, rotationDegrees * DegreesToRadians);
}

vec2 CollisionShape::WorldToLocal(const vec2 &world, const vec2 &position, const f32 rotationDegrees) {
  return Rotate(world - position, -rotationDegrees * DegreesToRadians);
}

bool CollisionShape::ContainsWorldPoint(const vec2 &worldPoint, const vec2 &position,
                                        const f32 rotationDegrees) const {
  return ContainsPoint(WorldToLocal(worldPoint, position, rotationDegrees));
}

bool CollisionShape::ContainsWorldPoint(const vec2 &worldPoint, const LocalFrame &frame) const {
  return ContainsPoint(frame.ToLocal(worldPoint));
}

void CollisionShape::WorldCorners(const LocalFrame &frame, vec2 corners[4]) const {
  const vec2 min = BoundsMin();
  const vec2 max = BoundsMax();

  corners[0] = frame.ToWorld(vec2(min.x, min.y));
  corners[1] = frame.ToWorld(vec2(max.x, min.y));
  corners[2] = frame.ToWorld(vec2(max.x, max.y));
  corners[3] = frame.ToWorld(vec2(min.x, max.y));
}

void CollisionShape::WorldCorners(const vec2 &position, const f32 rotationDegrees, vec2 corners[4]) const {
  const vec2 min = BoundsMin();
  const vec2 max = BoundsMax();

  corners[0] = LocalToWorld(vec2(min.x, min.y), position, rotationDegrees);
  corners[1] = LocalToWorld(vec2(max.x, min.y), position, rotationDegrees);
  corners[2] = LocalToWorld(vec2(max.x, max.y), position, rotationDegrees);
  corners[3] = LocalToWorld(vec2(min.x, max.y), position, rotationDegrees);
}

bool CollisionShape::HasSolidPixelIn(const CollisionShape &otherShape, const LocalFrame &frame,
                                     const LocalFrame &otherFrame) const {
  // Nur den Teil der eigenen Maske abtasten, der in der Huelle der anderen Form
  // liegt: deren vier Weltecken in den eigenen lokalen Raum abbilden.
  vec2 otherCorners[4];
  otherShape.WorldCorners(otherFrame, otherCorners);

  vec2 minLocal = frame.ToLocal(otherCorners[0]);
  vec2 maxLocal = minLocal;

  for (i32 i = 1; i < 4; ++i) {
    const vec2 corner = frame.ToLocal(otherCorners[i]);

    minLocal = glm::min(minLocal, corner);
    maxLocal = glm::max(maxLocal, corner);
  }

  // lokale Pixelkoordinaten (Ursprung = Pivot), ein Pixel Rand
  const i32 startX = std::max(0, static_cast<i32>(std::floor(minLocal.x + m_pivot.x)) - 1);
  const i32 endX   = std::min(m_width - 1, static_cast<i32>(std::ceil(maxLocal.x + m_pivot.x)) + 1);
  const i32 startY = std::max(0, static_cast<i32>(std::floor(minLocal.y + m_pivot.y)) - 1);
  const i32 endY   = std::min(m_height - 1, static_cast<i32>(std::ceil(maxLocal.y + m_pivot.y)) + 1);

  if ((startX > endX) || (startY > endY)) return false;

  for (i32 y = startY; y <= endY; ++y) {
    for (i32 x = startX; x <= endX; ++x) {
      if (!SolidAtPixel(x, y)) continue;

      // Pixelmittelpunkt in lokalen Koordinaten (Ursprung = Pivot) -> Welt
      const vec2 local(static_cast<f32>(x) + 0.5f - m_pivot.x, static_cast<f32>(y) + 0.5f - m_pivot.y);
      const vec2 world = frame.ToWorld(local);

      if (otherShape.ContainsWorldPoint(world, otherFrame)) return true;
    }
  }

  return false;
}

bool CollisionShape::Overlaps(const CollisionShape &other, const vec2 &position, const f32 rotationDegrees,
                              const vec2 &otherPosition, const f32 otherRotationDegrees) const {
  if (!IsValid() || !other.IsValid()) return false;

  // 1. Umkreis: billige Abweisung
  const f32 distance = glm::length(otherPosition - position);

  if (distance > (BoundingRadius() + other.BoundingRadius())) return false;

  // 2. gedrehte Bounding Boxes (Drehungen einmal vorberechnen)
  const LocalFrame frame(position, rotationDegrees);
  const LocalFrame otherFrame(otherPosition, otherRotationDegrees);

  vec2 corners[4];
  vec2 otherCorners[4];

  WorldCorners(frame, corners);
  other.WorldCorners(otherFrame, otherCorners);

  if (!BoxesOverlap(corners, otherCorners)) return false;

  // 3. pixelweise: liegt ein deckender Pixel(mittelpunkt) der einen Form in der
  //    anderen? Beide Richtungen, damit die Pruefung symmetrisch bleibt.
  if (HasSolidPixelIn(other, frame, otherFrame)) return true;

  return other.HasSolidPixelIn(*this, otherFrame, frame);
}

}  // namespace sgl
