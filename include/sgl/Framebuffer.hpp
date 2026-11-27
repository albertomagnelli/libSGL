#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// Renderziele mit einem oder mehreren Farbanhängen, optional Tiefe und
// Stencil, wahlweise mehrfach abgetastet (MSAA 2x, 4x, 8x, 16x):
//
//   SampleCount: One, Two, Four, Eight, Sixteen
//   FramebufferOptions { width, height, colorFormat, samples, isDepth, isStencil,
//                        isSrgb, colorAttachments }
//   Framebuffer: Framebuffer(width, height, options), resize, bind, unbind,
//                clear, isComplete, colorTexture, colorRenderbuffer, readPixels,
//                width, height, samples, isMultisampled, hasDepth, hasStencil
//   Freie Funktionen: BlitFramebuffer(src, dst, ...), ResolveFramebuffer(src, dst)
//
// Einfach abgetastete Ziele legen ihre Farbanhänge als Texture2d an (direkt
// sampler-bar), mehrfach abgetastete als Renderbuffer. Auflösen geschieht mit
// ResolveFramebuffer() über glBlitFramebuffer.
//
// Beispiel:
//
//   Framebuffer msaa(1280, 720, {.samples = SampleCount::Four, .isDepth = true});
//   Framebuffer resolved(1280, 720);          // ein Abtastwert, Texture2d-Anhang
//   msaa.bind(); /* ... zeichnen ... */
//   ResolveFramebuffer(msaa, resolved);
//   resolved.colorTexture(0).use(0);
// ─────────────────────────────────────────────────────────────────────────────

#include "Types.hpp"

#include "Color.hpp"
#include "Geometry.hpp"
#include "Image.hpp"
#include "Texture.hpp"

#include <array>
#include <stdexcept>
#include <string>
#include <vector>

namespace sgl {

// ─── Fehler ────────────────────────────────────────────────────────────────

class FramebufferError : public std::runtime_error {
public:
    explicit FramebufferError(const std::string& message) : std::runtime_error(message) {}
};

// ─── Abtastrate ────────────────────────────────────────────────────────────

enum class SampleCount : u32 {
    One = 1,       // kein MSAA
    Two = 2,
    Four = 4,
    Eight = 8,
    Sixteen = 16
};

[[nodiscard]] SampleCount ClampSampleCount(SampleCount wanted);
[[nodiscard]] i32 ToSamples(SampleCount count) noexcept;
[[nodiscard]] const char* ToString(SampleCount count) noexcept;

// ─── Optionen ──────────────────────────────────────────────────────────────

struct FramebufferOptions {
    ImageFormat colorFormat{ImageFormat::Rgba};
    SampleCount samples{SampleCount::One};
    bool isDepth{true};
    // Stencil gibt es nur zusammen mit Tiefe (GL_DEPTH24_STENCIL8).
    bool isStencil{false};
    bool isSrgb{false};
    // HDR-Ziel: Farbanhang als RGBA16F statt RGBA8. Werte über 1.0 bleiben
    // erhalten (Postprocessing/Bloom). readPixels() ist dann nicht sinnvoll.
    bool isHdr{false};
    // Anzahl der Farbanhänge (MRT), 1..kMaxColorAttachments
    i32 colorAttachments{1};
};

inline constexpr i32 kMaxColorAttachments = 8;

// ─── Framebuffer ───────────────────────────────────────────────────────────

class Framebuffer {
public:
    Framebuffer() = default;
    Framebuffer(i32 width, i32 height, const FramebufferOptions& options = {});
    ~Framebuffer();

    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;
    Framebuffer(Framebuffer&& other) noexcept;
    Framebuffer& operator=(Framebuffer&& other) noexcept;

    // ── Zustand ─────────────────────────────────────────────────────────────
    [[nodiscard]] bool isValid() const noexcept { return m_id != 0; }
    [[nodiscard]] GLuint id() const noexcept { return m_id; }
    [[nodiscard]] i32 width() const noexcept { return m_width; }
    [[nodiscard]] i32 height() const noexcept { return m_height; }
    [[nodiscard]] const FramebufferOptions& options() const noexcept { return m_options; }
    [[nodiscard]] SampleCount samples() const noexcept { return m_options.samples; }
    [[nodiscard]] bool isMultisampled() const noexcept { return m_options.samples != SampleCount::One; }
    [[nodiscard]] bool hasDepth() const noexcept { return m_options.isDepth; }
    [[nodiscard]] bool hasStencil() const noexcept { return m_options.isStencil; }
    [[nodiscard]] i32 colorAttachmentCount() const noexcept { return m_options.colorAttachments; }
    [[nodiscard]] i32 sampleCount() const noexcept { return ToSamples(m_options.samples); }
    [[nodiscard]] bool isComplete() const;

