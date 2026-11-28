#pragma once
// sgl · CollisionShape
// Pixelgenaue Kollisionsform aus dem Alphakanal eines Bildes.
//
// Die Deckungsmaske liegt einmalig in LOKALEN Koordinaten (Ursprung = Pivot,
// x nach rechts, y nach unten). Abfragen rechnen den Punkt in diesen lokalen
// Raum zurueck -- die Form selbst wird nie gedreht und bleibt damit unabhaengig
// von der Rotation des Sprites:
//
//   local = Rot(-winkel) * (welt - position)
//
// Ueberlappungen werden zunaechst mit Umkreis und Oriented Bounding Box
// abgewiesen und danach pixelweise entschieden.

#include <vector>

#include "Image.hpp"
#include "Types.hpp"

namespace sgl {

class CollisionShape {
public:
  // Vorberechnete Drehung: sin/cos entstehen einmal, nicht pro Abfragepunkt.
  // Bei pixelgenauen Tests mit hunderten Punkten ist das der groesste Gewinn.
  class LocalFrame {
  public:
    LocalFrame() = default;
    LocalFrame(const vec2 &position, f32 rotationDegrees);

    [[nodiscard]] vec2 toLocal(const vec2 &world) const;
    [[nodiscard]] vec2 toWorld(const vec2 &local) const;
    [[nodiscard]] vec2 getPosition() const;

  private:
    vec2 m_position{0.0f};
    f32  m_cos{1.0f};
    f32  m_sin{0.0f};
  };

  CollisionShape() = default;

  // Deckungsmaske (8 Bit, 1 Kanal) uebernehmen; Schwelle trennt deckend/leer
  static CollisionShape FromAlphaMask(const u8 *mask, i32 width, i32 height, u8 threshold = 8);

  // Alpha-Kanal eines Bildes (1 und 3 Kanaele gelten als voll deckend)
  static CollisionShape FromImage(const Image &image, u8 threshold = 8);

  // Ausschnitt eines Bildes (Atlas-Rahmen): x/y in Pixeln, sonst wie oben
  static CollisionShape FromImage(const Image &image, u8 threshold, i32 x, i32 y, i32 width, i32 height);

  [[nodiscard]] bool isValid() const;
  [[nodiscard]] i32  getWidth() const;
  [[nodiscard]] i32  getHeight() const;

  [[nodiscard]] i32 getOpaqueCount() const;  // Anzahl deckender Pixel
  [[nodiscard]] f32 getCoverage() const;     // Anteil deckender Pixel (0..1)

  // Pivot in Pixelkoordinaten der Maske (Standard: Mitte)
  void                       setPivot(const vec2 &pivot);
  [[nodiscard]] const vec2 &getPivot() const;

  [[nodiscard]] vec2 getBoundsMin() const; // linke/obere Kante relativ zum Pivot
  [[nodiscard]] vec2 getBoundsMax() const; // rechte/untere Kante relativ zum Pivot
  [[nodiscard]] f32  getBoundingRadius() const;

  // Abfragen in lokalen Koordinaten (Ursprung = Pivot, y nach unten)
  [[nodiscard]] bool isSolidAt(i32 x, i32 y) const;
  [[nodiscard]] bool containsPoint(const vec2 &local) const;

  // Punkt in der Welt, Sprite liegt bei position und ist um rotationDegrees gedreht
  // (positiv = gegen den Uhrzeigersinn)
  [[nodiscard]] bool containsWorldPoint(const vec2 &worldPoint, const vec2 &position, f32 rotationDegrees) const;
  [[nodiscard]] bool containsWorldPoint(const vec2 &worldPoint, const LocalFrame &frame) const;

  // Ueberlappung zweier Formen in Weltlage
  [[nodiscard]] bool overlaps(const CollisionShape &other, const vec2 &position, f32 rotationDegrees, const vec2 &otherPosition,
                f32 otherRotationDegrees) const;

  // Umrechnung zwischen lokalem und Weltraum (gleiche Winkelkonvention)
  static vec2 LocalToWorld(const vec2 &local, const vec2 &position, f32 rotationDegrees);
  static vec2 WorldToLocal(const vec2 &world, const vec2 &position, f32 rotationDegrees);

  // vier Eckpunkte der (gedrehten) Bounding Box in der Welt
  void worldCorners(const vec2 &position, f32 rotationDegrees, vec2 corners[4]) const;
  void worldCorners(const LocalFrame &frame, vec2 corners[4]) const;

private:
  [[nodiscard]] bool solidAtPixel(i32 x, i32 y) const;

  // Liegt ein deckender Pixel dieser Form (als Weltpunkt) in der anderen Form?
  [[nodiscard]] bool hasSolidPixelIn(const CollisionShape &otherShape, const LocalFrame &frame, const LocalFrame &otherFrame) const;

  std::vector<u8> m_solid; // 1 = deckend, 0 = leer (Breite * Hoehe)
  i32             m_width{};
  i32             m_height{};
  i32             m_opaque{};
  vec2            m_pivot{0.0f};
};

}  // namespace sgl
