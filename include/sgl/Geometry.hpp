#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// Rectangle mit Kollision und Drehung. Öffentliche Schnittstelle dieses Headers:
//
//   Rectangle, position, size, center, left
//   top, right, bottom, isEmpty, contains
//   containsStrict, overlaps, intersection, united, overlapArea
//   equals, containsRotated, overlapsRotated, cornerRotated, rotate
//   RectangleFromCorners, RectangleFromSize
//
// Beispiel:
//
//   Rectangle box{10, 10, 100, 60};
//   bool hit = box.contains({50.0f, 40.0f});
//   Rectangle cut = box.intersection(other);
// ─────────────────────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────────────────────
// sgl · Geometry
//
// Rechtecktyp der Bibliothek (x, y, width, height in Pixeln) und Helfer dazu.
// Die Implementierung liegt in Geometry.cpp.
//
// Koordinaten sind Pixelzentren: (0,0) ist die Mitte des linken oberen Pixels,
// y zeigt nach unten. Ein Rectangle deckt mit width/height genau width*height
// Pixel ab, also x .. x+width-1.
//
// Konventionen: Methoden camelCase, freie Funktionen mit Großbuchstaben,
// bool-Getter mit "is", Member "m_".
//
// Beispiel:
//
//   const Rectangle rahmen = RectangleFromCorners({20, 10}, {80, 50});
//   const bool hit = rahmen.contains({50.0f, 40.0f});
// ─────────────────────────────────────────────────────────────────────────────

#include "Types.hpp"

namespace sgl {

// Rechteck in Pixeln (wie in raylib): linke obere Ecke plus Größe
struct Rectangle {
    f32 x{0.0f};
    f32 y{0.0f};
    f32 width{0.0f};
    f32 height{0.0f};

    constexpr Rectangle() = default;
    // nicht constexpr, weil die Definition in Geometry.cpp liegt
    Rectangle(f32 positionX, f32 positionY, f32 rectangleWidth, f32 rectangleHeight);

    [[nodiscard]] vec2 position() const;
    [[nodiscard]] vec2 size() const;
    [[nodiscard]] vec2 center() const;

    [[nodiscard]] f32 left() const;
    [[nodiscard]] f32 top() const;
    [[nodiscard]] f32 right() const;
    [[nodiscard]] f32 bottom() const;

    // kein Inhalt (Breite oder Höhe null/negativ)
    [[nodiscard]] bool isEmpty() const;

    // ── Kollision ────────────────────────────────────────────────────────────
    // Punkt im Rechteck; der Rand zählt mit
    [[nodiscard]] bool contains(const vec2& point) const;
    // Punkt echt innerhalb; der Rand zählt nicht
    [[nodiscard]] bool containsStrict(const vec2& point) const;
    // anderes Rechteck liegt vollständig darin (Rand darf anliegen)
    [[nodiscard]] bool contains(const Rectangle& other) const;
    // Überschneidung zweier achsenparalleler Rechtecke; reines Berühren an einer
    // Kante zählt nicht als Überschneidung
    [[nodiscard]] bool overlaps(const Rectangle& other) const;
    // Schnittfläche; leeres Rechteck, wenn es keine gibt
    [[nodiscard]] Rectangle intersection(const Rectangle& other) const;
    // kleinstes Rechteck, das beide umschließt
    [[nodiscard]] Rectangle united(const Rectangle& other) const;
    // Flächeninhalt der Überschneidung in Pixeln
    [[nodiscard]] f32 overlapArea(const Rectangle& other) const;
    // gleiche Lage und Größe (mit Toleranz, weil f32 gerechnet wird)
    [[nodiscard]] bool equals(const Rectangle& other, f32 tolerance = 1e-4f) const;

    // ── gedrehte Rechtecke ───────────────────────────────────────────────────
    // Punkt im um "origin" (relativ zur linken oberen Ecke) gedrehten Rechteck
    [[nodiscard]] bool containsRotated(const vec2& point, const vec2& origin,
                                       f32 rotationDegrees) const;
    // Überschneidung zweier gedrehter Rechtecke (Trennachsen-Test, SAT).
    // Beide Rechtecke werden um ihren eigenen Ursprung gedreht.
    [[nodiscard]] static bool overlapsRotated(const Rectangle& a, const vec2& aOrigin,
                                              f32 aRotation, const Rectangle& b,
                                              const vec2& bOrigin, f32 bRotation);
    // Ecke 0..3 (0 = links oben, dann im Uhrzeigersinn im Bildschirmsystem)
    [[nodiscard]] vec2 cornerRotated(i32 index, const vec2& origin, f32 rotationDegrees) const;
    // Drehung um den Ursprung (Grad, positive Werte gegen den Uhrzeigersinn im
    // mathematischen Sinn – im Bildschirmsystem also im Uhrzeigersinn)
    [[nodiscard]] static vec2 rotate(const vec2& point, f32 degrees);
};

// Rechteck aus zwei beliebigen Eckpunkten; die Ecken werden sortiert, sodass
// width/height nie negativ sind.
[[nodiscard]] Rectangle RectangleFromCorners(const vec2& first, const vec2& second);

// Rechteck aus Position und Größe (vec2-Fassung von Rectangle{x, y, w, h}).
[[nodiscard]] Rectangle RectangleFromSize(const vec2& position, const vec2& size);

}  // namespace sgl
