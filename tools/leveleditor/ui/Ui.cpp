#include "Ui.hpp"

#include <sgl/Font.hpp>
#include <sgl/GL.hpp>
#include <sgl/Time.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <sstream>

#ifndef SGL_EDITOR_FONT
#define SGL_EDITOR_FONT "DejaVuSans.ttf"
#endif

namespace editor::ui {
namespace {

constexpr const char* kUiShader = R"(
#pragma stage vertex
#version 330 core
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aUv;
layout(location = 2) in vec4 aColor;
uniform vec2 uScreen;
out vec2 vUv;
out vec4 vColor;
void main() {
    vec2 p = aPosition / uScreen * 2.0 - 1.0;
    gl_Position = vec4(p.x, -p.y, 0.0, 1.0);
    vUv = aUv;
    vColor = aColor;
}
#pragma stage fragment
#version 330 core
in vec2 vUv;
in vec4 vColor;
out vec4 fragColor;
uniform sampler2D uTexture;
void main() {
    fragColor = texture(uTexture, vUv) * vColor;
}
)";

inline f32 Clamp01(f32 value) { return std::clamp(value, 0.0f, 1.0f); }

u64 HashText(std::string_view text) {
    u64 hash = 1469598103934665603ull;
    for (const char c : text) {
        hash ^= static_cast<unsigned char>(c);
        hash *= 1099511628211ull;
    }
    return hash;
}

std::size_t PreviousCodepoint(std::string_view text, std::size_t caret) {
    if (caret == 0) return 0;
    --caret;
    while (caret > 0 && (static_cast<unsigned char>(text[caret]) & 0xC0) == 0x80) --caret;
    return caret;
}

std::size_t NextCodepoint(std::string_view text, std::size_t caret) {
    if (caret >= text.size()) return text.size();
    ++caret;
    while (caret < text.size() && (static_cast<unsigned char>(text[caret]) & 0xC0) == 0x80) ++caret;
    return caret;
}

} // namespace

// ── Input ───────────────────────────────────────────────────────────────────

void InputState::newFrame() {
    for (bool &value : pressed) value = false;
    for (bool &value : released) value = false;
    for (bool &value : doubleClick) value = false;
    wheel = 0.0f;
    mouseDelta = {};
    chars.clear();
    keyPresses.clear();
}

bool InputState::keyPressed(sgl::Key key) const {
    return std::find(keyPresses.begin(), keyPresses.end(), key) != keyPresses.end();
}

// ── Textur ──────────────────────────────────────────────────────────────────

Texture::~Texture() { destroy(); }

Texture::Texture(Texture&& other) noexcept
    : id(other.id), width(other.width), height(other.height) {
    other.id = 0;
    other.width = 0;
    other.height = 0;
}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        destroy();
        id = other.id;
        width = other.width;
        height = other.height;
        other.id = 0;
        other.width = 0;
        other.height = 0;
    }
    return *this;
}

void Texture::destroy() {
    if (id != 0) {
        gl::glDeleteTextures(1, &id);
    }
    id = 0;
    width = 0;
    height = 0;
}

Texture Texture::fromImage(const sgl::Image &image, bool pixelArt) {
    Texture texture;
    if (image.isEmpty()) return texture;
    if (gl::glGenTextures == nullptr || gl::glTexImage2D == nullptr) return texture;

    GLenum format = GL_RGBA;
    GLint internal = GL_RGBA8;
    switch (image.format()) {
    case sgl::ImageFormat::Gray: format = GL_RED; internal = GL_R8; break;
    case sgl::ImageFormat::GrayAlpha: format = GL_RG; internal = GL_RG8; break;
    case sgl::ImageFormat::Rgb: format = GL_RGB; internal = GL_RGB8; break;
    case sgl::ImageFormat::Rgba: format = GL_RGBA; internal = GL_RGBA8; break;
    }

    GLint previousTexture = 0;
    gl::glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
    gl::glGenTextures(1, &texture.id);
    if (texture.id == 0) return texture;
    gl::glBindTexture(GL_TEXTURE_2D, texture.id);
    gl::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                        pixelArt ? GL_NEAREST : GL_LINEAR);
    gl::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                        pixelArt ? GL_NEAREST : GL_LINEAR);
    gl::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl::glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    gl::glTexImage2D(GL_TEXTURE_2D, 0, internal, image.width(), image.height(), 0, format,
                     GL_UNSIGNED_BYTE, image.data());
    gl::glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));
    texture.width = image.width();
    texture.height = image.height();
    return texture;
}

// ── Renderer ────────────────────────────────────────────────────────────────

Renderer2d::~Renderer2d() { shutdown(); }

bool Renderer2d::init(std::string &error) {
    if (gl::glGenVertexArrays == nullptr || gl::glGenBuffers == nullptr ||
        gl::glCreateShader == nullptr) {
        error = "OpenGL-Funktionen wurden nicht geladen.";
        return false;
    }
    try {
        sgl::ShaderSourceOptions options;
        options.version = 330;
        m_shader = sgl::Shader::FromSource(kUiShader, options);
    } catch (const std::exception &exception) {
        error = std::string("UI-Shader: ") + exception.what();
        return false;
    }
    if (!m_shader.isValid()) {
        error = "UI-Shader konnte nicht gelinkt werden.";
        return false;
    }

    gl::glGenVertexArrays(1, &m_vao);
    gl::glGenBuffers(1, &m_vbo);
    gl::glBindVertexArray(m_vao);
    gl::glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    gl::glEnableVertexAttribArray(0);
    gl::glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
    gl::glEnableVertexAttribArray(1);
    gl::glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              reinterpret_cast<void *>(sizeof(f32) * 2));
    gl::glEnableVertexAttribArray(2);
    gl::glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              reinterpret_cast<void *>(sizeof(f32) * 4));
    gl::glBindVertexArray(0);

    const unsigned char white[4] = {255, 255, 255, 255};
    gl::glGenTextures(1, &m_white);
    gl::glBindTexture(GL_TEXTURE_2D, m_white);
    gl::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl::glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    gl::glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    gl::glBindTexture(GL_TEXTURE_2D, 0);

    try {
        m_font = sgl::Font2d::FromFile(SGL_EDITOR_FONT);
    } catch (const std::exception &exception) {
        error = std::string("UI-Schrift konnte nicht geladen werden: ") + exception.what();
        return false;
    }
    if (!m_font.isValid()) {
        error = "UI-Schrift ist ungültig: " SGL_EDITOR_FONT;
        return false;
    }
    return true;
}

