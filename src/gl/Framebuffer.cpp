#include <sgl/Framebuffer.hpp>

#include <sgl/GL.hpp>
#include <sgl/Log.hpp>

#include <algorithm>
#include <cstring>

namespace sgl {

using namespace GL;

namespace {

GLenum ColorFormat(const ImageFormat format, const bool isSrgb, const bool isHdr) {
    if (isHdr) {
        return GL_RGBA16F;
    }
    switch (format) {
        case ImageFormat::Gray:
            return GL_R8;  // für einen Kanal gibt es kein sRGB-Format in GL
        case ImageFormat::GrayAlpha:
            return isSrgb ? GL_SRGB8_ALPHA8 : GL_RG8;
        case ImageFormat::Rgb:
            return isSrgb ? GL_SRGB8 : GL_RGB8;
        case ImageFormat::Rgba:
            return isSrgb ? GL_SRGB8_ALPHA8 : GL_RGBA8;
    }
    return GL_RGBA8;
}

GLenum ColorPixelFormat(const ImageFormat format) {
    switch (format) {
        case ImageFormat::Gray:
            return GL_RED;
        case ImageFormat::GrayAlpha:
            return GL_RG;
        case ImageFormat::Rgb:
            return GL_RGB;
        case ImageFormat::Rgba:
            return GL_RGBA;
    }
    return GL_RGBA;
}

// Anzahl der Bytes je Pixel im Auslesepfad
i32 ColorChannels(const ImageFormat format) {
    return ChannelCount(format);
}

}  // namespace

SampleCount ClampSampleCount(const SampleCount wanted) {
    GLint maxSamples = 1;
    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
    const i32 clamped = std::min(ToSamples(wanted), static_cast<i32>(maxSamples));
    switch (clamped) {
        case 16:
        case 15:
            return clamped >= 16 ? SampleCount::Sixteen : SampleCount::Eight;
        case 8:
        case 7:
        case 6:
        case 5:
            return SampleCount::Eight;
        case 4:
        case 3:
            return SampleCount::Four;
        case 2:
            return SampleCount::Two;
        default:
            return SampleCount::One;
    }
}

i32 ToSamples(const SampleCount count) noexcept {
    return static_cast<i32>(count);
}

const char* ToString(SampleCount count) noexcept {
    switch (count) {
        case SampleCount::One:
            return "1x";
        case SampleCount::Two:
            return "2x";
        case SampleCount::Four:
            return "4x";
        case SampleCount::Eight:
            return "8x";
        case SampleCount::Sixteen:
            return "16x";
    }
    return "?";
}

// ─── Framebuffer ───────────────────────────────────────────────────────────

Framebuffer::Framebuffer(i32 width, i32 height, const FramebufferOptions& options)
    : m_width(width), m_height(height), m_options(options) {
    if (width <= 0 || height <= 0) {
        throw FramebufferError("Framebuffer: Maße müssen positiv sein (" + std::to_string(width) +
                               "x" + std::to_string(height) + ")");
    }
    if (m_options.colorAttachments < 1 || m_options.colorAttachments > kMaxColorAttachments) {
        throw FramebufferError("Framebuffer: Anzahl der Farbanhänge liegt außerhalb 1.." +
                               std::to_string(kMaxColorAttachments));
    }
    if (m_options.isStencil && !m_options.isDepth) {
        m_options.isDepth = true;  // Stencil gibt es nur zusammen mit Tiefe
    }
    m_options.samples = ClampSampleCount(m_options.samples);
    allocate();
    SLOGD << "Framebuffer " << width << "x" << height << " " << ToString(m_options.samples) << " "
         << (isMultisampled() ? "MSAA" : "ein Abtastwert") << ", "
         << m_options.colorAttachments << " Farbanhang/Anhänge"
         << (hasDepth() ? (hasStencil() ? ", Tiefe+Stencil" : ", Tiefe") : "");
}

Framebuffer::~Framebuffer() {
    release();
}

Framebuffer::Framebuffer(Framebuffer&& other) noexcept {
    *this = std::move(other);
}

Framebuffer& Framebuffer::operator=(Framebuffer&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    release();
    m_id = other.m_id;
    m_depthBuffer = other.m_depthBuffer;
    m_width = other.m_width;
    m_height = other.m_height;
    m_options = other.m_options;
    m_colorTextures = std::move(other.m_colorTextures);
    m_colorBuffers = other.m_colorBuffers;

    other.m_id = 0;
    other.m_depthBuffer = 0;
    other.m_width = 0;
    other.m_height = 0;
    other.m_colorBuffers.fill(0);
    return *this;
}

void Framebuffer::allocate() {
    if (m_id == 0) {
        glCreateFramebuffers(1, &m_id);
        if (m_id == 0) {
            throw FramebufferError("Framebuffer: glCreateFramebuffers ist fehlgeschlagen");
        }
    }

    std::vector<GLenum> drawBuffers;
    drawBuffers.reserve(static_cast<std::size_t>(m_options.colorAttachments));
    const GLenum internalFormat = ColorFormat(m_options.colorFormat, m_options.isSrgb, m_options.isHdr);

    for (i32 index = 0; index < m_options.colorAttachments; ++index) {
        if (isMultisampled()) {
            GLuint buffer = 0;
            glCreateRenderbuffers(1, &buffer);
            glNamedRenderbufferStorageMultisample(buffer, sampleCount(), internalFormat, m_width,
                                                  m_height);
            m_colorBuffers[static_cast<std::size_t>(index)] = buffer;
            glNamedFramebufferRenderbuffer(
                m_id, static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + index), GL_RENDERBUFFER, buffer);
        } else {
            TextureOptions textureOptions;
            textureOptions.mipmaps = MipmapMode::None;
            textureOptions.isSrgb = m_options.isSrgb;
            textureOptions.isHdr = m_options.isHdr;
            textureOptions.minFilter = TextureFilter::Linear;
            textureOptions.magFilter = TextureFilter::Linear;
            textureOptions.wrapS = TextureWrap::ClampToEdge;
            textureOptions.wrapT = TextureWrap::ClampToEdge;
            Texture2d texture =
                Texture2d::createStorage(m_width, m_height, m_options.colorFormat, textureOptions);
            glNamedFramebufferTexture(m_id, static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + index),
                                      texture.id(), 0);
            m_colorTextures[static_cast<std::size_t>(index)] = std::move(texture);
        }
        drawBuffers.push_back(static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + index));
    }

    if (hasDepth()) {
        glCreateRenderbuffers(1, &m_depthBuffer);
        const GLenum depthFormat = hasStencil() ? GL_DEPTH24_STENCIL8 : GL_DEPTH_COMPONENT24;
        if (isMultisampled()) {
            glNamedRenderbufferStorageMultisample(m_depthBuffer, sampleCount(), depthFormat, m_width,
                                                  m_height);
        } else {
            glNamedRenderbufferStorage(m_depthBuffer, depthFormat, m_width, m_height);
        }
        glNamedFramebufferRenderbuffer(
            m_id, hasStencil() ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT,
            GL_RENDERBUFFER, m_depthBuffer);
    }

    glNamedFramebufferDrawBuffers(m_id, static_cast<GLsizei>(drawBuffers.size()),
                                  drawBuffers.data());
    glNamedFramebufferReadBuffer(m_id, GL_COLOR_ATTACHMENT0);

    if (!isComplete()) {
        const GLenum status = glCheckNamedFramebufferStatus(m_id, GL_FRAMEBUFFER);
        release();
        throw FramebufferError("Framebuffer ist nicht vollständig (Status 0x" +
                               std::to_string(status) + ")");
    }
}

