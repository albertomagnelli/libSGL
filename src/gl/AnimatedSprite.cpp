//
// Created by ingsto on 14.11.26.
//
// AnimatedSprite: Atlas-Rahmen, benannte Animationen, variables Tempo und
// vorberechnete Deckungsformen (pixelgenaue Kollision je Rahmen).
//
#include <sgl/AnimatedSprite.hpp>

#include <algorithm>
#include <cmath>

#include <sgl/Context.hpp>
#include <sgl/GL.hpp>
#include <sgl/Image.hpp>
#include <sgl/Log.hpp>

#include "common/Json.hpp"

namespace sgl {

using detail::Json;

namespace {
  constexpr f32 MillisecondToSecond = 0.001f;
  constexpr f32 MinimumDuration     = 0.001f; // 1 ms, damit nichts durch Null teilt
  constexpr f32 DefaultDuration     = 0.1f;

  f32 SanitizeDuration(const f32 seconds) { return (seconds >= MinimumDuration) ? seconds : MinimumDuration; }

  // Dateiendung ersetzen ("walker.png" -> "walker.json")
  std::string ReplaceExtension(const std::string &path, const std::string &extension) {
    const auto slash = path.find_last_of("/\\");
    const auto dot   = path.find_last_of('.');

    if ((dot != std::string::npos) && ((slash == std::string::npos) || (dot > slash))) {
      return path.substr(0, dot) + extension;
    }

    return path + extension;
  }

  // Dateiname ohne Verzeichnis und Endung (".../crimson-robe.png" -> "crimson-robe")
  std::string Stem(const std::string &path) {
    const auto slash = path.find_last_of("/\\");
    const auto start = (slash == std::string::npos) ? 0 : (slash + 1);
    const auto dot   = path.find_last_of('.');

    if ((dot != std::string::npos) && (dot > start)) return path.substr(start, dot - start);

    return path.substr(start);
  }

  // Ein Rahmen aus einem Eintrag: {"frame":{x,y,w,h},"duration":ms} oder flach
  AnimationFrame ReadFrame(const Json &entry, const i32 fallbackWidth, const i32 fallbackHeight) {
    const Json *box = entry.find("frame");

    if ((box == nullptr) || !box->isObject()) box = &entry;

    AnimationFrame frame;
    frame.x      = box->find("x") ? box->find("x")->intValue() : 0;
    frame.y      = box->find("y") ? box->find("y")->intValue() : 0;
    frame.width  = box->find("w") ? box->find("w")->intValue() : fallbackWidth;
    frame.height = box->find("h") ? box->find("h")->intValue() : fallbackHeight;

    // Aseprite: Dauer in Millisekunden. Fehlt sie, gilt die Standarddauer.
    const Json *duration = entry.find("duration");
    frame.duration       = (duration != nullptr) ? SanitizeDuration(duration->floatValue(DefaultDuration * 1000.0f) *
                                                                   MillisecondToSecond)
                                                 : DefaultDuration;

    return frame;
  }

