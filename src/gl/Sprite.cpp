//
// Created by ingsto on 14.11.26.
//
// Sprite: Textur, Deckungsform und Darstellung eines 2D-Bildes.
// Die Kollisionsform entsteht aus dem Alphakanal und liegt lokal (unrotiert);
// gezeichnet wird ueber einen gemeinsamen Shader und ein Quad-VAO.
//
#include <sgl/Sprite.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include <sgl/Context.hpp>
#include <sgl/GL.hpp>
#include <sgl/Log.hpp>
#include <sgl/Shader.hpp>

namespace sgl {

using namespace gl;

namespace {
  constexpr f32 DegreesToRadians = 3.14159265358979323846f / 180.0f;

  bool HasCurrentContext() {
    return GetCurrentWindow() != nullptr && gl::glCreateTextures != nullptr;
  }

  // ---------------------------------------------------------------------------
  // Zeichner je OpenGL-Kontext: Shader, Quad und Uniforms werden geteilt, aber
  // GL-Objekte gelten nur im Kontext, in dem sie entstanden sind. Bei mehreren
  // Fenstern (Tests, zweite Ansicht) gibt es daher einen Zeichner pro Kontext.
  // ---------------------------------------------------------------------------
  struct SpriteRenderer {
    Window     *context{};
    Shader      shader;
    u32         vertexArray{};
    u32         vertexBuffer{};
  };

  // Bewusst nie freigegeben: GL-Objekte duerfen nicht nach dem Kontext zerstoert
  // werden, und sgl meldet das Ende eines Fensters nicht an den Zeichner.
  std::vector<std::unique_ptr<SpriteRenderer>> &Renderers() {
    static auto *renderers = new std::vector<std::unique_ptr<SpriteRenderer>>();
    return *renderers;
  }

  // Pixelkoordinaten (y nach unten), Drehung um den Ursprung des Sprites
  constexpr const char *SpriteVertexShader = R"(
layout(location = 0) in vec2 aCorner;

uniform vec2  uViewport;
uniform vec2  uPosition;  // Pixel, y nach unten
uniform vec2  uSize;      // Pixel
uniform vec2  uOrigin;    // Pixel innerhalb des Sprites (Bezugspunkt)
uniform float uRotation;  // Radiant, positiv gegen den Uhrzeigersinn
uniform vec2  uFlip;

out vec2 uv;

void main() {
  // Ecke relativ zum Bezugspunkt, mit y nach unten
  vec2 local = (aCorner * uSize) - uOrigin;
  local.x *= uFlip.x;
  local.y *= uFlip.y;

  const float c = cos(uRotation);
  const float s = sin(uRotation);

  // y zeigt nach unten -> diese Matrix dreht positiv gegen den Uhrzeigersinn
  const vec2 rotated = vec2((local.x * c) + (local.y * s), (-local.x * s) + (local.y * c));
  const vec2 pixel   = uPosition + rotated;

  uv = aCorner;
  gl_Position = vec4(((pixel.x / uViewport.x) * 2.0) - 1.0, 1.0 - ((pixel.y / uViewport.y) * 2.0), 0.0, 1.0);
}
)";

  constexpr const char *SpriteFragmentShader = R"(
in vec2 uv;
out vec4 fragColor;

uniform sampler2D uTexture;
uniform vec2 uUvMin;
uniform vec2 uUvMax;
uniform vec4 uTint;

void main() {
  const vec2 texcoord = mix(uUvMin, uUvMax, uv);
  const vec4 texel    = texture(uTexture, texcoord);

  fragColor = vec4(texel.rgb * uTint.rgb, texel.a * uTint.a);
}
)";

  SpriteRenderer *RendererForCurrentContext() {
    Window *current = GetCurrentWindow();

    if ((current == nullptr) || (gl::glCreateVertexArrays == nullptr)) {
      SLOGE << "DrawSprite: kein aktiver OpenGL-Kontext (Window noetig)";

      return nullptr;
    }

    for (const std::unique_ptr<SpriteRenderer> &renderer : Renderers()) {
      if (renderer->context == current) return renderer.get();
    }

    auto renderer = std::make_unique<SpriteRenderer>();

    renderer->context = current;

    std::vector<StageSource> stages;
    StageSource vertex;
    vertex.stage       = ShaderStage::Vertex;
    vertex.source      = SpriteVertexShader;
    vertex.debugName   = "sprite vertex";
    StageSource fragment;
    fragment.stage     = ShaderStage::Fragment;
    fragment.source    = SpriteFragmentShader;
    fragment.debugName = "sprite fragment";
    stages.push_back(std::move(vertex));
    stages.push_back(std::move(fragment));

    renderer->shader = Shader::FromSources(std::move(stages));

    if (!renderer->shader.isValid()) {
      SLOGE << "DrawSprite: Sprite-Shader laesst sich nicht bauen: " << renderer->shader.lastError();

      return nullptr;
    }

    const f32 corners[8] = {0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};

    glCreateVertexArrays(1, &renderer->vertexArray);
    glCreateBuffers(1, &renderer->vertexBuffer);
    glNamedBufferData(renderer->vertexBuffer, sizeof(corners), corners, GL_STATIC_DRAW);

    glVertexArrayVertexBuffer(renderer->vertexArray, 0, renderer->vertexBuffer, 0, sizeof(f32) * 2);
    glEnableVertexArrayAttrib(renderer->vertexArray, 0);
    glVertexArrayAttribFormat(renderer->vertexArray, 0, 2, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(renderer->vertexArray, 0, 0);

    Renderers().push_back(std::move(renderer));

    return Renderers().back().get();
  }
}  // namespace