void Framebuffer::release() noexcept {
    for (auto& texture : m_colorTextures) {
        texture = Texture2d{};
    }
    for (auto& buffer : m_colorBuffers) {
        if (buffer != 0) {
            glDeleteRenderbuffers(1, &buffer);
            buffer = 0;
        }
    }
    if (m_depthBuffer != 0) {
        glDeleteRenderbuffers(1, &m_depthBuffer);
        m_depthBuffer = 0;
    }
    if (m_id != 0) {
        glDeleteFramebuffers(1, &m_id);
        m_id = 0;
    }
}

bool Framebuffer::isComplete() const {
    if (m_id == 0) {
        return false;
    }
    return glCheckNamedFramebufferStatus(m_id, GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

Texture2d& Framebuffer::colorTexture(const i32 index) {
    if (isMultisampled()) {
        throw FramebufferError(
            "Framebuffer: ein mehrfach abgetastetes Ziel hat Texturanhänge erst nach "
            "ResolveFramebuffer()");
    }
    if (index < 0 || index >= m_options.colorAttachments) {
        throw FramebufferError("Framebuffer: Farbanhang " + std::to_string(index) +
                               " liegt außerhalb 0.." +
                               std::to_string(m_options.colorAttachments - 1));
    }
    return m_colorTextures[static_cast<std::size_t>(index)];
}

const Texture2d& Framebuffer::colorTexture(const i32 index) const {
    return const_cast<Framebuffer*>(this)->colorTexture(index);
}

GLuint Framebuffer::colorRenderbuffer(const i32 index) const {
    if (index < 0 || index >= m_options.colorAttachments) {
        throw FramebufferError("Framebuffer: Farbanhang " + std::to_string(index) +
                               " liegt außerhalb 0.." +
                               std::to_string(m_options.colorAttachments - 1));
    }
    return m_colorBuffers[static_cast<std::size_t>(index)];
}

void Framebuffer::bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, m_id);
}