void Renderer2d::shutdown() {
    if (m_vbo != 0) gl::glDeleteBuffers(1, &m_vbo);
    if (m_vao != 0) gl::glDeleteVertexArrays(1, &m_vao);
    if (m_white != 0) gl::glDeleteTextures(1, &m_white);
    m_vbo = 0;
    m_vao = 0;
    m_white = 0;
    m_vertices.clear();
    m_clips.clear();
    m_glyphs.clear();
}

void Renderer2d::beginFrame(int framebufferWidth, int framebufferHeight, f32 contentScale,
                            f32 logicalWidth, f32 logicalHeight) {
    m_fbWidth = std::max(framebufferWidth, 1);
    m_fbHeight = std::max(framebufferHeight, 1);
    m_scale = contentScale > 0.0f ? contentScale : 1.0f;
    m_width = std::max(logicalWidth, 1.0f);
    m_height = std::max(logicalHeight, 1.0f);
    m_vertices.clear();
    m_clips.clear();
    m_boundTexture = m_white;
    gl::glViewport(0, 0, m_fbWidth, m_fbHeight);
    gl::glDisable(GL_DEPTH_TEST);
    gl::glDisable(GL_CULL_FACE);
    gl::glDisable(GL_SCISSOR_TEST);
    gl::glEnable(GL_BLEND);
    gl::glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    m_shader.use();
    m_shader.setUniform("uScreen", vec2(m_width, m_height));
    m_shader.setUniform("uTexture", 0);
    gl::glActiveTexture(GL_TEXTURE0);
    gl::glBindTexture(GL_TEXTURE_2D, m_white);
}

void Renderer2d::endFrame() { flush(); }

void Renderer2d::clear(const Color &color) {
    gl::glClearColor(static_cast<f32>(color.r) / 255.0f, static_cast<f32>(color.g) / 255.0f,
                     static_cast<f32>(color.b) / 255.0f, static_cast<f32>(color.a) / 255.0f);
    gl::glClear(GL_COLOR_BUFFER_BIT);
}

void Renderer2d::setTexture(GLuint texture) {
    const GLuint target = texture != 0 ? texture : m_white;
    if (target == m_boundTexture) return;
    flush();
    m_boundTexture = target;
    gl::glBindTexture(GL_TEXTURE_2D, target);
}

void Renderer2d::flush() {
    if (m_vertices.empty()) return;
    gl::glBindVertexArray(m_vao);
    gl::glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    gl::glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(m_vertices.size() * sizeof(Vertex)),
                     m_vertices.data(), GL_STREAM_DRAW);
    gl::glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_vertices.size()));
    m_vertices.clear();
}

void Renderer2d::addQuad(const Rect &rect, f32 u0, f32 v0, f32 u1, f32 v1,
                         const Color &color) {
    const f32 r = static_cast<f32>(color.r) / 255.0f;
    const f32 g = static_cast<f32>(color.g) / 255.0f;
    const f32 b = static_cast<f32>(color.b) / 255.0f;
    const f32 a = static_cast<f32>(color.a) / 255.0f;
    const f32 x0 = rect.x;
    const f32 y0 = rect.y;
    const f32 x1 = rect.x + rect.w;
    const f32 y1 = rect.y + rect.h;
    m_vertices.push_back({x0, y0, u0, v0, r, g, b, a});
    m_vertices.push_back({x1, y0, u1, v0, r, g, b, a});
    m_vertices.push_back({x1, y1, u1, v1, r, g, b, a});
    m_vertices.push_back({x0, y0, u0, v0, r, g, b, a});
    m_vertices.push_back({x1, y1, u1, v1, r, g, b, a});
    m_vertices.push_back({x0, y1, u0, v1, r, g, b, a});
}

void Renderer2d::rect(const Rect &r, const Color &color) {
    if (r.w <= 0.0f || r.h <= 0.0f) return;
    setTexture(m_white);
    addQuad(r, 0.0f, 0.0f, 1.0f, 1.0f, color);
}

void Renderer2d::rectOutline(const Rect &r, const Color &color, f32 thickness) {
    if (r.w <= 0.0f || r.h <= 0.0f) return;
    const f32 t = std::max(thickness, 1.0f);
    rect({r.x, r.y, r.w, t}, color);
    rect({r.x, r.y + r.h - t, r.w, t}, color);
    rect({r.x, r.y + t, t, r.h - t * 2.0f}, color);
    rect({r.x + r.w - t, r.y + t, t, r.h - t * 2.0f}, color);
}

void Renderer2d::line(vec2 a, vec2 b, f32 thickness, const Color &color) {
    const f32 dx = b.x - a.x;
    const f32 dy = b.y - a.y;
    const f32 length = std::sqrt(dx * dx + dy * dy);
    if (length < 0.0001f) return;
    const f32 nx = -dy / length * thickness * 0.5f;
    const f32 ny = dx / length * thickness * 0.5f;
    setTexture(m_white);
    const f32 r = static_cast<f32>(color.r) / 255.0f;
    const f32 g = static_cast<f32>(color.g) / 255.0f;
    const f32 bl = static_cast<f32>(color.b) / 255.0f;
    const f32 al = static_cast<f32>(color.a) / 255.0f;
    auto vertex = [&](f32 x, f32 y) { m_vertices.push_back({x, y, 0.0f, 0.0f, r, g, bl, al}); };
    vertex(a.x + nx, a.y + ny);
    vertex(b.x + nx, b.y + ny);
    vertex(b.x - nx, b.y - ny);
    vertex(a.x + nx, a.y + ny);
    vertex(b.x - nx, b.y - ny);
    vertex(a.x - nx, a.y - ny);
}

void Renderer2d::triangle(vec2 a, vec2 b, vec2 c, const Color &color) {
    setTexture(m_white);
    const f32 r = static_cast<f32>(color.r) / 255.0f;
    const f32 g = static_cast<f32>(color.g) / 255.0f;
    const f32 bl = static_cast<f32>(color.b) / 255.0f;
    const f32 al = static_cast<f32>(color.a) / 255.0f;
    m_vertices.push_back({a.x, a.y, 0.0f, 0.0f, r, g, bl, al});
    m_vertices.push_back({b.x, b.y, 0.0f, 0.0f, r, g, bl, al});
    m_vertices.push_back({c.x, c.y, 0.0f, 0.0f, r, g, bl, al});
}

