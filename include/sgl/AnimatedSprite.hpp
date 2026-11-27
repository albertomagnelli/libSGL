#pragma once
// sgl · AnimatedSprite
// Animierte 2D-Figur: Atlas mit mehreren Rahmen, benannte Animationen,
// einstellbares Tempo und pixelgenaue Kollision pro Rahmen.
//
//   AnimatedSprite walker = LoadAnimatedSprite("crimson-robe.png");  // JSON daneben
//   walker.Play("laufen");
//   walker.SetSpeed(0.5f);        // halbe Geschwindigkeit
//   walker.Update(GetFrameTime());
//   DrawAnimatedSprite(walker, {x, y}, winkel);
//
// Metadaten: Aseprite-Export (frames als Objekt oder Array, frameTags mit
// direction forward/reverse/pingpong, duration in Millisekunden) sowie die
// flache Variante {"x","y","w","h","duration"} je Rahmen.
//
// Die Deckungsform wird beim Laden fuer JEDEN Rahmen einmal gebaut (aus dem
// Alphakanal des Blattes) und beim Abspielen nur ausgewaehlt -- ein Rahmenwechsel
// kostet daher nichts. Kollisionen laufen ueber AnimatedSprite::CollidesWith();
// Frame() liefert das Sprite nur zum Zeichnen.

#include <string>
#include <string_view>
#include <vector>

#include "CollisionShape.hpp"
#include "Image.hpp"
#include "Sprite.hpp"
#include "Types.hpp"

namespace sgl {

enum AnimationDirection {
  ANIMATION_FORWARD = 0,
  ANIMATION_REVERSE,
  ANIMATION_PINGPONG,
};

struct AnimationFrame {
  i32 x{};
  i32 y{};
  i32 width{};
  i32 height{};
  f32 duration{0.1f}; // Sekunden
};

struct Animation {
  std::string                name;
  std::vector<AnimationFrame> frames;
  bool                       loop{true};
  AnimationDirection         direction{ANIMATION_FORWARD};
};

class AnimatedSprite {
public:
  AnimatedSprite() = default;
  ~AnimatedSprite();

  AnimatedSprite(const AnimatedSprite &)            = delete;
  AnimatedSprite &operator=(const AnimatedSprite &) = delete;
  AnimatedSprite(AnimatedSprite &&other) noexcept;
  AnimatedSprite &operator=(AnimatedSprite &&other) noexcept;

  // ----- Laden ------------------------------------------------------------
  // Bild laden; jsonPath leer = gleicher Name mit .json, sonst eine Animation
  // namens "default" mit allen Rahmen
  bool LoadFromFile(const std::string &imagePath, const std::string &jsonPath = std::string());

  // Atlas selbst beschreiben
  bool LoadFromImage(const Image &sheet, std::vector<Animation> animations, u8 collisionThreshold = 8);

  // Gleich grosse Rahmen in Zeilenreihenfolge
  bool LoadFromGrid(const Image &sheet, i32 frameWidth, i32 frameHeight, f32 frameDuration = 0.1f,
                    u8 collisionThreshold = 8);

  void Unload();

  bool IsValid() const;   // Animationen vorhanden (auch ohne GL-Kontext)
  bool HasTexture() const; // Textur vorhanden (Zeichnen moeglich)
  bool HasShape() const;   // Deckungsformen vorhanden

  // ----- Animationen ------------------------------------------------------
  i32                AnimationCount() const;
  const std::string &AnimationName(i32 index) const;
  bool               HasAnimation(const std::string &name) const;
  i32                FindAnimation(const std::string &name) const; // -1 = unbekannt
  const Animation   *AnimationAt(i32 index) const;

  // ----- Wiedergabe -------------------------------------------------------
  bool Play(const std::string &name, bool restart = false);
  void Restart();
  void Pause();
  void Resume();
  bool IsPaused() const;

  // Tempo als Faktor auf die Dauer aus den Metadaten (1.0 = Originalzeit,
  // 0 = angehalten). Gilt fuer alle Animationen ...
  void SetSpeed(f32 multiplier);
  f32  Speed() const;