void Framebuffer::bindDefault() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::setViewport() const {
    glViewport(0, 0, m_width, m_height);
}

void Framebuffer::setDrawBuffers() const {
    std::vector<GLenum> buffers;
    buffers.reserve(static_cast<std::size_t>(m_options.colorAttachments));
    for (i32 index = 0; index < m_options.colorAttachments; ++index) {
        buffers.push_back(static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + index));
    }
    glNamedFramebufferDrawBuffers(m_id, static_cast<GLsizei>(buffers.size()), buffers.data());
}

void Framebuffer::clear(const Color& color, const f32 depth, const i32 stencil) const {
    if (m_id == 0) {
        return;
    }
    // DSA: benanntes Löschen, ohne das Ziel zu binden. Der Scissor-Test gilt
    // auch hier, deshalb kurz abschalten.
    const GLboolean isScissor = glIsEnabled(GL_SCISSOR_TEST);
    if (isScissor == GL_TRUE) {
        glDisable(GL_SCISSOR_TEST);
    }
    const GLfloat rgb[4] = {static_cast<GLfloat>(color.r) / 255.0f,
                            static_cast<GLfloat>(color.g) / 255.0f,
                            static_cast<GLfloat>(color.b) / 255.0f,
                            static_cast<GLfloat>(color.a) / 255.0f};
    for (i32 index = 0; index < m_options.colorAttachments; ++index) {
        glClearNamedFramebufferfv(m_id, GL_COLOR, index, rgb);
    }
    if (hasDepth()) {
        glClearNamedFramebufferfi(m_id, GL_DEPTH_STENCIL, 0, static_cast<GLfloat>(depth), stencil);
    }
    if (isScissor == GL_TRUE) {
        glEnable(GL_SCISSOR_TEST);
    }
}

void Framebuffer::resize(i32 width, i32 height) {
    if (width <= 0 || height <= 0) {
        throw FramebufferError("Framebuffer: Maße müssen positiv sein (" + std::to_string(width) +
                               "x" + std::to_string(height) + ")");
    }
    if (width == m_width && height == m_height) {
        return;
    }
    // Anhänge neu anlegen; die Framebuffer-Kennung bleibt erhalten, damit
    // bereits eingetragene Bindungspunkte gültig bleiben.
    for (auto& texture : m_colorTextures) {
        texture = Texture2d{};
    }
    for (auto& buffer : m_colorBuffers) {
        if (buffer != 0) {
            glDeleteRenderbuffers(1, &buffer);
            buffer = 0;
        }
    }
    if (m_depthBuffer != 0) {
        glDeleteRenderbuffers(1, &m_depthBuffer);
        m_depthBuffer = 0;
    }
    m_width = width;
    m_height = height;
    allocate();
}

