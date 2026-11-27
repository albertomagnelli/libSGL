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
    const Json *box = entry.Find("frame");

    if ((box == nullptr) || !box->IsObject()) box = &entry;

    AnimationFrame frame;
    frame.x      = box->Find("x") ? box->Find("x")->Int() : 0;
    frame.y      = box->Find("y") ? box->Find("y")->Int() : 0;
    frame.width  = box->Find("w") ? box->Find("w")->Int() : fallbackWidth;
    frame.height = box->Find("h") ? box->Find("h")->Int() : fallbackHeight;

    // Aseprite: Dauer in Millisekunden. Fehlt sie, gilt die Standarddauer.
    const Json *duration = entry.Find("duration");
    frame.duration       = (duration != nullptr) ? SanitizeDuration(duration->Float(DefaultDuration * 1000.0f) *
                                                                   MillisecondToSecond)
                                                 : DefaultDuration;

    return frame;
  }

  AnimationDirection ReadDirection(const Json *tag) {
    if ((tag == nullptr) || !tag->IsString()) return ANIMATION_FORWARD;

    const std::string &value = tag->String();

    if (value == "reverse") return ANIMATION_REVERSE;
    if (value == "pingpong") return ANIMATION_PINGPONG;

    return ANIMATION_FORWARD;
  }
}  // namespace

AnimatedSprite::~AnimatedSprite() { Unload(); }

AnimatedSprite::AnimatedSprite(AnimatedSprite &&other) noexcept { MoveFrom(other); }

AnimatedSprite &AnimatedSprite::operator=(AnimatedSprite &&other) noexcept {
  if (this == &other) return *this;

  Unload();
  MoveFrom(other);

  return *this;
}