Sprite::~Sprite() { unload(); }

Sprite::Sprite(Sprite &&other) noexcept { moveFrom(other); }

Sprite &Sprite::operator=(Sprite &&other) noexcept {
  if (this == &other) return *this;

  unload();
  moveFrom(other);

  return *this;
}

void Sprite::moveFrom(Sprite &other) {
  m_texture      = std::move(other.m_texture);
  m_pixels       = std::move(other.m_pixels);
  m_shape        = std::move(other.m_shape);
  m_sourceX      = other.m_sourceX;
  m_sourceY      = other.m_sourceY;
  m_sourceWidth  = other.m_sourceWidth;
  m_sourceHeight = other.m_sourceHeight;
  m_origin       = other.m_origin;
  m_tint         = other.m_tint;
  m_flipX        = other.m_flipX;
  m_flipY        = other.m_flipY;
  m_smooth       = other.m_smooth;
  m_threshold    = other.m_threshold;

  other.m_sourceX      = 0;
  other.m_sourceY      = 0;
  other.m_sourceWidth  = 0;
  other.m_sourceHeight = 0;
  other.m_shape        = CollisionShape{};
  other.m_pixels       = Image{};
}

void Sprite::unload() {
  m_texture = Texture2d{};
  m_pixels  = Image{};
  m_shape   = CollisionShape{};

  m_sourceX      = 0;
  m_sourceY      = 0;
  m_sourceWidth  = 0;
  m_sourceHeight = 0;
}

// -----------------------------------------------------------------------------
// Laden
// -----------------------------------------------------------------------------
bool Sprite::loadFromImage(const Image &image, const bool smooth, const u8 collisionThreshold) {
  if (image.isEmpty()) {
    SLOGE << "Sprite: Bild ist ungueltig";
    return false;
  }

  unload();

  m_pixels    = image; // tiefe Kopie fuer die Deckungsform
  m_threshold = collisionThreshold;
  m_smooth    = smooth;

  m_sourceX      = 0;
  m_sourceY      = 0;
  m_sourceWidth  = image.width();
  m_sourceHeight = image.height();

  if (HasCurrentContext()) {
    try {
      TextureOptions options;
      options.mipmaps   = MipmapMode::None;
      options.isSrgb    = false;
      options.wrapS     = TextureWrap::ClampToEdge;
      options.wrapT     = TextureWrap::ClampToEdge;
      options.minFilter = smooth ? TextureFilter::Linear : TextureFilter::Nearest;
      options.magFilter = options.minFilter;
      m_texture = Texture2d::FromImage(image, options);
    } catch (const std::exception &error) {
      SLOGW << "Sprite: Textur laesst sich nicht anlegen (" << error.what() << ", Kollision funktioniert trotzdem)";
    }
  }

  rebuildShape();

  return true;
}

bool Sprite::loadFromTexture(const Texture2d &texture, const bool smooth, const u8 collisionThreshold) {
  if (!texture.isValid()) {
    SLOGE << "Sprite: Textur ist ungueltig";
    return false;
  }

  if (!HasCurrentContext()) {
    SLOGE << "Sprite: Textur ohne OpenGL-Kontext nicht lesbar";
    return false;
  }

  unload();

  m_threshold = collisionThreshold;
  m_smooth    = smooth;

  // Pixel fuer die Deckungsform aus der GPU zurueckholen und eine eigene Textur
  // anlegen (Texture2d ist move-only, die uebergebene bleibt beim Aufrufer)
  try {
    m_pixels = texture.readBack();
  } catch (const std::exception &error) {
    SLOGW << "Sprite: Pixel der Textur lassen sich nicht lesen (" << error.what() << ")";
    m_pixels = Image{};
  }

  if (!m_pixels.isEmpty()) {
    SLOGW << "Sprite: Pixel der Textur lassen sich nicht lesen (Kollision entfaellt)";
  }

  if (!m_pixels.isEmpty()) {
    try {
      TextureOptions options;
      options.mipmaps   = MipmapMode::None;
      options.isSrgb    = false;
      options.wrapS     = TextureWrap::ClampToEdge;
      options.wrapT     = TextureWrap::ClampToEdge;
      options.minFilter = smooth ? TextureFilter::Linear : TextureFilter::Nearest;
      options.magFilter = options.minFilter;
      m_texture = Texture2d::FromImage(m_pixels, options);
    } catch (const std::exception &error) {
      SLOGW << "Sprite: Textur laesst sich nicht anlegen (" << error.what() << ")";
    }
  }

  m_sourceX      = 0;
  m_sourceY      = 0;
  m_sourceWidth  = texture.width();
  m_sourceHeight = texture.height();

  rebuildShape();

  return m_texture.isValid();
}

