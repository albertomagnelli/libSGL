#pragma once
// sgl · Sprite
// 2D-Bild mit Textur (GPU), Deckungsform (CPU) und Darstellungsdaten.
//
//   Sprite sprite = LoadSpriteFromImage(ImageIO::Load("figur.png"));
//   DrawSprite(sprite, {x, y}, 45.0f);   // Winkel in Grad, positiv gegen den Uhrzeigersinn
//
// Die Kollisionsform wird einmalig aus dem Alphakanal gebaut und liegt in
// lokalen Koordinaten -- sie ist damit unabhaengig von der Rotation des Sprites
// (siehe CollisionShape). Kollisionen funktionieren auch ohne OpenGL, Zeichnen
// braucht einen Kontext.
//
// Bezugspunkt: origin (0..1, Standard Mitte) gilt fuer Zeichnen und Kollision
// gleichermassen; die Position ist damit der Punkt, um den gedreht wird.

#include "CollisionShape.hpp"
#include "Color.hpp"
#include "Image.hpp"
#include "Texture.hpp"
#include "Types.hpp"

namespace sgl {

class Sprite {
  public:
    Sprite() = default;
    ~Sprite();

    Sprite(const Sprite&)            = delete;
    Sprite& operator=(const Sprite&) = delete;
    Sprite(Sprite&& other) noexcept;
    Sprite& operator=(Sprite&& other) noexcept;

    // ----- Laden ------------------------------------------------------------
    // Pixel bleiben im Speicher (fuer die Deckungsform); smooth = Filterung
    bool loadFromImage(const Image& image, bool smooth = true, u8 collisionThreshold = 8);
    bool loadFromTexture(const Texture2d& texture, bool smooth = true, u8 collisionThreshold = 8);

    // Textur uebernehmen, ohne Pixel zu lesen und ohne Deckungsform zu bauen --
    // fuer Atlanten/Animationen, die ihre Form selbst verwalten (AnimatedSprite).
    bool loadFromTexture(Texture2d&& texture, i32 width = 0, i32 height = 0);

    void unload();

    // ----- Atlas ------------------------------------------------------------
    // Ausschnitt in der Textur; die Deckungsform wird fuer diesen Ausschnitt gebaut
    // Ausschnitt setzen; rebuild = false laesst die Deckungsform unangetastet
    // (fuer Animationsrahmen, deren Form vorberechnet ist)
    bool              setSourceRect(i32 x, i32 y, i32 width, i32 height, bool rebuild = true);
    void              resetSourceRect();
    [[nodiscard]] i32 sourceX() const;
    [[nodiscard]] i32 sourceY() const;

    // ----- Darstellung ------------------------------------------------------
    void                       setOrigin(const vec2& normalized); // 0..1, Standard (0.5, 0.5)
    [[nodiscard]] vec2         origin() const;
    void                       setTint(const Color& tint);
    [[nodiscard]] const Color& tint() const;
    void                       setFlip(bool flipX, bool flipY);
    [[nodiscard]] bool         flipX() const;
    [[nodiscard]] bool         flipY() const;
    void                       setSmooth(bool smooth);

    // ----- Abfragen ---------------------------------------------------------
    [[nodiscard]] bool                  isValid() const;  // zeichenbar (Textur vorhanden)
    [[nodiscard]] bool                  hasShape() const; // Deckungsform vorhanden (auch ohne Kontext)
    [[nodiscard]] i32                   width() const;    // Breite in Pixeln
    [[nodiscard]] i32                   height() const;
    [[nodiscard]] vec2                  size() const;
    [[nodiscard]] const Texture2d&      texture() const;
    [[nodiscard]] const CollisionShape& shape() const;
    [[nodiscard]] const Image&          pixels() const; // Quellpixel (kann leer sein)

    // ----- Kollision --------------------------------------------------------
    // Punkt (Welt) liegt im deckenden Bereich
    [[nodiscard]] bool containsPoint(const vec2& worldPoint, const vec2& position, f32 rotationDegrees) const;
    // zwei Sprites ueberlappen sich pixelgenau
    [[nodiscard]] bool collidesWith(const Sprite& other, const vec2& position, f32 rotationDegrees,
                                    const vec2&   otherPosition,
                                    f32           otherRotationDegrees) const;

  private:
    void moveFrom(Sprite& other);
    void rebuildShape();

    Texture2d      m_texture;
    Image          m_pixels; // Quellbild (nur wenn aus Image geladen)
    CollisionShape m_shape;

    i32 m_sourceX{};
    i32 m_sourceY{};
    i32 m_sourceWidth{};
    i32 m_sourceHeight{};

    vec2  m_origin{0.5f, 0.5f};
    Color m_tint{255, 255, 255, 255};
    bool  m_flipX{};
    bool  m_flipY{};
    bool  m_smooth{true};
    u8    m_threshold{8};
};

// Freie Funktionen: Laden und Zeichnen.
[[nodiscard]] Sprite LoadSpriteFromImage(const Image& image);
[[nodiscard]] Sprite LoadSpriteFromTexture(const Texture2d& texture);
void DrawSprite(const Sprite& sprite, vec2 position, f32 rotationAngle);

}  // namespace sgl