Image Framebuffer::readPixels(const i32 attachment) const {
    const Texture2d& texture = colorTexture(attachment);
    return texture.readBack();
}

bool Framebuffer::readPixel(const i32 x, const i32 y, u8* rgba) const {
    if (rgba == nullptr || m_id == 0) {
        return false;
    }
    if (isMultisampled()) {
        return false;  // MSAA muss vorher aufgelöst werden
    }
    if (x < 0 || y < 0 || x >= m_width || y >= m_height) {
        return false;
    }
    const i32 channels = ColorChannels(m_options.colorFormat);
    u8 pixel[16] = {};
    // DSA: direkt aus dem Texturanhang lesen, ohne Read-Framebuffer zu binden.
    const GLuint texture = m_colorTextures[0].id();
    if (texture != 0) {
        glGetTextureSubImage(texture, 0, x, y, 0, 1, 1, 1, ColorPixelFormat(m_options.colorFormat),
                             GL_UNSIGNED_BYTE, sizeof(pixel), pixel);
    } else {
        return false;  // MSAA-Ziel: vorher auflösen
    }
    std::memcpy(rgba, pixel, static_cast<std::size_t>(std::min(channels, 4)) * sizeof(u8));
    return true;
}

std::string Framebuffer::describe() const {
    std::string text = "Framebuffer " + std::to_string(m_width) + "x" + std::to_string(m_height) +
                       " " + ToString(m_options.samples);
    text += isMultisampled() ? " (MSAA)" : " (ein Abtastwert)";
    text += hasDepth() ? (hasStencil() ? ", Tiefe+Stencil" : ", Tiefe") : ", ohne Tiefe";
    text += ", " + std::to_string(m_options.colorAttachments) + " Farbanhang/Anhänge";
    text += isComplete() ? ", vollständig" : ", UNVOLLSTÄNDIG";
    return text;
}

// ─── Blit und Resolve ──────────────────────────────────────────────────────

bool CanBlitFramebuffer(const Framebuffer& src, const Framebuffer& dst) {
    if (!src.isValid() || !dst.isValid()) {
        return false;
    }
    if (src.options().colorFormat != dst.options().colorFormat) {
        return false;
    }
    if (src.colorAttachmentCount() > dst.colorAttachmentCount()) {
        return false;
    }
    // Tiefe/Stencil sind nur nötig, wenn sie auch kopiert werden – das prüft
    // BlitFramebuffer() beim Aufruf.
    return true;
}