void Renderer2d::circle(vec2 center, f32 radius, const Color &color, int segments) {
    const int count = std::max(segments, 6);
    for (int i = 0; i < count; ++i) {
        const f32 a0 = static_cast<f32>(i) / count * 6.2831853f;
        const f32 a1 = static_cast<f32>(i + 1) / count * 6.2831853f;
        triangle(center, {center.x + std::cos(a0) * radius, center.y + std::sin(a0) * radius},
                 {center.x + std::cos(a1) * radius, center.y + std::sin(a1) * radius}, color);
    }
}

void Renderer2d::image(const Rect &r, const Texture &texture, const Color &tint, bool flipY) {
    if (!texture.isValid() || r.w <= 0.0f || r.h <= 0.0f) return;
    setTexture(texture.id);
    addQuad(r, 0.0f, flipY ? 1.0f : 0.0f, 1.0f, flipY ? 0.0f : 1.0f, tint);
}

void Renderer2d::imageSub(const Rect &r, const Texture &texture, int sx, int sy, int sw, int sh,
                          const Color &tint) {
    if (!texture.isValid() || r.w <= 0.0f || r.h <= 0.0f || sw <= 0 || sh <= 0) return;
    setTexture(texture.id);
    const f32 u0 = static_cast<f32>(sx) / static_cast<f32>(texture.width);
    const f32 v0 = static_cast<f32>(sy) / static_cast<f32>(texture.height);
    const f32 u1 = static_cast<f32>(sx + sw) / static_cast<f32>(texture.width);
    const f32 v1 = static_cast<f32>(sy + sh) / static_cast<f32>(texture.height);
    addQuad(r, u0, v0, u1, v1, tint);
}

void Renderer2d::imageQuad(vec2 p0, vec2 p1, vec2 p2, vec2 p3, const Color &tint) {
    setTexture(m_white);
    const f32 r = static_cast<f32>(tint.r) / 255.0f;
    const f32 g = static_cast<f32>(tint.g) / 255.0f;
    const f32 b = static_cast<f32>(tint.b) / 255.0f;
    const f32 a = static_cast<f32>(tint.a) / 255.0f;
    m_vertices.push_back({p0.x, p0.y, 0.0f, 0.0f, r, g, b, a});
    m_vertices.push_back({p1.x, p1.y, 1.0f, 0.0f, r, g, b, a});
    m_vertices.push_back({p2.x, p2.y, 1.0f, 1.0f, r, g, b, a});
    m_vertices.push_back({p0.x, p0.y, 0.0f, 0.0f, r, g, b, a});
    m_vertices.push_back({p2.x, p2.y, 1.0f, 1.0f, r, g, b, a});
    m_vertices.push_back({p3.x, p3.y, 0.0f, 1.0f, r, g, b, a});
}

void Renderer2d::text(vec2 position, std::string_view text, f32 size, const Color &color) {
    if (!m_font.isValid() || text.empty() || size <= 0.0f) return;
    setTexture(m_white);
    const f32 ascent = m_font.ascent();
    const f32 lineStep = m_font.lineHeight() * size;
    const f32 r = static_cast<f32>(color.r) / 255.0f;
    const f32 g = static_cast<f32>(color.g) / 255.0f;
    const f32 b = static_cast<f32>(color.b) / 255.0f;
    const f32 a = static_cast<f32>(color.a) / 255.0f;

    f32 penX = position.x;
    f32 baseline = position.y + ascent * size;
    const std::vector<u32> codepoints = sgl::DecodeUtf8(text);
    u32 previous = 0;
    for (const u32 codepoint : codepoints) {
        if (codepoint == '\n') {
            penX = position.x;
            baseline += lineStep;
            previous = 0;
            continue;
        }
        if (previous != 0) {
            penX += m_font.kerning(previous, codepoint) * size;
        }
        auto found = m_glyphs.find(codepoint);
        if (found == m_glyphs.end()) {
            sgl::GlyphMesh mesh;
            try {
                mesh = m_font.glyphMesh(codepoint, sgl::MeshQuality::Normal);
            } catch (...) {
                mesh = {};
            }
            found = m_glyphs.emplace(codepoint, std::move(mesh)).first;
        }
        const sgl::GlyphMesh &mesh = found->second;
        for (const u32 index : mesh.indices) {
            const vec2 point = mesh.positions[index];
            m_vertices.push_back({penX + point.x * size, baseline - point.y * size, 0.0f, 0.0f,
                                  r, g, b, a});
        }
        const f32 advance = m_font.advance(codepoint);
        penX += (advance > 0.0f ? advance : 0.5f) * size;
        previous = codepoint;
    }
}

void Renderer2d::textRect(const Rect &r, std::string_view content, f32 size,
                          const Color &color, int align) {
    if (content.empty()) return;
    const f32 width = textWidth(content, size);
    f32 x = r.x + 6.0f;
    if (align == 1) x = r.x + (r.w - width) * 0.5f;
    if (align == 2) x = r.x + r.w - width - 6.0f;
    const f32 lineStep = m_font.lineHeight() * size;
    const f32 y = r.y + (r.h - lineStep) * 0.5f;
    text({x, y}, content, size, color);
}

f32 Renderer2d::textWidth(std::string_view text, f32 size) const {
    if (!m_font.isValid() || text.empty()) return 0.0f;
    return m_font.measure(text, size);
}

f32 Renderer2d::lineHeight(f32 size) const {
    return m_font.isValid() ? m_font.lineHeight() * size : size;
}

void Renderer2d::applyScissor() {
    if (m_clips.empty()) {
        gl::glDisable(GL_SCISSOR_TEST);
        return;
    }
    const Rect &clip = m_clips.back();
    const int x = static_cast<int>(std::floor(clip.x * m_scale));
    const int y = static_cast<int>(std::floor(m_fbHeight - (clip.y + clip.h) * m_scale));
    const int w = static_cast<int>(std::ceil(clip.w * m_scale));
    const int h = static_cast<int>(std::ceil(clip.h * m_scale));
    gl::glEnable(GL_SCISSOR_TEST);
    gl::glScissor(std::max(x, 0), std::max(y, 0), std::max(w, 0), std::max(h, 0));
}

void Renderer2d::pushClip(const Rect &rect) {
    flush();
    Rect clip = rect;
    if (!m_clips.empty()) clip = clip.intersect(m_clips.back());
    m_clips.push_back(clip);
    applyScissor();
}

void Renderer2d::popClip() {
    flush();
    if (!m_clips.empty()) m_clips.pop_back();
    applyScissor();
}