bool Sprite::loadFromTexture(Texture2d &&texture, const i32 width, const i32 height) {
  if (!texture.isValid()) {
    SLOGE << "Sprite: Textur ist ungueltig";

    return false;
  }

  unload();

  m_texture      = std::move(texture);
  m_sourceX      = 0;
  m_sourceY      = 0;
  m_sourceWidth  = (width > 0) ? width : m_texture.width();
  m_sourceHeight = (height > 0) ? height : m_texture.height();

  // keine Pixel, keine Deckungsform: der Aufrufer liefert die Form selbst
  return true;
}

bool Sprite::setSourceRect(const i32 x, const i32 y, const i32 width, const i32 height, const bool rebuild) {
  const i32 textureWidth  = !m_pixels.isEmpty() ? m_pixels.width() : m_texture.width();
  const i32 textureHeight = !m_pixels.isEmpty() ? m_pixels.height() : m_texture.height();

  // ohne Textur und ohne Pixel (kein GL-Kontext) ist keine Referenzgroesse
  // bekannt -- dann entscheidet der Aufrufer ueber die Gueltigkeit
  const bool checkBounds = (textureWidth > 0) && (textureHeight > 0);

  if ((width <= 0) || (height <= 0) || (x < 0) || (y < 0) ||
      (checkBounds && (((x + width) > textureWidth) || ((y + height) > textureHeight)))) {
    SLOGW << "Sprite: Ausschnitt " << width << "x" << height << " bei " << x << "," << y << " passt nicht in "
          << textureWidth << "x" << textureHeight;
    return false;
  }

  m_sourceX      = x;
  m_sourceY      = y;
  m_sourceWidth  = width;
  m_sourceHeight = height;

  if (rebuild) rebuildShape();

  return true;
}

void Sprite::resetSourceRect() {
  m_sourceX      = 0;
  m_sourceY      = 0;
  m_sourceWidth  = !m_pixels.isEmpty() ? m_pixels.width() : m_texture.width();
  m_sourceHeight = !m_pixels.isEmpty() ? m_pixels.height() : m_texture.height();

  rebuildShape();
}

i32 Sprite::sourceX() const { return m_sourceX; }

i32 Sprite::sourceY() const { return m_sourceY; }

// -----------------------------------------------------------------------------
// Deckungsform
// -----------------------------------------------------------------------------
void Sprite::rebuildShape() {
  m_shape = CollisionShape{};

  if (m_pixels.isEmpty() || (m_sourceWidth <= 0) || (m_sourceHeight <= 0)) return;

  const Image &image = m_pixels;

  // Ausschnitt als eigenes Bild herausschneiden (nur wenn noetig)
  if ((m_sourceX == 0) && (m_sourceY == 0) && (m_sourceWidth == image.width()) && (m_sourceHeight == image.height())) {
    m_shape = CollisionShape::FromImage(image, m_threshold);
  } else {
    const Image cropped = image.cropped(m_sourceX, m_sourceY, m_sourceWidth, m_sourceHeight);

    m_shape = CollisionShape::FromImage(cropped, m_threshold);
  }

  // Bezugspunkt der Form = origin des Sprites (Zeichnen und Kollision gleich)
  m_shape.setPivot(vec2(m_origin.x * static_cast<f32>(m_sourceWidth), m_origin.y * static_cast<f32>(m_sourceHeight)));
}

// -----------------------------------------------------------------------------
// Darstellung
// -----------------------------------------------------------------------------
void Sprite::setOrigin(const vec2 &normalized) {
  m_origin = vec2(std::clamp(normalized.x, 0.0f, 1.0f), std::clamp(normalized.y, 0.0f, 1.0f));

  if (m_shape.isValid()) {
    m_shape.setPivot(
        vec2(m_origin.x * static_cast<f32>(m_sourceWidth), m_origin.y * static_cast<f32>(m_sourceHeight)));
  }
}