    // ── Anhänge ─────────────────────────────────────────────────────────────
    // Nur bei einem Abtastwert: der Farbanhang als Textur (sampler-bar).
    // Bei MSAA wirft der Aufruf, dort gibt es Renderbuffer.
    [[nodiscard]] Texture2d& colorTexture(i32 index = 0);
    [[nodiscard]] const Texture2d& colorTexture(i32 index = 0) const;
    // Renderbuffer-Kennung des Farbanhangs (nur bei MSAA gesetzt, sonst 0).
    [[nodiscard]] GLuint colorRenderbuffer(i32 index = 0) const;
    // Renderbuffer-Kennung des Tiefe-/Stencil-Anhangs (0, wenn nicht vorhanden).
    [[nodiscard]] GLuint depthRenderbuffer() const noexcept { return m_depthBuffer; }

    // ── Binden und Zeichnen ─────────────────────────────────────────────────
    void bind() const;
    static void bindDefault();
    static void unbind() { bindDefault(); }
    // Setzt glViewport auf die Zielgröße.
    void setViewport() const;
    // Löscht Farbe(n), Tiefe und Stencil (je nach Anhängen).
    void clear(const Color& color = Color(0, 0, 0, 255), f32 depth = 1.0f, i32 stencil = 0) const;
    // Blendet die Farbanhänge als Zeichenziele ein (glDrawBuffers).
    void setDrawBuffers() const;

    // ── Größe ───────────────────────────────────────────────────────────────
    // Legt die Anhänge neu an; der Framebuffer bleibt dieselbe Kennung.
    void resize(i32 width, i32 height);

    // ── Auslesen ────────────────────────────────────────────────────────────
    // Nur bei einem Abtastwert sinnvoll; bei MSAA vorher auflösen.
    [[nodiscard]] Image readPixels(i32 attachment = 0) const;
    [[nodiscard]] bool readPixel(i32 x, i32 y, u8* rgba) const;

    // ── Vergleich (Tests, Prüfungen) ────────────────────────────────────────
    [[nodiscard]] std::string describe() const;

private:
    void allocate();
    void release() noexcept;

    GLuint m_id{0};
    GLuint m_depthBuffer{0};
    i32 m_width{0};
    i32 m_height{0};
    FramebufferOptions m_options{};
    // Bei einem Abtastwert: Texturen; bei MSAA: Renderbuffer-Kennungen.
    std::array<Texture2d, kMaxColorAttachments> m_colorTextures{};
    std::array<GLuint, kMaxColorAttachments> m_colorBuffers{};
};

// ─── Freie Funktionen ──────────────────────────────────────────────────────

// Kopiert Farbe (und wahlweise Tiefe/Stencil) von src nach dst.
// Bei mehreren Anhängen werden alle vorhandenen kopiert.
// filter: GL_NEAREST oder GL_LINEAR; für Tiefe/Stencil nur GL_NEAREST.
void BlitFramebuffer(const Framebuffer& src, Framebuffer& dst, bool isColor = true,
                     bool isDepth = false, GLenum filter = GL_NEAREST);
void BlitFramebuffer(const Framebuffer& src, const Rectangle& srcRect, Framebuffer& dst,
                     const Rectangle& dstRect, bool isColor = true, bool isDepth = false,
                     GLenum filter = GL_NEAREST);

// Löst ein mehrfach abgetastetes Ziel in ein einfach abgetastetes auf
// (Farbe, und Tiefe, wenn beide Seiten Tiefe haben).
void ResolveFramebuffer(const Framebuffer& src, Framebuffer& dst);

// Prüft, ob src und dst für einen Blit zusammenpassen, ohne zu werfen.
[[nodiscard]] bool CanBlitFramebuffer(const Framebuffer& src, const Framebuffer& dst);

}  // namespace sgl