void Renderer2d::icon(Icon icon, const Rect &r, const Color &color) {
    const f32 size = std::min(r.w, r.h);
    const f32 x = r.x + (r.w - size) * 0.5f;
    const f32 y = r.y + (r.h - size) * 0.5f;
    const f32 t = std::max(1.25f, size * 0.08f);
    const f32 cx = x + size * 0.5f;
    const f32 cy = y + size * 0.5f;
    const auto p = [&](f32 fx, f32 fy) { return vec2{x + fx * size, y + fy * size}; };
    switch (icon) {
    case Icon::Cursor:
        triangle(p(0.25f, 0.12f), p(0.25f, 0.82f), p(0.78f, 0.56f), color);
        line(p(0.48f, 0.55f), p(0.48f, 0.88f), t, color);
        break;
    case Icon::Brush:
        line(p(0.2f, 0.8f), p(0.65f, 0.3f), t * 2.0f, color);
        line(p(0.62f, 0.22f), p(0.78f, 0.38f), t * 2.0f, color);
        break;
    case Icon::Rect:
        rectOutline({p(0.18f, 0.22f).x, p(0.18f, 0.22f).y, size * 0.64f, size * 0.56f}, color, t);
        break;
    case Icon::Line:
        line(p(0.2f, 0.8f), p(0.8f, 0.2f), t, color);
        break;
    case Icon::Fill:
        triangle(p(0.2f, 0.42f), p(0.52f, 0.14f), p(0.52f, 0.7f), color);
        line(p(0.52f, 0.7f), p(0.82f, 0.8f), t, color);
        break;
    case Icon::Eraser:
        rectOutline({p(0.2f, 0.38f).x, p(0.2f, 0.38f).y, size * 0.5f, size * 0.36f}, color, t);
        line(p(0.18f, 0.8f), p(0.82f, 0.8f), t, color);
        break;
    case Icon::Picker:
        line(p(0.24f, 0.78f), p(0.7f, 0.3f), t, color);
        circle(p(0.74f, 0.24f), size * 0.1f, color);
        break;
    case Icon::Entity:
        circle({cx, cy}, size * 0.28f, color);
        break;
    case Icon::Pan:
        line(p(0.5f, 0.18f), p(0.5f, 0.82f), t, color);
        line(p(0.18f, 0.5f), p(0.82f, 0.5f), t, color);
        line(p(0.5f, 0.18f), p(0.4f, 0.3f), t, color);
        line(p(0.5f, 0.18f), p(0.6f, 0.3f), t, color);
        break;
    case Icon::Undo:
        line(p(0.28f, 0.4f), p(0.62f, 0.4f), t, color);
        line(p(0.28f, 0.4f), p(0.4f, 0.28f), t, color);
        line(p(0.28f, 0.4f), p(0.4f, 0.52f), t, color);
        line(p(0.62f, 0.4f), p(0.72f, 0.52f), t, color);
        break;
    case Icon::Redo:
        line(p(0.72f, 0.4f), p(0.38f, 0.4f), t, color);
        line(p(0.72f, 0.4f), p(0.6f, 0.28f), t, color);
        line(p(0.72f, 0.4f), p(0.6f, 0.52f), t, color);
        line(p(0.38f, 0.4f), p(0.28f, 0.52f), t, color);
        break;
    case Icon::Save:
        rectOutline({p(0.2f, 0.18f).x, p(0.2f, 0.18f).y, size * 0.6f, size * 0.64f}, color, t);
        line(p(0.34f, 0.18f), p(0.34f, 0.42f), t, color);
        line(p(0.34f, 0.42f), p(0.68f, 0.42f), t, color);
        break;
    case Icon::Open:
    case Icon::Folder:
        line(p(0.18f, 0.32f), p(0.18f, 0.78f), t, color);
        line(p(0.18f, 0.78f), p(0.82f, 0.78f), t, color);
        line(p(0.18f, 0.32f), p(0.42f, 0.32f), t, color);
        line(p(0.42f, 0.32f), p(0.5f, 0.4f), t, color);
        line(p(0.5f, 0.4f), p(0.82f, 0.4f), t, color);
        line(p(0.82f, 0.4f), p(0.82f, 0.78f), t, color);
        break;
    case Icon::Eye:
        line(p(0.16f, 0.5f), p(0.5f, 0.28f), t, color);
        line(p(0.5f, 0.28f), p(0.84f, 0.5f), t, color);
        line(p(0.16f, 0.5f), p(0.5f, 0.72f), t, color);
        line(p(0.5f, 0.72f), p(0.84f, 0.5f), t, color);
        circle({cx, cy}, size * 0.12f, color);
        break;
    case Icon::EyeOff:
        line(p(0.16f, 0.5f), p(0.5f, 0.28f), t, color);
        line(p(0.5f, 0.28f), p(0.84f, 0.5f), t, color);
        line(p(0.16f, 0.5f), p(0.5f, 0.72f), t, color);
        line(p(0.5f, 0.72f), p(0.84f, 0.5f), t, color);
        line(p(0.2f, 0.2f), p(0.8f, 0.8f), t, color);
        break;
    case Icon::Lock:
        rect({p(0.24f, 0.46f).x, p(0.24f, 0.46f).y, size * 0.52f, size * 0.36f}, color);
        line(p(0.34f, 0.46f), p(0.34f, 0.32f), t, color);
        line(p(0.34f, 0.32f), p(0.66f, 0.32f), t, color);
        line(p(0.66f, 0.32f), p(0.66f, 0.46f), t, color);
        break;
    case Icon::Unlock:
        rect({p(0.24f, 0.46f).x, p(0.24f, 0.46f).y, size * 0.52f, size * 0.36f}, color);
        line(p(0.34f, 0.46f), p(0.34f, 0.28f), t, color);
        line(p(0.34f, 0.28f), p(0.6f, 0.2f), t, color);
        break;
    case Icon::Plus:
        line(p(0.5f, 0.22f), p(0.5f, 0.78f), t, color);
        line(p(0.22f, 0.5f), p(0.78f, 0.5f), t, color);
        break;
    case Icon::Trash:
        rectOutline({p(0.28f, 0.3f).x, p(0.28f, 0.3f).y, size * 0.44f, size * 0.5f}, color, t);
        line(p(0.2f, 0.26f), p(0.8f, 0.26f), t, color);
        line(p(0.4f, 0.26f), p(0.4f, 0.16f), t, color);
        line(p(0.6f, 0.26f), p(0.6f, 0.16f), t, color);
        break;
    case Icon::Up:
        line(p(0.2f, 0.62f), p(0.5f, 0.32f), t, color);
        line(p(0.5f, 0.32f), p(0.8f, 0.62f), t, color);
        break;
    case Icon::Down:
        line(p(0.2f, 0.38f), p(0.5f, 0.68f), t, color);
        line(p(0.5f, 0.68f), p(0.8f, 0.38f), t, color);
        break;
    case Icon::Grid:
        line(p(0.18f, 0.4f), p(0.82f, 0.4f), t, color);
        line(p(0.18f, 0.6f), p(0.82f, 0.6f), t, color);
        line(p(0.4f, 0.18f), p(0.4f, 0.82f), t, color);
        line(p(0.6f, 0.18f), p(0.6f, 0.82f), t, color);
        break;
    case Icon::ZoomIn:
    case Icon::ZoomOut:
        circle({x + size * 0.42f, y + size * 0.42f}, size * 0.24f, color, 18);
        line(p(0.6f, 0.6f), p(0.82f, 0.82f), t * 1.5f, color);
        line(p(0.3f, 0.42f), p(0.54f, 0.42f), t * 0.8f, color);
        if (icon == Icon::ZoomIn) line(p(0.42f, 0.3f), p(0.42f, 0.54f), t * 0.8f, color);
        break;
    case Icon::Play:
        triangle(p(0.3f, 0.18f), p(0.3f, 0.82f), p(0.82f, 0.5f), color);
        break;
    case Icon::Settings:
        circle({cx, cy}, size * 0.2f, color, 16);
        for (int i = 0; i < 6; ++i) {
            const f32 angle = static_cast<f32>(i) * 1.0471976f;
            line({cx + std::cos(angle) * size * 0.24f, cy + std::sin(angle) * size * 0.24f},
                 {cx + std::cos(angle) * size * 0.34f, cy + std::sin(angle) * size * 0.34f}, t,
                 color);
        }
        break;
    case Icon::Close:
        line(p(0.24f, 0.24f), p(0.76f, 0.76f), t, color);
        line(p(0.76f, 0.24f), p(0.24f, 0.76f), t, color);
        break;
    case Icon::Check:
        line(p(0.2f, 0.52f), p(0.42f, 0.74f), t, color);
        line(p(0.42f, 0.74f), p(0.82f, 0.28f), t, color);
        break;
    case Icon::ChevronDown:
        line(p(0.28f, 0.42f), p(0.5f, 0.62f), t, color);
        line(p(0.5f, 0.62f), p(0.72f, 0.42f), t, color);
        break;
    case Icon::None:
        break;
    }
}