  AnimationDirection ReadDirection(const Json *tag) {
    if ((tag == nullptr) || !tag->isString()) return AnimationDirection::Forward;

    const std::string &value = tag->string();

    if (value == "reverse") return AnimationDirection::Reverse;
    if (value == "pingpong") return AnimationDirection::PingPong;

    return AnimationDirection::Forward;
  }
}  // namespace

AnimatedSprite::~AnimatedSprite() { unload(); }

AnimatedSprite::AnimatedSprite(AnimatedSprite &&other) noexcept { moveFrom(other); }

AnimatedSprite &AnimatedSprite::operator=(AnimatedSprite &&other) noexcept {
  if (this == &other) return *this;

  unload();
  moveFrom(other);

  return *this;
}

void AnimatedSprite::moveFrom(AnimatedSprite &other) {
  m_sprite            = std::move(other.m_sprite);
  m_animations        = std::move(other.m_animations);
  m_steps             = std::move(other.m_steps);
  m_shapes            = std::move(other.m_shapes);
  m_animationSpeeds   = std::move(other.m_animationSpeeds);

  m_animationIndex    = other.m_animationIndex;
  m_stepIndex         = other.m_stepIndex;
  m_stepTime          = other.m_stepTime;
  m_speed             = other.m_speed;
  m_frameDuration     = other.m_frameDuration;
  m_paused            = other.m_paused;
  m_finished          = other.m_finished;
  m_smooth            = other.m_smooth;
  m_origin            = other.m_origin;
  m_threshold         = other.m_threshold;
  m_sheetWidth        = other.m_sheetWidth;
  m_sheetHeight       = other.m_sheetHeight;

  other.m_animations.clear();
  other.m_steps.clear();
  other.m_shapes.clear();
  other.m_animationSpeeds.clear();
  other.m_animationIndex = 0;
  other.m_stepIndex      = 0;
}

void AnimatedSprite::unload() {
  m_sprite.unload();
  m_animations.clear();
  m_steps.clear();
  m_shapes.clear();
  m_animationSpeeds.clear();

  m_animationIndex = 0;
  m_stepIndex      = 0;
  m_stepTime       = 0.0f;
  m_finished       = false;
  m_paused         = false;
  m_sheetWidth     = 0;
  m_sheetHeight    = 0;
}

// -----------------------------------------------------------------------------
// Laden
// -----------------------------------------------------------------------------
bool AnimatedSprite::loadFromFile(const std::string &imagePath, const std::string &jsonPath) {
  Image sheet;
  try {
    sheet = ImageIO::Load(imagePath);
  } catch (const std::exception &error) {
    SLOGE << "AnimatedSprite: Bild \"" << imagePath << "\" nicht ladbar (" << error.what() << ")";

    return false;
  }

  if (sheet.isEmpty()) {
    SLOGE << "AnimatedSprite: Bild \"" << imagePath << "\" nicht ladbar";

    return false;
  }

  const std::string metadataPath = jsonPath.empty() ? ReplaceExtension(imagePath, ".json") : jsonPath;

  const Json document = Json::ParseFile(metadataPath);
  if (!document.isValid()) {
    SLOGW << "AnimatedSprite: keine brauchbaren Metadaten (" << document.error()
          << "), nehme das ganze Blatt als eine Animation";

    Animation animation;
    animation.name = Stem(imagePath);

    AnimationFrame frame;
    frame.x        = 0;
    frame.y        = 0;
    frame.width    = sheet.width();
    frame.height   = sheet.height();
    frame.duration = DefaultDuration;

    animation.frames.push_back(frame);

    return build(sheet, {std::move(animation)}, m_threshold);
  }

  // ----- Rahmen -----------------------------------------------------------
  const Json *framesJson = document.find("frames");

  if (framesJson == nullptr) {
    SLOGE << "AnimatedSprite: \"" << metadataPath << "\" enthaelt keine Rahmen";

    return false;
  }

  const i32 fallbackWidth  = 0;
  const i32 fallbackHeight = 0;

  std::vector<AnimationFrame> frames;
  frames.reserve(static_cast<std::size_t>(framesJson->size()));

  if (framesJson->isArray()) {
    for (i32 i = 0; i < framesJson->size(); ++i) {
      frames.push_back(ReadFrame(framesJson->at(i), fallbackWidth, fallbackHeight));
    }
  } else if (framesJson->isObject()) {
    for (i32 i = 0; i < framesJson->size(); ++i) {
      const Json *entry = framesJson->find(framesJson->keyAt(i));

      if (entry == nullptr) continue;

      frames.push_back(ReadFrame(*entry, fallbackWidth, fallbackHeight));
    }
  }

  if (frames.empty()) {
    SLOGE << "AnimatedSprite: keine Rahmen in \"" << metadataPath << "\"";

    return false;
  }

  // ----- Animationen ------------------------------------------------------
  std::vector<Animation> animations;

  const Json *meta = document.find("meta");
  const Json *tags = (meta != nullptr) ? meta->find("frameTags") : nullptr;

  if ((tags != nullptr) && tags->isArray() && (tags->size() > 0)) {
    for (i32 t = 0; t < tags->size(); ++t) {
      const Json &tag = tags->at(t);

      Animation animation;
      animation.name = tag.find("name") ? tag.find("name")->string() : ("animation" + std::to_string(t));
      animation.loop = tag.find("repeat") ? (tag.find("repeat")->intValue() != 0) : true;
      animation.direction = ReadDirection(tag.find("direction"));

      const i32 from = std::clamp(tag.find("from") ? tag.find("from")->intValue() : 0, 0,
                                  static_cast<i32>(frames.size()) - 1);
      const i32 to   = std::clamp(tag.find("to") ? tag.find("to")->intValue() : (static_cast<i32>(frames.size()) - 1),
                                  from, static_cast<i32>(frames.size()) - 1);

      for (i32 i = from; i <= to; ++i) animation.frames.push_back(frames[static_cast<std::size_t>(i)]);

      if (animation.frames.empty()) continue;

      animations.push_back(std::move(animation));
    }
  }

  if (animations.empty()) {
    // keine Tags: alle Rahmen in Dateireihenfolge
    Animation animation;
    animation.name   = Stem(imagePath);
    animation.frames = frames;

    animations.push_back(std::move(animation));
  }

  return build(sheet, std::move(animations), m_threshold);
}

bool AnimatedSprite::loadFromImage(const Image &sheet, std::vector<Animation> animations,
                                   const u8 collisionThreshold) {
  m_threshold = collisionThreshold;

  return build(sheet, std::move(animations), collisionThreshold);
}

bool AnimatedSprite::loadFromGrid(const Image &sheet, const i32 frameWidth, const i32 frameHeight,
                                  const f32 frameDuration, const u8 collisionThreshold) {
  if (sheet.isEmpty() || (frameWidth <= 0) || (frameHeight <= 0)) {
    SLOGE << "AnimatedSprite: ungueltiges Blatt oder Rahmengroesse";

    return false;
  }

  Animation animation;
  animation.name = "default";

  for (i32 y = 0; (y + frameHeight) <= sheet.height(); y += frameHeight) {
    for (i32 x = 0; (x + frameWidth) <= sheet.width(); x += frameWidth) {
      AnimationFrame frame;
      frame.x        = x;
      frame.y        = y;
      frame.width    = frameWidth;
      frame.height   = frameHeight;
      frame.duration = SanitizeDuration(frameDuration);

      animation.frames.push_back(frame);
    }
  }

  if (animation.frames.empty()) return false;

  m_threshold = collisionThreshold;

  return build(sheet, {std::move(animation)}, collisionThreshold);
}

bool AnimatedSprite::build(const Image &sheet, std::vector<Animation> animations, const u8 collisionThreshold) {
  if (sheet.isEmpty() || animations.empty()) {
    SLOGE << "AnimatedSprite: Blatt oder Animationen fehlen";

    return false;
  }

  unload();

  m_threshold = collisionThreshold;

  // Textur aus dem Blatt; ohne GL-Kontext bleiben nur die Deckungsformen
  // (Kollision ja, Zeichnen nein -- wie bei Sprite)
  m_sheetWidth  = sheet.width();
  m_sheetHeight = sheet.height();

  if ((GetCurrentWindow() != nullptr) && (gl::glCreateTextures != nullptr)) {
    try {
      TextureOptions options;
      options.mipmaps   = MipmapMode::None;
      options.isSrgb    = false;
      options.wrapS     = TextureWrap::ClampToEdge;
      options.wrapT     = TextureWrap::ClampToEdge;
      options.minFilter = m_smooth ? TextureFilter::Linear : TextureFilter::Nearest;
      options.magFilter = options.minFilter;

      Texture2d texture = Texture2d::FromImage(sheet, options);
      m_sprite.loadFromTexture(std::move(texture), sheet.width(), sheet.height());
      m_sprite.setOrigin(m_origin);
      m_sprite.setSmooth(m_smooth);
    } catch (const std::exception &error) {
      SLOGW << "AnimatedSprite: Textur laesst sich nicht anlegen (" << error.what() << ", Zeichnen entfaellt)";
    }
  } else {
    SLOGW << "AnimatedSprite: ohne GL-Kontext keine Textur (Kollision funktioniert, Zeichnen nicht)";
  }

  m_animations = std::move(animations);

  for (const Animation &animation : m_animations) {
    for (const AnimationFrame &frame : animation.frames) {
      if ((frame.width <= 0) || (frame.height <= 0) || (frame.x < 0) || (frame.y < 0) ||
          ((frame.x + frame.width) > sheet.width()) || ((frame.y + frame.height) > sheet.height())) {
        SLOGE << "AnimatedSprite: Rahmen " << frame.width << "x" << frame.height << " bei " << frame.x << "," << frame.y
              << " passt nicht ins Blatt " << sheet.width() << "x" << sheet.height();
        m_animations.clear();

        return false;
      }
    }
  }

  m_animationSpeeds.assign(m_animations.size(), 1.0f);

  // Deckungsformen je Rahmen und Abspielreihenfolge je Animation
  m_shapes.resize(m_animations.size());
  m_steps.resize(m_animations.size());

  for (std::size_t a = 0; a < m_animations.size(); ++a) {
    const Animation &animation = m_animations[a];

    m_shapes[a].reserve(animation.frames.size());

    for (const AnimationFrame &frame : animation.frames) {
      m_shapes[a].push_back(CollisionShape::FromImage(sheet, collisionThreshold, frame.x, frame.y, frame.width,
                                                      frame.height));
    }

    std::vector<Step> &steps = m_steps[a];

    const auto count = static_cast<i32>(animation.frames.size());

    switch (animation.direction) {
      case AnimationDirection::Reverse:
        for (i32 i = count - 1; i >= 0; --i) {
          steps.push_back(Step{i, SanitizeDuration(animation.frames[static_cast<std::size_t>(i)].duration)});
        }

        break;

      case AnimationDirection::PingPong:
        // hin und zurueck, ohne die Enden doppelt zu spielen
        for (i32 i = 0; i < count; ++i) {
          steps.push_back(Step{i, SanitizeDuration(animation.frames[static_cast<std::size_t>(i)].duration)});
        }
        for (i32 i = count - 2; i >= 1; --i) {
          steps.push_back(Step{i, SanitizeDuration(animation.frames[static_cast<std::size_t>(i)].duration)});
        }

        break;

      case AnimationDirection::Forward:
      default:
        for (i32 i = 0; i < count; ++i) {
          steps.push_back(Step{i, SanitizeDuration(animation.frames[static_cast<std::size_t>(i)].duration)});
        }

        break;
    }
  }

  m_animationIndex = 0;
  m_stepIndex      = 0;
  m_stepTime       = 0.0f;
  m_finished       = false;

  applyOriginToShapes();
  applyFrame();

  SLOGD << "AnimatedSprite: " << m_animations.size() << " Animation(en), " << totalFrameCount()
        << " Rahmen, Blatt " << sheet.width() << "x" << sheet.height();

  return true;
}

// -----------------------------------------------------------------------------
// Animationen und Wiedergabe
// -----------------------------------------------------------------------------
bool AnimatedSprite::isValid() const { return !m_animations.empty(); }

bool AnimatedSprite::hasTexture() const { return m_sprite.isValid(); }

bool AnimatedSprite::hasShape() const {
  const CollisionShape &shape = currentShape();

  return shape.isValid();
}

i32 AnimatedSprite::animationCount() const { return static_cast<i32>(m_animations.size()); }

const std::string &AnimatedSprite::animationName(const i32 index) const {
  static const std::string empty;

  if ((index < 0) || (index >= static_cast<i32>(m_animations.size()))) return empty;

  return m_animations[static_cast<std::size_t>(index)].name;
}

bool AnimatedSprite::hasAnimation(const std::string &name) const { return findAnimation(name) >= 0; }

i32 AnimatedSprite::findAnimation(const std::string &name) const {
  for (std::size_t i = 0; i < m_animations.size(); ++i) {
    if (m_animations[i].name == name) return static_cast<i32>(i);
  }

  return -1;
}

const Animation *AnimatedSprite::animationAt(const i32 index) const {
  if ((index < 0) || (index >= static_cast<i32>(m_animations.size()))) return nullptr;

  return &m_animations[static_cast<std::size_t>(index)];
}

const Animation *AnimatedSprite::current() const { return animationAt(m_animationIndex); }

const std::vector<AnimatedSprite::Step> &AnimatedSprite::currentSteps() const {
  static const std::vector<Step> empty;

  if ((m_animationIndex < 0) || (m_animationIndex >= static_cast<i32>(m_steps.size()))) return empty;

  return m_steps[static_cast<std::size_t>(m_animationIndex)];
}

bool AnimatedSprite::play(const std::string &name, const bool restart) {
  const i32 index = findAnimation(name);

  if (index < 0) {
    SLOGW << "AnimatedSprite: Animation \"" << name << "\" gibt es nicht";

    return false;
  }

  if ((index == m_animationIndex) && !restart) return true;

  m_animationIndex = index;
  m_stepIndex      = 0;
  m_stepTime       = 0.0f;
  m_finished       = false;

  applyFrame();

  return true;
}

void AnimatedSprite::restart() {
  m_stepIndex = 0;
  m_stepTime  = 0.0f;
  m_finished  = false;

  applyFrame();
}

void AnimatedSprite::pause() { m_paused = true; }

void AnimatedSprite::resume() { m_paused = false; }

bool AnimatedSprite::isPaused() const { return m_paused; }

void AnimatedSprite::setSpeed(const f32 multiplier) { m_speed = std::max(0.0f, multiplier); }

f32 AnimatedSprite::speed() const { return m_speed; }

void AnimatedSprite::setAnimationSpeed(const std::string &name, const f32 multiplier) {
  const i32 index = findAnimation(name);

  if (index < 0) {
    SLOGW << "AnimatedSprite: Animation \"" << name << "\" gibt es nicht";

    return;
  }

  m_animationSpeeds[static_cast<std::size_t>(index)] = std::max(0.0f, multiplier);
}

f32 AnimatedSprite::animationSpeed(const std::string &name) const {
  const i32 index = findAnimation(name);

  if ((index < 0) || (index >= static_cast<i32>(m_animationSpeeds.size()))) return 1.0f;

  return m_animationSpeeds[static_cast<std::size_t>(index)];
}

f32 AnimatedSprite::effectiveSpeed() const {
  if (m_paused) return 0.0f;

  const f32 animationSpeed = ((m_animationIndex >= 0) && (m_animationIndex < static_cast<i32>(m_animationSpeeds.size())))
                                 ? m_animationSpeeds[static_cast<std::size_t>(m_animationIndex)]
                                 : 1.0f;

  return m_speed * animationSpeed;
}

void AnimatedSprite::setFrameDuration(const f32 seconds) {
  m_frameDuration = (seconds > 0.0f) ? seconds : 0.0f;
}

f32 AnimatedSprite::frameDurationOverride() const { return m_frameDuration; }

void AnimatedSprite::setLooping(const bool loop) {
  if (m_animationIndex < 0) return;

  if (static_cast<std::size_t>(m_animationIndex) < m_animations.size()) {
    m_animations[static_cast<std::size_t>(m_animationIndex)].loop = loop;
  }
}

bool AnimatedSprite::isLooping() const {
  const Animation *animation = current();

  return (animation != nullptr) && animation->loop;
}

void AnimatedSprite::setFrame(const i32 index) {
  const std::vector<Step> &steps = currentSteps();

  if (steps.empty()) return;

  // erster Schritt, der diesen Rahmen zeigt
  for (std::size_t step = 0; step < steps.size(); ++step) {
    if (steps[step].frame != index) continue;

    m_stepIndex = static_cast<i32>(step);
    m_stepTime  = 0.0f;
    m_finished  = false;

    applyFrame();

    return;
  }
}

void AnimatedSprite::update(const f32 deltaTime) {
  if (!isValid()) return;

  const f32 speed = effectiveSpeed();

  if (speed <= 0.0f) return;

  const std::vector<Step> &steps = currentSteps();

  if (steps.empty()) return;

  m_stepTime += deltaTime * speed;

  // mehrere Rahmen pro Bild durchlaufen (hohes Tempo, grosse deltaTime)
  for (i32 guard = 0; guard < 4096; ++guard) {
    const f32 duration = currentFrameDuration();

    if (m_stepTime < duration) break;

    m_stepTime -= duration;

    if ((m_stepIndex + 1) < static_cast<i32>(steps.size())) {
      ++m_stepIndex;

      applyFrame();

      continue;
    }

    const Animation *animation = current();

    if ((animation != nullptr) && animation->loop) {
      m_stepIndex = 0;
      m_stepTime  = std::fmod(m_stepTime, duration); // Restzeit behalten, nicht verlieren

      applyFrame();

      continue;
    }

    // nicht schleifend: am letzten Rahmen stehen bleiben
    m_stepIndex = static_cast<i32>(steps.size()) - 1;
    m_stepTime  = duration;
    m_finished  = true;

    break;
  }
}

// -----------------------------------------------------------------------------
// Zustand
// -----------------------------------------------------------------------------
const std::string &AnimatedSprite::currentAnimation() const {
  static const std::string empty;

  const Animation *animation = current();

  return (animation != nullptr) ? animation->name : empty;
}

i32 AnimatedSprite::currentAnimationIndex() const { return m_animationIndex; }

i32 AnimatedSprite::frameIndex() const {
  const std::vector<Step> &steps = currentSteps();

  if (steps.empty()) return 0;

  return steps[static_cast<std::size_t>(std::clamp(m_stepIndex, 0, static_cast<i32>(steps.size()) - 1))].frame;
}

i32 AnimatedSprite::stepIndex() const { return m_stepIndex; }

i32 AnimatedSprite::frameCount() const {
  const Animation *animation = current();

  return (animation != nullptr) ? static_cast<i32>(animation->frames.size()) : 0;
}

i32 AnimatedSprite::totalFrameCount() const {
  i32 total = 0;

  for (const Animation &animation : m_animations) total += static_cast<i32>(animation.frames.size());

  return total;
}

f32 AnimatedSprite::frameTime() const { return m_stepTime; }

f32 AnimatedSprite::currentFrameDuration() const {
  if (m_frameDuration > 0.0f) return m_frameDuration;

  const std::vector<Step> &steps = currentSteps();

  if (steps.empty()) return DefaultDuration;

  return steps[static_cast<std::size_t>(std::clamp(m_stepIndex, 0, static_cast<i32>(steps.size()) - 1))].duration;
}

f32 AnimatedSprite::duration() const {
  f32 total = 0.0f;

  for (const Step &step : currentSteps()) total += (m_frameDuration > 0.0f) ? m_frameDuration : step.duration;

  return total;
}

f32 AnimatedSprite::progress() const {
  const f32 totalDuration = duration();

  if (totalDuration <= 0.0f) return 0.0f;

  f32 elapsed = m_stepTime;

  const std::vector<Step> &steps = currentSteps();

  for (i32 step = 0; (step < m_stepIndex) && (step < static_cast<i32>(steps.size())); ++step) {
    elapsed += (m_frameDuration > 0.0f) ? m_frameDuration : steps[static_cast<std::size_t>(step)].duration;
  }

  return std::clamp(elapsed / totalDuration, 0.0f, 1.0f);
}

bool AnimatedSprite::isFinished() const { return m_finished; }

Sprite &AnimatedSprite::frame() { return m_sprite; }

const Sprite &AnimatedSprite::frame() const { return m_sprite; }

const CollisionShape &AnimatedSprite::currentShape() const {
  static const CollisionShape empty;

  if ((m_animationIndex < 0) || (m_animationIndex >= static_cast<i32>(m_shapes.size()))) return empty;

  const std::vector<CollisionShape> &shapes = m_shapes[static_cast<std::size_t>(m_animationIndex)];

  if (shapes.empty()) return empty;

  const i32 index = std::clamp(frameIndex(), 0, static_cast<i32>(shapes.size()) - 1);

  return shapes[static_cast<std::size_t>(index)];
}

const CollisionShape &AnimatedSprite::shape() const { return currentShape(); }

vec2 AnimatedSprite::size() const {
  return vec2(static_cast<f32>(m_sprite.width()), static_cast<f32>(m_sprite.height()));
}

void AnimatedSprite::applyFrame() {
  const Animation *animation = current();

  if (animation == nullptr) return;

  const i32 index = frameIndex();

  if ((index < 0) || (index >= static_cast<i32>(animation->frames.size()))) return;

  const AnimationFrame &frame = animation->frames[static_cast<std::size_t>(index)];

  // Ausschnitt tauschen, Deckungsform bleibt (sie ist bereits vorberechnet)
  m_sprite.setSourceRect(frame.x, frame.y, frame.width, frame.height, false);
}

void AnimatedSprite::applyOriginToShapes() {
  for (std::vector<CollisionShape> &shapes : m_shapes) {
    for (CollisionShape &shape : shapes) {
      if (!shape.isValid()) continue;

      shape.setPivot(vec2(m_origin.x * static_cast<f32>(shape.width()), m_origin.y * static_cast<f32>(shape.height())));
    }
  }

  m_sprite.setOrigin(m_origin);
}

// -----------------------------------------------------------------------------
// Darstellung
// -----------------------------------------------------------------------------
void AnimatedSprite::setOrigin(const vec2 &normalized) {
  m_origin = vec2(std::clamp(normalized.x, 0.0f, 1.0f), std::clamp(normalized.y, 0.0f, 1.0f));

  applyOriginToShapes();
}

vec2 AnimatedSprite::origin() const { return m_origin; }

void AnimatedSprite::setTint(const Color &tint) { m_sprite.setTint(tint); }

const Color &AnimatedSprite::tint() const { return m_sprite.tint(); }

void AnimatedSprite::setSmooth(const bool smooth) {
  m_smooth = smooth;

  m_sprite.setSmooth(smooth);
}

// -----------------------------------------------------------------------------
// Kollision (ueber die vorberechneten Formen des jeweils aktuellen Rahmens)
// -----------------------------------------------------------------------------
bool AnimatedSprite::containsPoint(const vec2 &worldPoint, const vec2 &position, const f32 rotationDegrees) const {
  return currentShape().containsWorldPoint(worldPoint, position, rotationDegrees);
}

bool AnimatedSprite::collidesWith(const AnimatedSprite &other, const vec2 &position, const f32 rotationDegrees,
                                  const vec2 &otherPosition, const f32 otherRotationDegrees) const {
  return currentShape().overlaps(other.currentShape(), position, rotationDegrees, otherPosition,
                                 otherRotationDegrees);
}

bool AnimatedSprite::collidesWith(const Sprite &other, const vec2 &position, const f32 rotationDegrees,
                                  const vec2 &otherPosition, const f32 otherRotationDegrees) const {
  return currentShape().overlaps(other.shape(), position, rotationDegrees, otherPosition, otherRotationDegrees);
}

// -----------------------------------------------------------------------------
// Freie Funktionen
// -----------------------------------------------------------------------------
AnimatedSprite LoadAnimatedSprite(const std::string_view imagePath, const std::string_view jsonPath) {
  AnimatedSprite sprite;

  if (!sprite.loadFromFile(std::string(imagePath), std::string(jsonPath))) return AnimatedSprite{};

  return sprite;
}

void DrawAnimatedSprite(const AnimatedSprite &sprite, const vec2 position, const f32 rotationAngle) {
  if (!sprite.isValid() || !sprite.hasTexture()) return;

  DrawSprite(sprite.frame(), position, rotationAngle);
}

}  // namespace sgl