namespace {

void DoBlit(const Framebuffer& src, const Rectangle& srcRect, Framebuffer& dst,
            const Rectangle& dstRect, const bool isColor, const bool isDepth, GLenum filter) {
    if (!CanBlitFramebuffer(src, dst)) {
        throw FramebufferError("BlitFramebuffer: Quelle und Ziel passen nicht zusammen (" +
                               src.describe() + " -> " + dst.describe() + ")");
    }
    GLbitfield mask = 0;
    if (isColor) {
        mask |= GL_COLOR_BUFFER_BIT;
    }
    if (isDepth) {
        if (!src.hasDepth() || !dst.hasDepth()) {
            throw FramebufferError("BlitFramebuffer: Tiefe verlangt, aber eine Seite hat keine");
        }
        mask |= GL_DEPTH_BUFFER_BIT;
        if (src.hasStencil() && dst.hasStencil()) {
            mask |= GL_STENCIL_BUFFER_BIT;
        }
        filter = GL_NEAREST;  // Tiefe/Stencil nur ohne Filterung
    }
    if (mask == 0) {
        return;
    }
    // Multisample-Resolve: Quelle mehrfach, Ziel einfach -> erlaubt; umgekehrt nicht.
    if (!src.isMultisampled() && dst.isMultisampled() && mask != GL_COLOR_BUFFER_BIT) {
        throw FramebufferError("BlitFramebuffer: Tiefe kann nicht in ein MSAA-Ziel kopiert werden");
    }

    // DSA: benannter Blit, ohne Bindung. Der Lese-Puffer bleibt auf
    // GL_COLOR_ATTACHMENT0 (so legt allocate() ihn an); beim Ziel wird für die
    // Dauer des Blits nur Anhang 0 als Zeichenziel eingetragen, damit ein
    // MRT-Ziel nicht in alle Anhänge geschrieben wird.
    const bool isColorBlit = (mask & GL_COLOR_BUFFER_BIT) != 0;
    if (isColorBlit) {
        const GLenum onlyFirst = GL_COLOR_ATTACHMENT0;
        glNamedFramebufferDrawBuffers(dst.id(), 1, &onlyFirst);
    }

    glBlitNamedFramebuffer(src.id(), dst.id(), static_cast<GLint>(srcRect.x),
                           static_cast<GLint>(srcRect.y),
                           static_cast<GLint>(srcRect.x + srcRect.width),
                           static_cast<GLint>(srcRect.y + srcRect.height),
                           static_cast<GLint>(dstRect.x), static_cast<GLint>(dstRect.y),
                           static_cast<GLint>(dstRect.x + dstRect.width),
                           static_cast<GLint>(dstRect.y + dstRect.height), mask, filter);

    if ((mask & GL_COLOR_BUFFER_BIT) != 0) {
        dst.setDrawBuffers();  // alle Anhänge wieder als Zeichenziele
    }
}

}  // namespace

void BlitFramebuffer(const Framebuffer& src, Framebuffer& dst, const bool isColor, const bool isDepth,
                     GLenum filter) {
    const Rectangle srcRect{0.0f, 0.0f, static_cast<f32>(src.width()), static_cast<f32>(src.height())};
    const Rectangle dstRect{0.0f, 0.0f, static_cast<f32>(dst.width()), static_cast<f32>(dst.height())};
    DoBlit(src, srcRect, dst, dstRect, isColor, isDepth, filter);
}

void BlitFramebuffer(const Framebuffer& src, const Rectangle& srcRect, Framebuffer& dst,
                     const Rectangle& dstRect, const bool isColor, const bool isDepth, GLenum filter) {
    DoBlit(src, srcRect, dst, dstRect, isColor, isDepth, filter);
}

void ResolveFramebuffer(const Framebuffer& src, Framebuffer& dst) {
    if (!src.isMultisampled()) {
        throw FramebufferError("ResolveFramebuffer: Quelle ist nicht mehrfach abgetastet (" +
                               src.describe() + ")");
    }
    if (dst.isMultisampled()) {
        throw FramebufferError("ResolveFramebuffer: Ziel darf nicht mehrfach abgetastet sein (" +
                               dst.describe() + ")");
    }
    if (src.width() != dst.width() || src.height() != dst.height()) {
        throw FramebufferError("ResolveFramebuffer: Größen unterscheiden sich (" +
                               std::to_string(src.width()) + "x" + std::to_string(src.height()) +
                               " -> " + std::to_string(dst.width()) + "x" +
                               std::to_string(dst.height()) + ")");
    }
    DoBlit(src,
           Rectangle{0.0f, 0.0f, static_cast<f32>(src.width()), static_cast<f32>(src.height())}, dst,
           Rectangle{0.0f, 0.0f, static_cast<f32>(dst.width()), static_cast<f32>(dst.height())},
           true, src.hasDepth() && dst.hasDepth(), GL_NEAREST);
    SLOGD << "MSAA aufgelöst: " << ToString(src.samples()) << " -> 1x (" << src.width() << "x"
         << src.height() << ")";
}

}  // namespace sgl