// ── Ui ──────────────────────────────────────────────────────────────────────

void Ui::beginFrame(InputState &input, Renderer2d &renderer, const Theme &theme,
                    const Rect &screen) {
    m_input = &input;
    m_renderer = &renderer;
    m_theme = theme;
    m_screen = screen;
    m_hot = 0;
    m_tooltip.clear();
    m_scrollActive = false;
    m_idDepth = 0;
    m_idStack[0] = 0;
    if (input.pressed[0] && m_focus != 0 && !m_focusRect.contains(input.mouse)) {
        m_focus = 0;
    }
}

void Ui::endFrame() {
    if (m_combo.opened) drawComboPopup();

    if (!m_tooltip.empty() && m_input != nullptr) {
        const f32 width = m_renderer->textWidth(m_tooltip, m_theme.fontSmall) + 16.0f;
        const f32 height = m_theme.fontSmall + 12.0f;
        Rect box{m_tooltipPos.x + 14.0f, m_tooltipPos.y + 18.0f, width, height};
        if (box.x + box.w > m_screen.x + m_screen.w) box.x = m_screen.x + m_screen.w - box.w - 4.0f;
        if (box.y + box.h > m_screen.y + m_screen.h) box.y = m_input->mouse.y - box.h - 8.0f;
        m_renderer->rect(box, Color{18, 20, 25, 245});
        m_renderer->rectOutline(box, m_theme.border, 1.0f);
        m_renderer->textRect(box, m_tooltip, m_theme.fontSmall, m_theme.text);
    }
}

void Ui::pushId(std::string_view text) {
    const UiId parent = m_idStack[std::max(m_idDepth, 0)];
    const UiId value = parent ^ (HashText(text) + 0x9e3779b97f4a7c15ull + (parent << 6) + (parent >> 2));
    m_idDepth = std::min(m_idDepth + 1, 31);
    m_idStack[m_idDepth] = value;
}

void Ui::popId() {
    if (m_idDepth > 0) --m_idDepth;
}

UiId Ui::makeId(std::string_view text) const {
    const UiId parent = m_idStack[std::max(m_idDepth, 0)];
    return parent ^ (HashText(text) + 0x9e3779b97f4a7c15ull + (parent << 6) + (parent >> 2));
}

bool Ui::hovered(const Rect &rect) const {
    if (m_input == nullptr || m_inputBlocked) return false;
    if (m_combo.opened && !m_insideCombo) return false;
    return rect.contains(m_input->mouse);
}

void Ui::drawFrame(const Rect &rect, const Color &fill, const Color &border, bool hot, bool active) {
    m_renderer->rect(rect, fill);
    m_renderer->rectOutline(rect, active ? m_theme.accent : (hot ? m_theme.accentDim : border), 1.0f);
}

void Ui::panel(const Rect &rect, std::string_view title) {
    m_renderer->rect(rect, m_theme.panel);
    m_renderer->rectOutline(rect, m_theme.border, 1.0f);
    if (!title.empty()) {
        const Rect header = rect.top(24.0f);
        m_renderer->rect(header, m_theme.header);
        m_renderer->line({header.x, header.y + header.h - 1.0f},
                         {header.x + header.w, header.y + header.h - 1.0f}, 1.0f, m_theme.border);
        m_renderer->textRect(header.inset(8.0f, 0.0f), title, m_theme.fontNormal, m_theme.text);
    }
}

void Ui::separator(const Rect &rect) {
    m_renderer->line({rect.x, rect.y + rect.h * 0.5f},
                     {rect.x + rect.w, rect.y + rect.h * 0.5f}, 1.0f, m_theme.border);
}

void Ui::label(const Rect &rect, std::string_view text, const Color &color, int align) {
    m_renderer->textRect(rect, text, m_theme.fontNormal, color, align);
}

