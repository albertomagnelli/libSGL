#pragma once
// sgl · AnimatedSprite
// Animierte 2D-Figur: Atlas mit mehreren Rahmen, benannte Animationen,
// einstellbares Tempo und pixelgenaue Kollision pro Rahmen.
//
//   AnimatedSprite walker = LoadAnimatedSprite("crimson-robe.png");  // JSON daneben
//   walker.play("laufen");
//   walker.setSpeed(0.5f);        // halbe Geschwindigkeit
//   walker.update(GetFrameTime());
//   DrawAnimatedSprite(walker, {x, y}, winkel);
//
// Metadaten: Aseprite-Export (frames als Objekt oder Array, frameTags mit
// direction forward/reverse/pingpong, duration in Millisekunden) sowie die
// flache Variante {"x","y","w","h","duration"} je Rahmen.
//
// Die Deckungsform wird beim Laden fuer JEDEN Rahmen einmal gebaut (aus dem
// Alphakanal des Blattes) und beim Abspielen nur ausgewaehlt -- ein Rahmenwechsel
// kostet daher nichts. Kollisionen laufen ueber AnimatedSprite::collidesWith();
// getFrame() liefert das Sprite nur zum Zeichnen.

#include <string>
#include <string_view>
#include <vector>

#include "CollisionShape.hpp"
#include "Image.hpp"
#include "Sprite.hpp"
#include "Types.hpp"

namespace sgl {

enum class AnimationDirection {
  Forward = 0,
  Reverse,
  PingPong,
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
  AnimationDirection         direction{AnimationDirection::Forward};
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
  bool loadFromFile(const std::string &imagePath, const std::string &jsonPath = std::string());

  // Atlas selbst beschreiben
  bool loadFromImage(const Image &sheet, std::vector<Animation> animations, u8 collisionThreshold = 8);

  // Gleich grosse Rahmen in Zeilenreihenfolge
  bool loadFromGrid(const Image &sheet, i32 frameWidth, i32 frameHeight, f32 frameDuration = 0.1f,
                    u8 collisionThreshold = 8);

  void unload();

  [[nodiscard]] bool isValid() const;   // Animationen vorhanden (auch ohne GL-Kontext)
  [[nodiscard]] bool hasTexture() const; // Textur vorhanden (Zeichnen moeglich)
  [[nodiscard]] bool hasShape() const;   // Deckungsformen vorhanden

  // ----- Animationen ------------------------------------------------------
  [[nodiscard]] i32                getAnimationCount() const;
  [[nodiscard]] const std::string &getAnimationName(i32 index) const;
  [[nodiscard]] bool               hasAnimation(const std::string &name) const;
  [[nodiscard]] i32                findAnimation(const std::string &name) const; // -1 = unbekannt
  [[nodiscard]] const Animation   *getAnimationAt(i32 index) const;

  // ----- Wiedergabe -------------------------------------------------------
  bool play(const std::string &name, bool restart = false);
  void restart();
  void pause();
  void resume();
  [[nodiscard]] bool isPaused() const;

  // Tempo als Faktor auf die Dauer aus den Metadaten (1.0 = Originalzeit,
  // 0 = angehalten). Gilt fuer alle Animationen ...
  void setSpeed(f32 multiplier);
  [[nodiscard]] f32  getSpeed() const;

  // ... oder nur fuer eine einzelne
  void setAnimationSpeed(const std::string &name, f32 multiplier);
  [[nodiscard]] f32  getAnimationSpeed(const std::string &name) const;
  [[nodiscard]] f32  getEffectiveSpeed() const; // Speed * AnimationSpeed * (Pause ? 0 : 1)

  // Alle Rahmen gleich schnell (ueberschreibt die Dauer aus den Metadaten)
  void setFrameDurationOverride(f32 seconds);
  [[nodiscard]] f32  getFrameDurationOverride() const;

  void setLooping(bool loop);
  [[nodiscard]] bool isLooping() const;

  // Direkt auf einen Rahmen der aktuellen Animation springen
  void setFrameIndex(i32 index);

  void update(f32 deltaTime);

  // ----- Zustand ----------------------------------------------------------
  [[nodiscard]] const std::string &getCurrentAnimation() const;
  [[nodiscard]] i32                getCurrentAnimationIndex() const;
  [[nodiscard]] i32                getFrameIndex() const; // Rahmen innerhalb der Animation
  [[nodiscard]] i32                getStepIndex() const;  // Schritt der Abspielreihenfolge
  [[nodiscard]] i32                getFrameCount() const;
  [[nodiscard]] i32                getTotalFrameCount() const;
  [[nodiscard]] f32                getFrameTime() const;     // Zeit im aktuellen Rahmen (s)
  [[nodiscard]] f32                getCurrentFrameDuration() const; // Dauer dieses Rahmens (s, Speed 1)
  [[nodiscard]] f32                getDuration() const;      // Gesamtdauer der Animation (s, Speed 1)
  [[nodiscard]] f32                getProgress() const;      // 0..1
  [[nodiscard]] bool               isFinished() const;    // nicht schleifende Animation durch

  [[nodiscard]] Sprite              &getFrame();       // nur zum Zeichnen
  [[nodiscard]] const Sprite        &getFrame() const; // nur zum Zeichnen
  [[nodiscard]] const CollisionShape &getShape() const; // Deckungsform des aktuellen Rahmens
  [[nodiscard]] vec2                 getSize() const;   // Groesse des aktuellen Rahmens

  // ----- Darstellung ------------------------------------------------------
  void         setOrigin(const vec2 &normalized); // 0..1, Standard Mitte
  [[nodiscard]] vec2         getOrigin() const;
  void         setTint(const Color &tint);
  [[nodiscard]] const Color &getTint() const;
  void         setSmooth(bool smooth);
  [[nodiscard]] bool         isSmooth() const;

  // ----- Kollision --------------------------------------------------------
  bool containsPoint(const vec2 &worldPoint, const vec2 &position, f32 rotationDegrees) const;
  bool collidesWith(const AnimatedSprite &other, const vec2 &position, f32 rotationDegrees,
                    const vec2 &otherPosition, f32 otherRotationDegrees) const;
  bool collidesWith(const Sprite &other, const vec2 &position, f32 rotationDegrees, const vec2 &otherPosition,
                    f32 otherRotationDegrees) const;

private:
  struct Step {
    i32 frame{0};        // Index in Animation::frames
    f32 duration{0.1f};  // Sekunden
  };

  bool build(const Image &sheet, std::vector<Animation> animations, u8 collisionThreshold);
  void applyFrame();
  void applyOriginToShapes();
  const CollisionShape &currentShape() const;
  const Animation      *current() const;
  const std::vector<Step> &currentSteps() const;
  void moveFrom(AnimatedSprite &other);

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