void AnimatedSprite::MoveFrom(AnimatedSprite &other) {
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

void AnimatedSprite::Unload() {
  m_sprite.Unload();
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
bool AnimatedSprite::LoadFromFile(const std::string &imagePath, const std::string &jsonPath) {
  Image sheet;
  try {
    sheet = ImageIO::load(imagePath);
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
  if (!document.IsValid()) {
    SLOGW << "AnimatedSprite: keine brauchbaren Metadaten (" << document.Error()
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

    return Build(sheet, {std::move(animation)}, m_threshold);
  }

  // ----- Rahmen -----------------------------------------------------------
  const Json *framesJson = document.Find("frames");

  if (framesJson == nullptr) {
    SLOGE << "AnimatedSprite: \"" << metadataPath << "\" enthaelt keine Rahmen";

    return false;
  }

  const i32 fallbackWidth  = 0;
  const i32 fallbackHeight = 0;

  std::vector<AnimationFrame> frames;
  frames.reserve(static_cast<std::size_t>(framesJson->Size()));

  if (framesJson->IsArray()) {
    for (i32 i = 0; i < framesJson->Size(); ++i) {
      frames.push_back(ReadFrame(framesJson->At(i), fallbackWidth, fallbackHeight));
    }
  } else if (framesJson->IsObject()) {
    for (i32 i = 0; i < framesJson->Size(); ++i) {
      const Json *entry = framesJson->Find(framesJson->KeyAt(i));

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

  const Json *meta = document.Find("meta");
  const Json *tags = (meta != nullptr) ? meta->Find("frameTags") : nullptr;

  if ((tags != nullptr) && tags->IsArray() && (tags->Size() > 0)) {
    for (i32 t = 0; t < tags->Size(); ++t) {
      const Json &tag = tags->At(t);

      Animation animation;
      animation.name = tag.Find("name") ? tag.Find("name")->String() : ("animation" + std::to_string(t));
      animation.loop = tag.Find("repeat") ? (tag.Find("repeat")->Int() != 0) : true;
      animation.direction = ReadDirection(tag.Find("direction"));

      const i32 from = std::clamp(tag.Find("from") ? tag.Find("from")->Int() : 0, 0,
                                  static_cast<i32>(frames.size()) - 1);
      const i32 to   = std::clamp(tag.Find("to") ? tag.Find("to")->Int() : (static_cast<i32>(frames.size()) - 1),
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

  return Build(sheet, std::move(animations), m_threshold);
}

bool AnimatedSprite::LoadFromImage(const Image &sheet, std::vector<Animation> animations,
                                   const u8 collisionThreshold) {
  m_threshold = collisionThreshold;

  return Build(sheet, std::move(animations), collisionThreshold);
}

bool AnimatedSprite::LoadFromGrid(const Image &sheet, const i32 frameWidth, const i32 frameHeight,
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

  return Build(sheet, {std::move(animation)}, collisionThreshold);
}

bool AnimatedSprite::Build(const Image &sheet, std::vector<Animation> animations, const u8 collisionThreshold) {
  if (sheet.isEmpty() || animations.empty()) {
    SLOGE << "AnimatedSprite: Blatt oder Animationen fehlen";

    return false;
  }

  Unload();

  m_threshold = collisionThreshold;

  // Textur aus dem Blatt; ohne GL-Kontext bleiben nur die Deckungsformen
  // (Kollision ja, Zeichnen nein -- wie bei Sprite)
  m_sheetWidth  = sheet.width();
  m_sheetHeight = sheet.height();

  if ((getCurrentWindow() != nullptr) && (GL::glCreateTextures != nullptr)) {
    try {
      TextureOptions options;
      options.mipmaps   = MipmapMode::None;
      options.isSrgb    = false;
      options.wrapS     = TextureWrap::ClampToEdge;
      options.wrapT     = TextureWrap::ClampToEdge;
      options.minFilter = m_smooth ? TextureFilter::Linear : TextureFilter::Nearest;
      options.magFilter = options.minFilter;

      Texture2d texture = Texture2d::fromImage(sheet, options);
      m_sprite.LoadFromTexture(std::move(texture), sheet.width(), sheet.height());
      m_sprite.SetOrigin(m_origin);
      m_sprite.SetSmooth(m_smooth);
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
      case ANIMATION_REVERSE:
        for (i32 i = count - 1; i >= 0; --i) {
          steps.push_back(Step{i, SanitizeDuration(animation.frames[static_cast<std::size_t>(i)].duration)});
        }

        break;

      case ANIMATION_PINGPONG:
        // hin und zurueck, ohne die Enden doppelt zu spielen
        for (i32 i = 0; i < count; ++i) {
          steps.push_back(Step{i, SanitizeDuration(animation.frames[static_cast<std::size_t>(i)].duration)});
        }
        for (i32 i = count - 2; i >= 1; --i) {
          steps.push_back(Step{i, SanitizeDuration(animation.frames[static_cast<std::size_t>(i)].duration)});
        }

        break;

      case ANIMATION_FORWARD:
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

  ApplyOriginToShapes();
  ApplyFrame();

  SLOGD << "AnimatedSprite: " << m_animations.size() << " Animation(en), " << TotalFrameCount()
        << " Rahmen, Blatt " << sheet.width() << "x" << sheet.height();

  return true;
}

// -----------------------------------------------------------------------------
// Animationen und Wiedergabe
// -----------------------------------------------------------------------------
bool AnimatedSprite::IsValid() const { return !m_animations.empty(); }

bool AnimatedSprite::HasTexture() const { return m_sprite.IsValid(); }

bool AnimatedSprite::HasShape() const {
  const CollisionShape &shape = CurrentShape();

  return shape.IsValid();
}

i32 AnimatedSprite::AnimationCount() const { return static_cast<i32>(m_animations.size()); }

const std::string &AnimatedSprite::AnimationName(const i32 index) const {
  static const std::string empty;

  if ((index < 0) || (index >= static_cast<i32>(m_animations.size()))) return empty;

  return m_animations[static_cast<std::size_t>(index)].name;
}

bool AnimatedSprite::HasAnimation(const std::string &name) const { return FindAnimation(name) >= 0; }

i32 AnimatedSprite::FindAnimation(const std::string &name) const {
  for (std::size_t i = 0; i < m_animations.size(); ++i) {
    if (m_animations[i].name == name) return static_cast<i32>(i);
  }

  return -1;
}

const Animation *AnimatedSprite::AnimationAt(const i32 index) const {
  if ((index < 0) || (index >= static_cast<i32>(m_animations.size()))) return nullptr;

  return &m_animations[static_cast<std::size_t>(index)];
}

const Animation *AnimatedSprite::Current() const { return AnimationAt(m_animationIndex); }

const std::vector<AnimatedSprite::Step> &AnimatedSprite::CurrentSteps() const {
  static const std::vector<Step> empty;

  if ((m_animationIndex < 0) || (m_animationIndex >= static_cast<i32>(m_steps.size()))) return empty;

  return m_steps[static_cast<std::size_t>(m_animationIndex)];
}

bool AnimatedSprite::Play(const std::string &name, const bool restart) {
  const i32 index = FindAnimation(name);

  if (index < 0) {
    SLOGW << "AnimatedSprite: Animation \"" << name << "\" gibt es nicht";

    return false;
  }

  if ((index == m_animationIndex) && !restart) return true;

  m_animationIndex = index;
  m_stepIndex      = 0;
  m_stepTime       = 0.0f;
  m_finished       = false;

  ApplyFrame();

  return true;
}

void AnimatedSprite::Restart() {
  m_stepIndex = 0;
  m_stepTime  = 0.0f;
  m_finished  = false;

  ApplyFrame();
}

void AnimatedSprite::Pause() { m_paused = true; }

void AnimatedSprite::Resume() { m_paused = false; }

bool AnimatedSprite::IsPaused() const { return m_paused; }

void AnimatedSprite::SetSpeed(const f32 multiplier) { m_speed = std::max(0.0f, multiplier); }

f32 AnimatedSprite::Speed() const { return m_speed; }

void AnimatedSprite::SetAnimationSpeed(const std::string &name, const f32 multiplier) {
  const i32 index = FindAnimation(name);

  if (index < 0) {
    SLOGW << "AnimatedSprite: Animation \"" << name << "\" gibt es nicht";

    return;
  }

  m_animationSpeeds[static_cast<std::size_t>(index)] = std::max(0.0f, multiplier);
}

f32 AnimatedSprite::AnimationSpeed(const std::string &name) const {
  const i32 index = FindAnimation(name);

  if ((index < 0) || (index >= static_cast<i32>(m_animationSpeeds.size()))) return 1.0f;

  return m_animationSpeeds[static_cast<std::size_t>(index)];
}

f32 AnimatedSprite::EffectiveSpeed() const {
  if (m_paused) return 0.0f;

  const f32 animationSpeed = ((m_animationIndex >= 0) && (m_animationIndex < static_cast<i32>(m_animationSpeeds.size())))
                                 ? m_animationSpeeds[static_cast<std::size_t>(m_animationIndex)]
                                 : 1.0f;

  return m_speed * animationSpeed;
}

void AnimatedSprite::SetFrameDuration(const f32 seconds) {
  m_frameDuration = (seconds > 0.0f) ? seconds : 0.0f;
}

f32 AnimatedSprite::FrameDurationOverride() const { return m_frameDuration; }

void AnimatedSprite::SetLooping(const bool loop) {
  if (m_animationIndex < 0) return;

  if (static_cast<std::size_t>(m_animationIndex) < m_animations.size()) {
    m_animations[static_cast<std::size_t>(m_animationIndex)].loop = loop;
  }
}

bool AnimatedSprite::IsLooping() const {
  const Animation *animation = Current();

  return (animation != nullptr) && animation->loop;
}

void AnimatedSprite::SetFrame(const i32 index) {
  const std::vector<Step> &steps = CurrentSteps();

  if (steps.empty()) return;

  // erster Schritt, der diesen Rahmen zeigt
  for (std::size_t step = 0; step < steps.size(); ++step) {
    if (steps[step].frame != index) continue;

    m_stepIndex = static_cast<i32>(step);
    m_stepTime  = 0.0f;
    m_finished  = false;

    ApplyFrame();

    return;
  }
}

void AnimatedSprite::Update(const f32 deltaTime) {
  if (!IsValid()) return;

  const f32 speed = EffectiveSpeed();

  if (speed <= 0.0f) return;

  const std::vector<Step> &steps = CurrentSteps();

  if (steps.empty()) return;

  m_stepTime += deltaTime * speed;

  // mehrere Rahmen pro Bild durchlaufen (hohes Tempo, grosse deltaTime)
  for (i32 guard = 0; guard < 4096; ++guard) {
    const f32 duration = CurrentFrameDuration();

    if (m_stepTime < duration) break;

    m_stepTime -= duration;

    if ((m_stepIndex + 1) < static_cast<i32>(steps.size())) {
      ++m_stepIndex;

      ApplyFrame();

      continue;
    }

    const Animation *animation = Current();

    if ((animation != nullptr) && animation->loop) {
      m_stepIndex = 0;
      m_stepTime  = std::fmod(m_stepTime, duration); // Restzeit behalten, nicht verlieren

      ApplyFrame();

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
const std::string &AnimatedSprite::CurrentAnimation() const {
  static const std::string empty;

  const Animation *animation = Current();

  return (animation != nullptr) ? animation->name : empty;
}

i32 AnimatedSprite::CurrentAnimationIndex() const { return m_animationIndex; }

i32 AnimatedSprite::FrameIndex() const {
  const std::vector<Step> &steps = CurrentSteps();

  if (steps.empty()) return 0;

  return steps[static_cast<std::size_t>(std::clamp(m_stepIndex, 0, static_cast<i32>(steps.size()) - 1))].frame;
}

i32 AnimatedSprite::StepIndex() const { return m_stepIndex; }

i32 AnimatedSprite::FrameCount() const {
  const Animation *animation = Current();

  return (animation != nullptr) ? static_cast<i32>(animation->frames.size()) : 0;
}

i32 AnimatedSprite::TotalFrameCount() const {
  i32 total = 0;

  for (const Animation &animation : m_animations) total += static_cast<i32>(animation.frames.size());

  return total;
}

f32 AnimatedSprite::FrameTime() const { return m_stepTime; }

f32 AnimatedSprite::CurrentFrameDuration() const {
  if (m_frameDuration > 0.0f) return m_frameDuration;

  const std::vector<Step> &steps = CurrentSteps();

  if (steps.empty()) return DefaultDuration;

  return steps[static_cast<std::size_t>(std::clamp(m_stepIndex, 0, static_cast<i32>(steps.size()) - 1))].duration;
}

f32 AnimatedSprite::Duration() const {
  f32 total = 0.0f;

  for (const Step &step : CurrentSteps()) total += (m_frameDuration > 0.0f) ? m_frameDuration : step.duration;

  return total;
}

f32 AnimatedSprite::Progress() const {
  const f32 duration = Duration();

  if (duration <= 0.0f) return 0.0f;

  f32 elapsed = m_stepTime;

  const std::vector<Step> &steps = CurrentSteps();

  for (i32 step = 0; (step < m_stepIndex) && (step < static_cast<i32>(steps.size())); ++step) {
    elapsed += (m_frameDuration > 0.0f) ? m_frameDuration : steps[static_cast<std::size_t>(step)].duration;
  }

  return std::clamp(elapsed / duration, 0.0f, 1.0f);
}

bool AnimatedSprite::IsFinished() const { return m_finished; }

Sprite &AnimatedSprite::Frame() { return m_sprite; }

const Sprite &AnimatedSprite::Frame() const { return m_sprite; }

const CollisionShape &AnimatedSprite::CurrentShape() const {
  static const CollisionShape empty;

  if ((m_animationIndex < 0) || (m_animationIndex >= static_cast<i32>(m_shapes.size()))) return empty;

  const std::vector<CollisionShape> &shapes = m_shapes[static_cast<std::size_t>(m_animationIndex)];

  if (shapes.empty()) return empty;

  const i32 index = std::clamp(FrameIndex(), 0, static_cast<i32>(shapes.size()) - 1);

  return shapes[static_cast<std::size_t>(index)];
}

const CollisionShape &AnimatedSprite::Shape() const { return CurrentShape(); }

vec2 AnimatedSprite::Size() const {
  return vec2(static_cast<f32>(m_sprite.Width()), static_cast<f32>(m_sprite.Height()));
}

void AnimatedSprite::ApplyFrame() {
  const Animation *animation = Current();

  if (animation == nullptr) return;

  const i32 index = FrameIndex();

  if ((index < 0) || (index >= static_cast<i32>(animation->frames.size()))) return;

  const AnimationFrame &frame = animation->frames[static_cast<std::size_t>(index)];

  // Ausschnitt tauschen, Deckungsform bleibt (sie ist bereits vorberechnet)
  m_sprite.SetSourceRect(frame.x, frame.y, frame.width, frame.height, false);
}

void AnimatedSprite::ApplyOriginToShapes() {
  for (std::vector<CollisionShape> &shapes : m_shapes) {
    for (CollisionShape &shape : shapes) {
      if (!shape.IsValid()) continue;

      shape.SetPivot(vec2(m_origin.x * static_cast<f32>(shape.Width()), m_origin.y * static_cast<f32>(shape.Height())));
    }
  }

  m_sprite.SetOrigin(m_origin);
}

// -----------------------------------------------------------------------------
// Darstellung
// -----------------------------------------------------------------------------
void AnimatedSprite::SetOrigin(const vec2 &normalized) {
  m_origin = vec2(std::clamp(normalized.x, 0.0f, 1.0f), std::clamp(normalized.y, 0.0f, 1.0f));

  ApplyOriginToShapes();
}

vec2 AnimatedSprite::Origin() const { return m_origin; }

void AnimatedSprite::SetTint(const Color &tint) { m_sprite.SetTint(tint); }

const Color &AnimatedSprite::Tint() const { return m_sprite.Tint(); }

void AnimatedSprite::SetSmooth(const bool smooth) {
  m_smooth = smooth;

  m_sprite.SetSmooth(smooth);
}

// -----------------------------------------------------------------------------
// Kollision (ueber die vorberechneten Formen des jeweils aktuellen Rahmens)
// -----------------------------------------------------------------------------
bool AnimatedSprite::ContainsPoint(const vec2 &worldPoint, const vec2 &position, const f32 rotationDegrees) const {
  return CurrentShape().ContainsWorldPoint(worldPoint, position, rotationDegrees);
}

bool AnimatedSprite::CollidesWith(const AnimatedSprite &other, const vec2 &position, const f32 rotationDegrees,
                                  const vec2 &otherPosition, const f32 otherRotationDegrees) const {
  return CurrentShape().Overlaps(other.CurrentShape(), position, rotationDegrees, otherPosition,
                                 otherRotationDegrees);
}

bool AnimatedSprite::CollidesWith(const Sprite &other, const vec2 &position, const f32 rotationDegrees,
                                  const vec2 &otherPosition, const f32 otherRotationDegrees) const {
  return CurrentShape().Overlaps(other.Shape(), position, rotationDegrees, otherPosition, otherRotationDegrees);
}

// -----------------------------------------------------------------------------
// Freie Funktionen
// -----------------------------------------------------------------------------
AnimatedSprite LoadAnimatedSprite(const std::string_view imagePath, const std::string_view jsonPath) {
  AnimatedSprite sprite;

  if (!sprite.LoadFromFile(std::string(imagePath), std::string(jsonPath))) return AnimatedSprite{};

  return sprite;
}

void DrawAnimatedSprite(const AnimatedSprite &sprite, const vec2 position, const f32 rotationAngle) {
  if (!sprite.IsValid() || !sprite.HasTexture()) return;

  DrawSprite(sprite.Frame(), position, rotationAngle);
}

}  // namespace sgl