bool Ui::button(const Rect &rect, std::string_view text, bool enabled, bool accent) {
    const UiId id = makeId(text);
    const bool isHovered = enabled && hovered(rect);
    if (isHovered) m_hot = id;
    bool clicked = false;
    if (isHovered && m_input->pressed[0]) m_active = id;
    if (m_active == id) {
        if (m_input->released[0]) {
            clicked = isHovered;
            m_active = 0;
        } else if (!m_input->down[0]) {
            m_active = 0;
        }
    }
    const bool pressed = m_active == id;
    Color fill = accent ? m_theme.accent : m_theme.panelAlt;
    if (!enabled) fill = Color{static_cast<u8>(fill.r / 2), static_cast<u8>(fill.g / 2),
                                  static_cast<u8>(fill.b / 2), 255};
    else if (pressed) fill = accent ? m_theme.accentHover : m_theme.selection;
    else if (isHovered) fill = accent ? m_theme.accentHover : Color{50, 56, 68, 255};
    m_renderer->rect(rect, fill);
    m_renderer->rectOutline(rect, isHovered ? m_theme.accentDim : m_theme.border, 1.0f);
    m_renderer->textRect(rect, text, m_theme.fontNormal,
                         enabled ? m_theme.text : m_theme.textFaint, 1);
    return clicked;
}

bool Ui::iconButton(const Rect &rect, Icon icon, std::string_view tip, bool active,
                    bool enabled) {
    const UiId id = makeId(tip);
    const bool isHovered = enabled && hovered(rect);
    if (isHovered) m_hot = id;
    bool clicked = false;
    if (isHovered && m_input->pressed[0]) m_active = id;
    if (m_active == id) {
        if (m_input->released[0]) {
            clicked = isHovered;
            m_active = 0;
        } else if (!m_input->down[0]) {
            m_active = 0;
        }
    }
    if (active) m_renderer->rect(rect, m_theme.selection);
    else if (isHovered) m_renderer->rect(rect, m_theme.panelAlt);
    m_renderer->icon(icon, rect.inset(4.0f), enabled ? m_theme.text : m_theme.textFaint);
    if (isHovered && !tip.empty()) tooltip(tip);
    return clicked;
}

bool Ui::checkbox(const Rect &rect, std::string_view text, bool &value) {
    const UiId id = makeId(text);
    const bool isHovered = hovered(rect);
    if (isHovered) m_hot = id;
    bool changed = false;
    if (isHovered && m_input->pressed[0]) {
        value = !value;
        changed = true;
    }
    const Rect box{rect.x, rect.y + (rect.h - 16.0f) * 0.5f, 16.0f, 16.0f};
    m_renderer->rect(box, value ? m_theme.accent : m_theme.panelAlt);
    m_renderer->rectOutline(box, isHovered ? m_theme.accentHover : m_theme.border, 1.0f);
    if (value) m_renderer->icon(Icon::Check, box.inset(2.0f), m_theme.text);
    m_renderer->textRect({rect.x + 24.0f, rect.y, rect.w - 24.0f, rect.h}, text,
                         m_theme.fontNormal, m_theme.text);
    return changed;
}

bool Ui::radio(const Rect &rect, std::string_view text, int &value, int option) {
    const bool isHovered = hovered(rect);
    bool changed = false;
    if (isHovered && m_input->pressed[0] && value != option) {
        value = option;
        changed = true;
    }
    const vec2 center{rect.x + 8.0f, rect.y + rect.h * 0.5f};
    m_renderer->circle(center, 7.0f, value == option ? m_theme.accent : m_theme.panelAlt, 16);
    m_renderer->circle(center, 7.0f, m_theme.border, 16);
    if (value == option) m_renderer->circle(center, 3.0f, m_theme.text, 12);
    m_renderer->textRect({rect.x + 22.0f, rect.y, rect.w - 22.0f, rect.h}, text,
                         m_theme.fontNormal, m_theme.text);
    return changed;
}

bool Ui::slider(const Rect &rect, std::string_view text, f32 &value, f32 minimum, f32 maximum) {
    const UiId id = makeId(text);
    const Rect track = rect.bottom(18.0f);
    const bool isHovered = hovered(track);
    if (isHovered) m_hot = id;
    if (isHovered && m_input->pressed[0]) m_active = id;
    bool changed = false;
    if (m_active == id && m_input->down[0]) {
        const f32 t = std::clamp((m_input->mouse.x - track.x) / std::max(track.w, 1.0f), 0.0f, 1.0f);
        const f32 next = minimum + (maximum - minimum) * t;
        if (next != value) {
            value = next;
            changed = true;
        }
    }
    if (m_input->released[0] && m_active == id) m_active = 0;
    m_renderer->textRect({rect.x, rect.y, rect.w, 18.0f}, text, m_theme.fontSmall,
                         m_theme.textDim);
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.2f", static_cast<double>(value));
    m_renderer->textRect({rect.x, rect.y, rect.w, 18.0f}, buffer, m_theme.fontSmall,
                         m_theme.text, 2);
    m_renderer->rect(track, m_theme.panelAlt);
    const f32 t = (value - minimum) / std::max(maximum - minimum, 0.0001f);
    m_renderer->rect({track.x, track.y, track.w * std::clamp(t, 0.0f, 1.0f), track.h},
                     m_theme.accent);
    m_renderer->rectOutline(track, m_theme.border, 1.0f);
    return changed;
}

bool Ui::dragFloat(const Rect &rect, std::string_view text, f32 &value, f32 step, f32 minimum,
                   f32 maximum, int decimals) {
    const UiId id = makeId(text);
    const bool isHovered = hovered(rect);
    if (isHovered) m_hot = id;
    if (isHovered && m_input->pressed[0]) m_active = id;
    bool changed = false;
    if (m_active == id && m_input->down[0] && std::abs(m_input->mouseDelta.x) > 0.0f) {
        const f32 multiplier = m_input->shift ? 0.1f : (m_input->control ? 10.0f : 1.0f);
        value = std::clamp(value + m_input->mouseDelta.x * step * multiplier, minimum, maximum);
        changed = true;
    }
    if (m_input->released[0] && m_active == id) m_active = 0;
    drawFrame(rect, m_theme.panelAlt, m_theme.border, isHovered, m_active == id);
    m_renderer->textRect({rect.x + 6.0f, rect.y, rect.w - 12.0f, rect.h}, text,
                         m_theme.fontSmall, m_theme.textDim);
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.*f", decimals, static_cast<double>(value));
    m_renderer->textRect({rect.x + 6.0f, rect.y, rect.w - 12.0f, rect.h}, buffer,
                         m_theme.fontSmall, m_theme.text, 2);
    return changed;
}