  // ... oder nur fuer eine einzelne
  void SetAnimationSpeed(const std::string &name, f32 multiplier);
  f32  AnimationSpeed(const std::string &name) const;
  f32  EffectiveSpeed() const; // Speed * AnimationSpeed * (Pause ? 0 : 1)

  // Alle Rahmen gleich schnell (ueberschreibt die Dauer aus den Metadaten)
  void SetFrameDuration(f32 seconds);
  f32  FrameDurationOverride() const;

  void SetLooping(bool loop);
  bool IsLooping() const;

  // Direkt auf einen Rahmen der aktuellen Animation springen
  void SetFrame(i32 index);

  void Update(f32 deltaTime);

  // ----- Zustand ----------------------------------------------------------
  const std::string &CurrentAnimation() const;
  i32                CurrentAnimationIndex() const;
  i32                FrameIndex() const; // Rahmen innerhalb der Animation
  i32                StepIndex() const;  // Schritt der Abspielreihenfolge
  i32                FrameCount() const;
  i32                TotalFrameCount() const;
  f32                FrameTime() const;     // Zeit im aktuellen Rahmen (s)
  f32                CurrentFrameDuration() const; // Dauer dieses Rahmens (s, Speed 1)
  f32                Duration() const;      // Gesamtdauer der Animation (s, Speed 1)
  f32                Progress() const;      // 0..1
  bool               IsFinished() const;    // nicht schleifende Animation durch

  Sprite              &Frame();       // nur zum Zeichnen
  const Sprite        &Frame() const; // nur zum Zeichnen
  const CollisionShape &Shape() const; // Deckungsform des aktuellen Rahmens
  vec2                 Size() const;   // Groesse des aktuellen Rahmens

  // ----- Darstellung ------------------------------------------------------
  void         SetOrigin(const vec2 &normalized); // 0..1, Standard Mitte
  vec2         Origin() const;
  void         SetTint(const Color &tint);
  const Color &Tint() const;
  void         SetSmooth(bool smooth);

  // ----- Kollision --------------------------------------------------------
  bool ContainsPoint(const vec2 &worldPoint, const vec2 &position, f32 rotationDegrees) const;
  bool CollidesWith(const AnimatedSprite &other, const vec2 &position, f32 rotationDegrees,
                    const vec2 &otherPosition, f32 otherRotationDegrees) const;
  bool CollidesWith(const Sprite &other, const vec2 &position, f32 rotationDegrees, const vec2 &otherPosition,
                    f32 otherRotationDegrees) const;

private:
  struct Step {
    i32 frame{0};        // Index in Animation::frames
    f32 duration{0.1f};  // Sekunden
  };

  bool Build(const Image &sheet, std::vector<Animation> animations, u8 collisionThreshold);
  void ApplyFrame();
  void ApplyOriginToShapes();
  const CollisionShape &CurrentShape() const;
  const Animation      *Current() const;
  const std::vector<Step> &CurrentSteps() const;
  void MoveFrom(AnimatedSprite &other);

  Sprite                               m_sprite; // Textur + aktueller Ausschnitt
  std::vector<Animation>               m_animations;
  std::vector<std::vector<Step>>       m_steps;     // Abspielreihenfolge je Animation
  std::vector<std::vector<CollisionShape>> m_shapes;   // je Animation je Rahmen
  std::vector<f32>                     m_animationSpeeds;

  i32  m_animationIndex{0};
  i32  m_stepIndex{0};
  f32  m_stepTime{0.0f};
  f32  m_speed{1.0f};
  f32  m_frameDuration{0.0f}; // 0 = aus den Metadaten
  bool m_paused{false};
  bool m_finished{false};
  bool m_smooth{false};
  vec2 m_origin{0.5f, 0.5f};
  u8   m_threshold{8};
  i32  m_sheetWidth{0};
  i32  m_sheetHeight{0};
};

// Freie Funktionen: Laden und Zeichnen.
[[nodiscard]] AnimatedSprite LoadAnimatedSprite(std::string_view imagePath,
                                                std::string_view jsonPath = {});
void DrawAnimatedSprite(const AnimatedSprite& sprite, vec2 position, f32 rotationAngle);

}  // namespace sgl