vec2 Sprite::origin() const { return m_origin; }

void Sprite::setTint(const Color &tint) { m_tint = tint; }

const Color &Sprite::tint() const { return m_tint; }

void Sprite::setFlip(const bool flipX, const bool flipY) {
  m_flipX = flipX;
  m_flipY = flipY;
}

bool Sprite::flipX() const { return m_flipX; }

bool Sprite::flipY() const { return m_flipY; }

void Sprite::setSmooth(const bool smooth) {
  m_smooth = smooth;

  if (m_texture.isValid()) {
    m_texture.setFilter(smooth ? TextureFilter::Linear : TextureFilter::Nearest,
                        smooth ? TextureFilter::Linear : TextureFilter::Nearest, MipmapMode::None);
  }
}

// -----------------------------------------------------------------------------
// Abfragen
// -----------------------------------------------------------------------------
bool Sprite::isValid() const { return m_texture.isValid() && (m_sourceWidth > 0) && (m_sourceHeight > 0); }

bool Sprite::hasShape() const { return m_shape.isValid(); }

i32 Sprite::width() const { return m_sourceWidth; }

i32 Sprite::height() const { return m_sourceHeight; }

vec2 Sprite::size() const { return vec2(static_cast<f32>(m_sourceWidth), static_cast<f32>(m_sourceHeight)); }

const Texture2d &Sprite::texture() const { return m_texture; }

const CollisionShape &Sprite::shape() const { return m_shape; }

const Image &Sprite::pixels() const { return m_pixels; }

// -----------------------------------------------------------------------------
// Kollision
// -----------------------------------------------------------------------------
bool Sprite::containsPoint(const vec2 &worldPoint, const vec2 &position, const f32 rotationDegrees) const {
  return m_shape.containsWorldPoint(worldPoint, position, rotationDegrees);
}

bool Sprite::collidesWith(const Sprite &other, const vec2 &position, const f32 rotationDegrees,
                          const vec2 &otherPosition, const f32 otherRotationDegrees) const {
  return m_shape.overlaps(other.m_shape, position, rotationDegrees, otherPosition, otherRotationDegrees);
}

// -----------------------------------------------------------------------------
// Freie Funktionen
// -----------------------------------------------------------------------------
void DrawSprite(const Sprite &sprite, const vec2 position, const f32 rotationAngle) {
  if (!sprite.isValid()) return;

  SpriteRenderer *renderer = RendererForCurrentContext();

  if (renderer == nullptr) return;

  const Texture2d &texture = sprite.texture();

  if ((texture.width() <= 0) || (texture.height() <= 0)) return;

  GLint viewport[4] = {};
  glGetIntegerv(GL_VIEWPORT, viewport);

  const f32 screenWidth  = static_cast<f32>(viewport[2]);
  const f32 screenHeight = static_cast<f32>(viewport[3]);

  Shader &shader = renderer->shader;

  shader.use();
  shader.setUniform("uViewport", vec2(screenWidth, screenHeight));
  shader.setUniform("uPosition", position);
  shader.setUniform("uSize", sprite.size());
  shader.setUniform("uOrigin",
                    vec2(sprite.origin().x * static_cast<f32>(sprite.width()),
                         sprite.origin().y * static_cast<f32>(sprite.height())));
  shader.setUniform("uRotation", rotationAngle * DegreesToRadians);
  shader.setUniform("uFlip", vec2(sprite.flipX() ? -1.0f : 1.0f, sprite.flipY() ? -1.0f : 1.0f));
  shader.setUniform("uTint", sprite.tint().toVec4());

  const f32 textureWidth  = static_cast<f32>(texture.width());
  const f32 textureHeight = static_cast<f32>(texture.height());

  shader.setUniform("uUvMin", vec2(static_cast<f32>(sprite.sourceX()) / textureWidth,
                                   static_cast<f32>(sprite.sourceY()) / textureHeight));
  shader.setUniform("uUvMax", vec2(static_cast<f32>(sprite.sourceX() + sprite.width()) / textureWidth,
                                   static_cast<f32>(sprite.sourceY() + sprite.height()) / textureHeight));
  shader.setUniform("uTexture", 0);

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  texture.use(0);

  glBindVertexArray(renderer->vertexArray);
  glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
  glBindVertexArray(0);

  glDisable(GL_BLEND);
}

Sprite LoadSpriteFromTexture(const Texture2d &texture) {
  Sprite sprite;
  if (!sprite.loadFromTexture(texture)) return Sprite{};

  return sprite;
}

Sprite LoadSpriteFromImage(const Image &image) {
  Sprite sprite;
  if (!sprite.loadFromImage(image)) return Sprite{};

  return sprite;
}

}  // namespace sgl