bool Ui::dragInt(const Rect &rect, std::string_view text, int &value, int minimum, int maximum) {
    const UiId id = makeId(text);
    const bool isHovered = hovered(rect);
    if (isHovered) m_hot = id;
    if (isHovered && m_input->pressed[0]) m_active = id;
    bool changed = false;
    if (m_active == id && m_input->down[0] && m_input->mouseDelta.x != 0.0f) {
        const int delta = static_cast<int>(std::lround(m_input->mouseDelta.x * 0.25f));
        if (delta != 0) {
            value = std::clamp(value + delta, minimum, maximum);
            changed = true;
        }
    }
    if (m_input->released[0] && m_active == id) m_active = 0;
    drawFrame(rect, m_theme.panelAlt, m_theme.border, isHovered, m_active == id);
    m_renderer->textRect({rect.x + 6.0f, rect.y, rect.w - 12.0f, rect.h}, text,
                         m_theme.fontSmall, m_theme.textDim);
    m_renderer->textRect({rect.x + 6.0f, rect.y, rect.w - 12.0f, rect.h},
                         std::to_string(value), m_theme.fontSmall, m_theme.text, 2);
    return changed;
}

void Ui::insertText(std::string &text, std::size_t &caret, u32 codepoint) {
    const std::string encoded = sgl::EncodeUtf8(codepoint);
    text.insert(caret, encoded);
    caret += encoded.size();
}

void Ui::eraseBefore(std::string &text, std::size_t &caret) {
    if (caret == 0) return;
    const std::size_t start = PreviousCodepoint(text, caret);
    text.erase(start, caret - start);
    caret = start;
}

void Ui::eraseAfter(std::string &text, std::size_t &caret) {
    if (caret >= text.size()) return;
    const std::size_t end = NextCodepoint(text, caret);
    text.erase(caret, end - caret);
}

bool Ui::textField(const Rect &rect, std::string_view label, std::string &text, bool enabled) {
    const UiId id = makeId(label);
    TextState &state = m_text[id];
    const bool isHovered = enabled && hovered(rect);
    if (isHovered) m_hot = id;

    if (isHovered && m_input->pressed[0]) {
        m_focus = id;
        state.caret = text.size();
        state.scroll = 0.0f;
    }
    if (m_focus == id) {
        m_focusRect = rect;
    }

    bool changed = false;
    if (m_focus == id && enabled) {
        if (m_input->control && m_input->keyPressed(sgl::Key::V) && m_getClipboard) {
            std::string clip = m_getClipboard();
            if (!clip.empty()) {
                text.insert(state.caret, clip);
                state.caret += clip.size();
                changed = true;
            }
        }
        for (const u32 codepoint : m_input->chars) {
            if (codepoint >= 32 && codepoint != 127) {
                insertText(text, state.caret, codepoint);
                changed = true;
            }
        }
        for (const sgl::Key key : m_input->keyPresses) {
            switch (key) {
            case sgl::Key::Backspace: eraseBefore(text, state.caret); changed = true; break;
            case sgl::Key::Delete: eraseAfter(text, state.caret); changed = true; break;
            case sgl::Key::Left:
                if (m_input->shift) state.caret = 0;
                else state.caret = PreviousCodepoint(text, state.caret);
                break;
            case sgl::Key::Right:
                if (m_input->shift) state.caret = text.size();
                else state.caret = NextCodepoint(text, state.caret);
                break;
            case sgl::Key::Home: state.caret = 0; break;
            case sgl::Key::End: state.caret = text.size(); break;
            case sgl::Key::Enter:
            case sgl::Key::Escape:
            case sgl::Key::KPEnter:
                m_focus = 0;
                break;
            default: break;
            }
        }
    }
    if (m_focus == id && m_input->control && m_input->keyPressed(sgl::Key::C) && m_setClipboard) {
        m_setClipboard(text);
    }
    if (m_focus == id && m_input->control && m_input->keyPressed(sgl::Key::X) && m_setClipboard) {
        m_setClipboard(text);
        text.clear();
        state.caret = 0;
        changed = true;
    }

    const bool focused = m_focus == id;
    drawFrame(rect, m_theme.panelAlt, m_theme.border, isHovered, focused);
    const Rect inner = rect.inset(6.0f, 4.0f);
    if (!label.empty() && text.empty() && !focused) {
        m_renderer->textRect(inner, label, m_theme.fontNormal, m_theme.textFaint);
    } else {
        const f32 prefixWidth = m_renderer->textWidth(text.substr(0, state.caret), m_theme.fontNormal);
        const f32 available = std::max(inner.w - 4.0f, 1.0f);
        if (prefixWidth - state.scroll > available) state.scroll = prefixWidth - available;
        if (prefixWidth < state.scroll) state.scroll = std::max(prefixWidth - 20.0f, 0.0f);
        m_renderer->pushClip(inner);
        m_renderer->text({inner.x - state.scroll, inner.y}, text, m_theme.fontNormal,
                         enabled ? m_theme.text : m_theme.textFaint);
        if (focused && std::fmod(sgl::GetTime(), 1.0) < 0.5) {
            m_renderer->rect({inner.x + prefixWidth - state.scroll, inner.y - 1.0f, 1.5f,
                              inner.h + 2.0f},
                             m_theme.accentHover);
        }
        m_renderer->popClip();
    }
    if (isHovered) {
        if (m_input->pressed[0]) tooltip("Text bearbeiten");
    }
    return changed;
}

bool Ui::combo(const Rect &rect, std::string_view label, const std::vector<std::string> &items,
               int &selected) {
    const UiId id = makeId(label);
    const bool wasBlocked = m_inputBlocked;
    if (m_combo.opened && m_combo.id == id) m_inputBlocked = false;
    m_insideCombo = true;
    bool applied = false;
    if (m_combo.id == id && m_combo.pending) {
        m_combo.pending = false;
        if (m_combo.selected != selected) {
            selected = m_combo.selected;
            applied = true;
        }
    }
    const bool isHovered = hovered(rect);
    if (isHovered) m_hot = id;
    if (isHovered && m_input->pressed[0]) {
        m_combo.id = id;
        m_combo.anchor = rect;
        m_combo.items = items;
        m_combo.selected = selected;
        m_combo.pending = false;
        m_combo.opened = !m_combo.opened;
    }
    if (m_combo.opened && m_combo.id == id) {
        m_combo.items = items;
        m_combo.anchor = rect;
    }
    drawFrame(rect, m_theme.panelAlt, m_theme.border, isHovered, m_combo.opened && m_combo.id == id);
    const std::string value = selected >= 0 && selected < static_cast<int>(items.size())
                                  ? items[static_cast<std::size_t>(selected)]
                                  : std::string{};
    m_renderer->textRect({rect.x + 6.0f, rect.y, rect.w - 28.0f, rect.h}, value,
                         m_theme.fontNormal, m_theme.text);
    m_renderer->icon(Icon::ChevronDown, {rect.x + rect.w - 24.0f, rect.y, 24.0f, rect.h},
                     m_theme.textDim);
    m_insideCombo = false;
    m_inputBlocked = wasBlocked;
    return applied;
}

void Ui::drawComboPopup() {
    if (m_combo.items.empty()) {
        m_combo.opened = false;
        return;
    }
    const f32 rowHeight = 24.0f;
    const f32 width = std::max(m_combo.anchor.w, 120.0f);
    f32 y = m_combo.anchor.y + m_combo.anchor.h;
    const f32 height = rowHeight * static_cast<f32>(m_combo.items.size()) + 8.0f;
    if (y + height > m_screen.y + m_screen.h) y = m_combo.anchor.y - height;
    const Rect popup{m_combo.anchor.x, y, width, height};
    m_renderer->rect(popup, m_theme.panelAlt);
    m_renderer->rectOutline(popup, m_theme.border, 1.0f);
    for (std::size_t i = 0; i < m_combo.items.size(); ++i) {
        const Rect row{popup.x + 4.0f, popup.y + 4.0f + rowHeight * static_cast<f32>(i),
                       popup.w - 8.0f, rowHeight};
        const bool isHovered = m_input != nullptr && row.contains(m_input->mouse);
        if (isHovered) m_renderer->rect(row, m_theme.selection);
        if (static_cast<int>(i) == m_combo.selected) {
            m_renderer->rect({row.x, row.y, 3.0f, row.h}, m_theme.accent);
        }
        m_renderer->textRect(row, m_combo.items[i], m_theme.fontNormal, m_theme.text);
        if (isHovered && m_input->released[0]) {
            m_combo.selected = static_cast<int>(i);
            m_combo.pending = true;
            m_combo.opened = false;
        }
    }
    if (m_input != nullptr && m_input->pressed[0] && !popup.contains(m_input->mouse) &&
        !m_combo.anchor.contains(m_input->mouse)) {
        m_combo.opened = false;
    }
}

bool Ui::selectable(const Rect &rect, std::string_view text, bool selected, int depth) {
    const UiId id = makeId(text);
    const bool isHovered = hovered(rect);
    if (isHovered) m_hot = id;
    bool clicked = false;
    if (isHovered && m_input->pressed[0]) clicked = true;
    if (selected) m_renderer->rect(rect, m_theme.selection);
    else if (isHovered) m_renderer->rect(rect, m_theme.panelAlt);
    m_renderer->textRect({rect.x + 8.0f + depth * 14.0f, rect.y,
                          rect.w - 8.0f - depth * 14.0f, rect.h},
                         text, m_theme.fontNormal, selected ? m_theme.text : m_theme.textDim);
    return clicked;
}

bool Ui::tab(const Rect &rect, std::string_view text, bool active) {
    const UiId id = makeId(text);
    const bool isHovered = hovered(rect);
    if (isHovered) m_hot = id;
    bool clicked = false;
    if (isHovered && m_input->pressed[0]) clicked = true;
    m_renderer->rect(rect, active ? m_theme.panel : (isHovered ? m_theme.panelAlt : m_theme.header));
    if (active) m_renderer->rect(rect.bottom(2.0f), m_theme.accent);
    m_renderer->textRect(rect, text, m_theme.fontNormal, active ? m_theme.text : m_theme.textDim, 1);
    return clicked;
}

bool Ui::colorSwatch(const Rect &rect, const Color &color, bool *open) {
    const bool isHovered = hovered(rect);
    const f32 cell = 6.0f;
    for (f32 y = 0.0f; y < rect.h; y += cell) {
        for (f32 x = 0.0f; x < rect.w; x += cell) {
            const bool even = (static_cast<int>((x + rect.x) / cell) +
                               static_cast<int>((y + rect.y) / cell)) % 2 == 0;
            m_renderer->rect({rect.x + x, rect.y + y, std::min(cell, rect.w - x),
                              std::min(cell, rect.h - y)},
                             even ? m_theme.checkerA : m_theme.checkerB);
        }
    }
    m_renderer->rect(rect, color);
    m_renderer->rectOutline(rect, isHovered ? m_theme.accentHover : m_theme.border, 1.0f);
    if (isHovered && open != nullptr && m_input->pressed[0]) {
        *open = !*open;
        return true;
    }
    return false;
}

void Ui::tooltip(std::string_view text) {
    if (m_input == nullptr || text.empty()) return;
    m_tooltip = std::string(text);
    m_tooltipPos = m_input->mouse;
}

Rect Ui::beginScroll(const Rect &rect, float contentHeight) {
    const UiId id = makeId("##scroll");
    ScrollState &state = m_scroll[id];
    state.content = contentHeight;
    const bool isHovered = rect.contains(m_input->mouse) && !m_inputBlocked;
    if (isHovered && m_input->wheel != 0.0f) {
        state.offset -= m_input->wheel * 40.0f;
        m_input->wheel = 0.0f;
    }
    state.offset = std::clamp(state.offset, 0.0f, std::max(contentHeight - rect.h, 0.0f));
    m_renderer->pushClip(rect);
    m_scrollActive = true;
    m_scrollRect = rect;
    m_scrollId = id;
    m_scrollStart = state.offset;
    return {rect.x, rect.y - state.offset, rect.w, contentHeight};
}

void Ui::endScroll() {
    if (!m_scrollActive) return;
    m_renderer->popClip();
    ScrollState &state = m_scroll[m_scrollId];
    if (state.content > m_scrollRect.h) {
        const f32 barHeight = std::max(m_scrollRect.h * (m_scrollRect.h / state.content), 24.0f);
        const f32 maxOffset = std::max(state.content - m_scrollRect.h, 1.0f);
        const f32 t = std::clamp(state.offset / maxOffset, 0.0f, 1.0f);
        const Rect bar{m_scrollRect.x + m_scrollRect.w - 6.0f,
                       m_scrollRect.y + t * (m_scrollRect.h - barHeight), 4.0f, barHeight};
        m_renderer->rect(bar, m_theme.border);
    }
    m_scrollActive = false;
}

} // namespace editor::ui
