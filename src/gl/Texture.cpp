#include <sgl/Texture.hpp>

#include <sgl/Context.hpp>
#include <sgl/Log.hpp>

#include <cstring>

#include <algorithm>
#include <cmath>

// Anisotrope Filterung ist erst ab GL 4.6 Kern; ältere Header kennen nur die
// EXT-Namen. Beide Konstanten sind identisch.
#ifndef GL_TEXTURE_MAX_ANISOTROPY
#define GL_TEXTURE_MAX_ANISOTROPY 0x84FE
#endif
#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY
#define GL_MAX_TEXTURE_MAX_ANISOTROPY 0x84FF
#endif
// Nahtlose Würfelkanten je Textur (ARB_seamless_cubemap_per_texture, Kern ab 4.5);
// derselbe Enum aktiviert sonst den globalen Zustand per glEnable.
#ifndef GL_TEXTURE_CUBE_MAP_SEAMLESS
#define GL_TEXTURE_CUBE_MAP_SEAMLESS 0x884F
#endif

// Alle Texturoperationen laufen über Direct State Access (GL 4.5): kein
// glBindTexture, kein glActiveTexture, kein glTexImage*. Das Texturobjekt wird
// direkt angesprochen (glTextureStorage*, glTextureSubImage*, glTextureParameter*,
// glGenerateTextureMipmap, glGetTextureSubImage). Gebunden wird nur noch zum
// Zeichnen über glBindTextureUnit – das ist Sampler-Zustand, nicht Texturzustand.

namespace sgl {

using namespace gl;
namespace {

GLint WrapToGl(const TextureWrap wrap) {
    switch (wrap) {
        case TextureWrap::Repeat:
            return GL_REPEAT;
        case TextureWrap::MirroredRepeat:
            return GL_MIRRORED_REPEAT;
        case TextureWrap::ClampToEdge:
            return GL_CLAMP_TO_EDGE;
        case TextureWrap::ClampToBorder:
            return GL_CLAMP_TO_BORDER;
    }
    return GL_REPEAT;
}

GLint FilterToGl(const TextureFilter filter) {
    return filter == TextureFilter::Nearest ? GL_NEAREST : GL_LINEAR;
}

// Min-Filter aus Filter und Mipmap-Modus zusammensetzen.
GLint MinFilterToGl(const TextureFilter filter, const MipmapMode mipmaps) {
    switch (mipmaps) {
        case MipmapMode::None:
            return FilterToGl(filter);
        case MipmapMode::Nearest:
            return filter == TextureFilter::Nearest ? GL_NEAREST_MIPMAP_NEAREST
                                                    : GL_LINEAR_MIPMAP_NEAREST;
        case MipmapMode::Linear:
            return filter == TextureFilter::Nearest ? GL_NEAREST_MIPMAP_LINEAR
                                                    : GL_LINEAR_MIPMAP_LINEAR;
    }
    return GL_LINEAR;
}

// Anzahl der Mipmap-Ebenen bis 1x1 (für die unveränderliche Speicherzuteilung).
i32 LevelCount(i32 width, i32 height) {
    i32 levels = 1;
    while (width > 1 || height > 1) {
        width = std::max(width / 2, 1);
        height = std::max(height / 2, 1);
        ++levels;
    }
    return levels;
}

// Zeilenausrichtung beim Hochladen. Dafür gibt es kein DSA-Gegenstück; das ist
// globaler Pixelstore-Zustand und wird deshalb gesichert und zurückgestellt.
struct UnpackGuard {
    UnpackGuard() {
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &m_alignment);
        glGetIntegerv(GL_UNPACK_ROW_LENGTH, &m_rowLength);
        glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &m_skipPixels);
        glGetIntegerv(GL_UNPACK_SKIP_ROWS, &m_skipRows);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    }
    ~UnpackGuard() {
        glPixelStorei(GL_UNPACK_ALIGNMENT, m_alignment);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, m_rowLength);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, m_skipPixels);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, m_skipRows);
    }
    UnpackGuard(const UnpackGuard&) = delete;
    UnpackGuard& operator=(const UnpackGuard&) = delete;

private:
    GLint m_alignment{4};
    GLint m_rowLength{0};
    GLint m_skipPixels{0};
    GLint m_skipRows{0};
};

struct PackGuard {
    explicit PackGuard(GLint alignment) {
        glGetIntegerv(GL_PACK_ALIGNMENT, &m_alignment);
        glPixelStorei(GL_PACK_ALIGNMENT, alignment);
    }
    ~PackGuard() { glPixelStorei(GL_PACK_ALIGNMENT, m_alignment); }
    PackGuard(const PackGuard&) = delete;
    PackGuard& operator=(const PackGuard&) = delete;

private:
    GLint m_alignment{4};
};

// Randfarbe setzen (getrennt, weil sie als float-Vektor übergeben wird).
void ApplyBorderColor(GLuint id, const std::array<u8, 4>& color) {
    const GLfloat border[4] = {color[0] / 255.0f, color[1] / 255.0f, color[2] / 255.0f,
                               color[3] / 255.0f};
    glTextureParameterfv(id, GL_TEXTURE_BORDER_COLOR, border);
}

void ApplyAnisotropy(GLuint id, const f32 requested) {
    if (requested <= 0.0f) {
        return;
    }
    GLfloat maximum = 1.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maximum);
    glTextureParameterf(id, GL_TEXTURE_MAX_ANISOTROPY, std::clamp(requested, 1.0f, maximum));
}

// Ebene 0 (face < 0) bzw. ein Cube-Gesicht zurücklesen – ohne Binden, über
// glGetTextureSubImage (z-Offset adressiert bei Cube-Maps das Gesicht).
Image ReadTexture(const GLuint id, const i32 face, const i32 width, const i32 height,
                  const ImageFormat format) {
    if (id == 0 || width <= 0 || height <= 0) {
        throw TextureError("readBack: Textur ist leer");
    }
    Image image(width, height, format);
    const PackGuard guard(1);
    glGetTextureSubImage(id, 0, 0, 0, std::max(face, 0), width, height, 1,
                         PixelFormatFor(format), GL_UNSIGNED_BYTE,
                         static_cast<GLsizei>(image.byteSize()), image.data());
    return image;
}

// Parameter einer 2D-Textur setzen – gebraucht beim Erzeugen und beim
// Neuanlegen in upload().
void ApplyParameters2d(GLuint id, const TextureOptions& options) {
    glTextureParameteri(id, GL_TEXTURE_WRAP_S, WrapToGl(options.wrapS));
    glTextureParameteri(id, GL_TEXTURE_WRAP_T, WrapToGl(options.wrapT));
    glTextureParameteri(id, GL_TEXTURE_MIN_FILTER,
                        MinFilterToGl(options.minFilter, options.mipmaps));
    glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, FilterToGl(options.magFilter));
    ApplyBorderColor(id, options.borderColor);
    ApplyAnisotropy(id, options.anisotropy);
}

std::size_t ExpectedBytes(const i32 width, const i32 height, const ImageFormat format) {
    return static_cast<std::size_t>(width) * static_cast<std::size_t>(height) *
           static_cast<std::size_t>(ChannelCount(format));
}

}  // namespace

// ─── Formate ────────────────────────────────────────────────────────────────

GLenum PixelFormatFor(const ImageFormat format) {
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

GLenum InternalFormatFor(const ImageFormat format, const bool isSrgb) {
    // Graustufen haben in GL kein sRGB-Format; sie bleiben linear.
    if (isSrgb) {
        switch (format) {
            case ImageFormat::Rgb:
                return GL_SRGB8;
            case ImageFormat::Rgba:
                return GL_SRGB8_ALPHA8;
            default:
                break;
        }
    }
    switch (format) {
        case ImageFormat::Gray:
            return GL_R8;
        case ImageFormat::GrayAlpha:
            return GL_RG8;
        case ImageFormat::Rgb:
            return GL_RGB8;
        case ImageFormat::Rgba:
            return GL_RGBA8;
    }
    return GL_RGBA8;
}

// ─── Texture2d ──────────────────────────────────────────────────────────────

Texture2d::~Texture2d() {
    release();
}

Texture2d::Texture2d(Texture2d&& other) noexcept
    : m_id(other.m_id),
      m_width(other.m_width),
      m_height(other.m_height),
      m_format(other.m_format),
      m_options(other.m_options) {
    other.m_id = 0;
    other.m_width = 0;
    other.m_height = 0;
}

Texture2d& Texture2d::operator=(Texture2d&& other) noexcept {
    if (this != &other) {
        release();
        m_id = other.m_id;
        m_width = other.m_width;
        m_height = other.m_height;
        m_format = other.m_format;
        m_options = other.m_options;
        other.m_id = 0;
        other.m_width = 0;
        other.m_height = 0;
    }
    return *this;
}

void Texture2d::release() noexcept {
    if (m_isResident) {
        (void)makeNonResident();  // residenter Handle muss vor dem Löschen weg
    }
    m_handle = 0;
    if (m_id != 0) {
        glDeleteTextures(1, &m_id);
        m_id = 0;
    }
    m_width = 0;
    m_height = 0;
}

Texture2d Texture2d::CreateStorage(const i32 width, const i32 height, ImageFormat format,
                                  const TextureOptions& options) {
    if (width <= 0 || height <= 0) {
        throw TextureError("Texture2d: Maße müssen positiv sein (" + std::to_string(width) + "x" +
                           std::to_string(height) + ")");
    }
    Texture2d texture;
    texture.m_width = width;
    texture.m_height = height;
    texture.m_format = format;
    texture.m_options = options;
    texture.m_options.mipmaps = MipmapMode::None;  // Renderziele brauchen keine Mipmaps

    glCreateTextures(GL_TEXTURE_2D, 1, &texture.m_id);
    if (texture.m_id == 0) {
        throw TextureError("Texture2d: glCreateTextures ist fehlgeschlagen");
    }

    const GLenum internalFormat =
        (options.isHdr && (format == ImageFormat::Rgba))
            ? GL_RGBA16F
            : static_cast<GLenum>(InternalFormatFor(format, options.isSrgb));

    glTextureStorage2D(texture.m_id, 1, internalFormat, width, height);
    texture.applyParameters();
    SLOGD << "Renderziel-Textur erzeugt: " << width << "x" << height << " " << ToString(format);
    return texture;
}

Texture2d Texture2d::FromMemory(const i32 width, const i32 height, const ImageFormat format,
                                std::span<const u8> pixels, const TextureOptions& options) {
    if (width <= 0 || height <= 0) {
        throw TextureError("Texture2d: Maße müssen positiv sein (" + std::to_string(width) + "x" +
                           std::to_string(height) + ")");
    }
    const std::size_t expected = ExpectedBytes(width, height, format);
    if (pixels.size() != expected) {
        throw TextureError("Texture2d: Puffer hat " + std::to_string(pixels.size()) +
                           " Bytes, erwartet " + std::to_string(expected));
    }

    Texture2d texture;
    texture.m_width = width;
    texture.m_height = height;
    texture.m_format = format;
    texture.m_options = options;

    // DSA: Objekt erzeugen und Speicher zuteilen, ohne es irgendwo zu binden.
    glCreateTextures(GL_TEXTURE_2D, 1, &texture.m_id);
    if (texture.m_id == 0) {
        throw TextureError("Texture2d: glCreateTextures ist fehlgeschlagen");
    }
    const i32 levels = texture.hasMipmaps() ? LevelCount(width, height) : 1;
    glTextureStorage2D(texture.m_id, levels,
                       static_cast<GLenum>(InternalFormatFor(format, options.isSrgb)), width,
                       height);
    {
        const UnpackGuard guard;
        glTextureSubImage2D(texture.m_id, 0, 0, 0, width, height, PixelFormatFor(format),
                            GL_UNSIGNED_BYTE, pixels.data());
    }
    texture.applyParameters();
    if (texture.hasMipmaps()) {
        glGenerateTextureMipmap(texture.m_id);
    }

    SLOGD << "Textur erzeugt: " << width << "x" << height << " " << ToString(format)
         << (options.isSrgb ? " sRGB" : "") << " (" << levels << " Ebenen)";
    return texture;
}

Texture2d Texture2d::FromImage(const Image& image, const TextureOptions& options) {
    if (image.isEmpty()) {
        throw TextureError("Texture2d: Bild ist leer");
    }
    return FromMemory(image.width(), image.height(), image.format(),
                      std::span<const u8>(image.data(), image.byteSize()), options);
}

void Texture2d::applyParameters() {
    ApplyParameters2d(m_id, m_options);
}

void Texture2d::upload(const Image& image) {
    if (m_id == 0) {
        throw TextureError("upload: Textur ist ungültig");
    }
    if (image.isEmpty()) {
        throw TextureError("upload: Bild ist leer");
    }
    // Maße oder Format geändert: glTextureStorage* legt den Speicher
    // unveränderlich fest, ein zweites Storage auf demselben Objekt wäre
    // GL_INVALID_OPERATION. Also ein frisches Texturobjekt anlegen und tauschen.
    if (image.width() != m_width || image.height() != m_height || image.format() != m_format) {
        GLuint fresh = 0;
        glCreateTextures(GL_TEXTURE_2D, 1, &fresh);
        if (fresh == 0) {
            throw TextureError("upload: glCreateTextures ist fehlgeschlagen");
        }
        const i32 levels = hasMipmaps() ? LevelCount(image.width(), image.height()) : 1;
        const GLenum internalFormat =
            (m_options.isHdr && (image.format() == ImageFormat::Rgba))
                ? GL_RGBA16F
                : static_cast<GLenum>(InternalFormatFor(image.format(), m_options.isSrgb));
        glTextureStorage2D(fresh, levels, internalFormat, image.width(), image.height());
        {
            const UnpackGuard guard;
            glTextureSubImage2D(fresh, 0, 0, 0, image.width(), image.height(),
                                PixelFormatFor(image.format()), GL_UNSIGNED_BYTE, image.data());
        }
        ApplyParameters2d(fresh, m_options);
        if (hasMipmaps()) {
            glGenerateTextureMipmap(fresh);
        }
        glDeleteTextures(1, &m_id);
        m_id = fresh;
        m_width = image.width();
        m_height = image.height();
        m_format = image.format();
        SLOGD << "Textur neu angelegt: " << m_width << "x" << m_height << " " << ToString(m_format);
        return;
    }
    {
        const UnpackGuard guard;
        glTextureSubImage2D(m_id, 0, 0, 0, m_width, m_height, PixelFormatFor(m_format),
                            GL_UNSIGNED_BYTE, image.data());
    }
    applyParameters();
    if (hasMipmaps()) {
        glGenerateTextureMipmap(m_id);
    }
}

void Texture2d::updateRegion(const Rectangle& region, const Image& image) {
    if (m_id == 0) {
        throw TextureError("updateRegion: Textur ist ungültig");
    }
    if (image.format() != m_format) {
        throw TextureError("updateRegion: Bildformat passt nicht zur Textur");
    }
    const i32 x = static_cast<i32>(std::lround(region.x));
    const i32 y = static_cast<i32>(std::lround(region.y));
    const i32 width = static_cast<i32>(std::lround(region.width));
    const i32 height = static_cast<i32>(std::lround(region.height));
    if (width <= 0 || height <= 0 || x < 0 || y < 0 || x + width > m_width ||
        y + height > m_height) {
        throw TextureError("updateRegion: Bereich liegt außerhalb der Textur");
    }
    if (image.width() != width || image.height() != height) {
        throw TextureError("updateRegion: Bildmaße (" + std::to_string(image.width()) + "x" +
                           std::to_string(image.height()) + ") passen nicht zum Bereich (" +
                           std::to_string(width) + "x" + std::to_string(height) + ")");
    }

    const UnpackGuard guard;
    glPixelStorei(GL_UNPACK_ROW_LENGTH, static_cast<GLint>(image.rowStride() /
                                                           static_cast<std::size_t>(
                                                               image.channelCount())));
    glTextureSubImage2D(m_id, 0, x, y, width, height, PixelFormatFor(m_format), GL_UNSIGNED_BYTE,
                        image.data());
    if (hasMipmaps()) {
        glGenerateTextureMipmap(m_id);
    }
}

void Texture2d::generateMipmaps() {
    if (m_id == 0) {
        throw TextureError("generateMipmaps: Textur ist ungültig");
    }
    glGenerateTextureMipmap(m_id);
}

void Texture2d::setWrap(const TextureWrap s, const TextureWrap t) {
    m_options.wrapS = s;
    m_options.wrapT = t;
    if (m_id != 0) {
        applyParameters();
    }
}

void Texture2d::setFilter(const TextureFilter minFilter, const TextureFilter magFilter,
                          const MipmapMode mipmaps) {
    m_options.minFilter = minFilter;
    m_options.magFilter = magFilter;
    m_options.mipmaps = mipmaps;
    if (m_id != 0) {
        applyParameters();
        if (hasMipmaps()) {
            generateMipmaps();
        }
    }
}

void Texture2d::setAnisotropy(const f32 maxAnisotropy) {
    m_options.anisotropy = maxAnisotropy;
    if (m_id != 0) {
        ApplyAnisotropy(m_id, m_options.anisotropy);
    }
}

void Texture2d::setBorderColor(std::array<u8, 4> color) {
    m_options.borderColor = color;
    if (m_id != 0) {
        ApplyBorderColor(m_id, m_options.borderColor);
    }
}

Image Texture2d::readBack() const {
    return ReadTexture(m_id, -1, m_width, m_height, m_format);
}

void Texture2d::use(const i32 unit) const {
    if (m_id == 0) {
        return;
    }
    glBindTextureUnit(static_cast<GLuint>(std::max(unit, 0)), m_id);
}

void Texture2d::Unbind(const i32 unit) {
    glBindTextureUnit(static_cast<GLuint>(std::max(unit, 0)), 0);
}

Texture2d::ScopedBind::ScopedBind(const Texture2d& texture, i32 unit) : m_unit(unit) {
    // Bindung der Einheit indexiert abfragen – ohne die aktive Einheit zu ändern.
    glGetIntegeri_v(GL_TEXTURE_BINDING_2D, static_cast<GLuint>(std::max(unit, 0)),
                    &m_previousTexture);
    glBindTextureUnit(static_cast<GLuint>(std::max(unit, 0)), texture.id());
}

Texture2d::ScopedBind::~ScopedBind() {
    glBindTextureUnit(static_cast<GLuint>(std::max(m_unit, 0)),
                      static_cast<GLuint>(m_previousTexture));
}


// ─── Residency (bindless) ───────────────────────────────────────────────────
// GL_ARB_bindless_texture ist nicht Kern von OpenGL 4.6. Die Funktionen werden
// deshalb zur Laufzeit gesucht; fehlt die Erweiterung, melden die Methoden
// false und die Klassen verhalten sich wie zuvor.

namespace {

using PFN_GET_TEXTURE_HANDLE = u64 (*)(GLuint);
using PFN_MAKE_RESIDENT = void (*)(u64);
using PFN_MAKE_NON_RESIDENT = void (*)(u64);
using PFN_IS_HANDLE_RESIDENT = GLboolean (*)(u64);

struct BindlessApi {
    bool isChecked{false};
    bool isSupported{false};
    PFN_GET_TEXTURE_HANDLE getTextureHandle{nullptr};
    PFN_MAKE_RESIDENT makeResident{nullptr};
    PFN_MAKE_NON_RESIDENT makeNonResident{nullptr};
    PFN_IS_HANDLE_RESIDENT isResident{nullptr};
};

template <class Function>
Function LoadProcedure(const char* name) {
    return reinterpret_cast<Function>(sgl::GetProcAddress(name));
}

bool HasExtension(const char* wanted) {
    GLint count = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &count);
    for (GLint index = 0; index < count; ++index) {
        const char* name = reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, index));
        if (name != nullptr && std::strcmp(name, wanted) == 0) {
            return true;
        }
    }
    return false;
}

BindlessApi& Bindless() {
    static BindlessApi api;
    if (api.isChecked) {
        return api;
    }
    api.isChecked = true;
    if (!HasExtension("GL_ARB_bindless_texture")) {
        return api;
    }
    api.getTextureHandle = LoadProcedure<PFN_GET_TEXTURE_HANDLE>("glGetTextureHandleARB");
    api.makeResident = LoadProcedure<PFN_MAKE_RESIDENT>("glMakeTextureHandleResidentARB");
    api.makeNonResident = LoadProcedure<PFN_MAKE_NON_RESIDENT>("glMakeTextureHandleNonResidentARB");
    api.isResident = LoadProcedure<PFN_IS_HANDLE_RESIDENT>("glIsTextureHandleResidentARB");
    api.isSupported = api.getTextureHandle != nullptr && api.makeResident != nullptr &&
                      api.makeNonResident != nullptr && api.isResident != nullptr;
    return api;
}

}  // namespace

bool Texture2d::IsResidencySupported() {
    return Bindless().isSupported;
}

u64 Texture2d::textureHandle() const {
    if (m_id == 0) {
        throw TextureError("textureHandle: Textur ist ungültig");
    }
    BindlessApi& api = Bindless();
    if (!api.isSupported) {
        throw TextureError(
            "textureHandle: GL_ARB_bindless_texture wird von diesem Treiber nicht unterstützt");
    }
    if (m_handle == 0) {
        m_handle = api.getTextureHandle(m_id);
    }
    return m_handle;
}

bool Texture2d::makeResident() {
    if (m_id == 0) {
        return false;
    }
    BindlessApi& api = Bindless();
    if (!api.isSupported) {
        return false;
    }
    if (m_handle == 0) {
        m_handle = api.getTextureHandle(m_id);
    }
    if (m_isResident) {
        return true;
    }
    api.makeResident(m_handle);
    m_isResident = api.isResident(m_handle) == GL_TRUE;
    return m_isResident;
}

bool Texture2d::makeNonResident() {
    BindlessApi& api = Bindless();
    if (!api.isSupported || m_handle == 0 || !m_isResident) {
        return !m_isResident;
    }
    api.makeNonResident(m_handle);
    m_isResident = api.isResident(m_handle) == GL_TRUE;
    return !m_isResident;
}

bool Texture2d::isResident() const {
    return m_isResident;
}

bool CubeMap::IsResidencySupported() {
    return Bindless().isSupported;
}

u64 CubeMap::textureHandle() const {
    if (m_id == 0) {
        throw TextureError("textureHandle: CubeMap ist ungültig");
    }
    BindlessApi& api = Bindless();
    if (!api.isSupported) {
        throw TextureError(
            "textureHandle: GL_ARB_bindless_texture wird von diesem Treiber nicht unterstützt");
    }
    if (m_handle == 0) {
        m_handle = api.getTextureHandle(m_id);
    }
    return m_handle;
}

bool CubeMap::makeResident() {
    if (m_id == 0) {
        return false;
    }
    BindlessApi& api = Bindless();
    if (!api.isSupported) {
        return false;
    }
    if (m_handle == 0) {
        m_handle = api.getTextureHandle(m_id);
    }
    if (m_isResident) {
        return true;
    }
    api.makeResident(m_handle);
    m_isResident = api.isResident(m_handle) == GL_TRUE;
    return m_isResident;
}

bool CubeMap::makeNonResident() {
    BindlessApi& api = Bindless();
    if (!api.isSupported || m_handle == 0 || !m_isResident) {
        return !m_isResident;
    }
    api.makeNonResident(m_handle);
    m_isResident = api.isResident(m_handle) == GL_TRUE;
    return !m_isResident;
}

bool CubeMap::isResident() const {
    return m_isResident;
}

// ─── CubeMap ────────────────────────────────────────────────────────────────

CubeMap::~CubeMap() {
    release();
}

CubeMap::CubeMap(CubeMap&& other) noexcept
    : m_id(other.m_id),
      m_size(other.m_size),
      m_format(other.m_format),
      m_options(other.m_options) {
    other.m_id = 0;
    other.m_size = 0;
}

CubeMap& CubeMap::operator=(CubeMap&& other) noexcept {
    if (this != &other) {
        release();
        m_id = other.m_id;
        m_size = other.m_size;
        m_format = other.m_format;
        m_options = other.m_options;
        other.m_id = 0;
        other.m_size = 0;
    }
    return *this;
}

void CubeMap::release() noexcept {
    if (m_isResident) {
        (void)makeNonResident();
    }
    m_handle = 0;
    if (m_id != 0) {
        glDeleteTextures(1, &m_id);
        m_id = 0;
    }
    m_size = 0;
}

void CubeMap::applyParameters() {
    glTextureParameteri(m_id, GL_TEXTURE_WRAP_S, WrapToGl(m_options.wrapS));
    glTextureParameteri(m_id, GL_TEXTURE_WRAP_T, WrapToGl(m_options.wrapT));
    glTextureParameteri(m_id, GL_TEXTURE_WRAP_R, WrapToGl(m_options.wrapR));
    glTextureParameteri(m_id, GL_TEXTURE_MIN_FILTER,
                        MinFilterToGl(m_options.minFilter, m_options.mipmaps));
    glTextureParameteri(m_id, GL_TEXTURE_MAG_FILTER, FilterToGl(m_options.magFilter));
    // Kanten zwischen den Gesichtern interpolieren (kein Naht-Artefakt bei
    // gefilterten CubeMaps), als Eigenschaft genau dieser Textur.
    glTextureParameteri(m_id, GL_TEXTURE_CUBE_MAP_SEAMLESS,
                        m_options.isSeamless ? GL_TRUE : GL_FALSE);
    ApplyAnisotropy(m_id, m_options.anisotropy);
}

CubeMap CubeMap::FromImages(const std::array<Image, 6>& faces, const TextureOptions& options) {
    for (std::size_t index = 0; index < faces.size(); ++index) {
        if (faces[index].isEmpty()) {
            throw TextureError("CubeMap: Gesicht " + std::to_string(index) + " ist leer");
        }
        if (faces[index].width() != faces[0].width() ||
            faces[index].height() != faces[0].height()) {
            throw TextureError("CubeMap: alle Gesichter müssen gleich groß sein");
        }
        if (faces[index].format() != faces[0].format()) {
            throw TextureError("CubeMap: alle Gesichter müssen dasselbe Format haben");
        }
    }

    CubeMap cubeMap;
    cubeMap.m_size = static_cast<u32>(faces[0].width());
    cubeMap.m_format = faces[0].format();
    cubeMap.m_options = options;

    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &cubeMap.m_id);
    if (cubeMap.m_id == 0) {
        throw TextureError("CubeMap: glCreateTextures ist fehlgeschlagen");
    }
    const i32 size = static_cast<i32>(cubeMap.m_size);
    const i32 levels = cubeMap.hasMipmaps() ? LevelCount(size, size) : 1;
    glTextureStorage2D(cubeMap.m_id, levels,
                       static_cast<GLenum>(InternalFormatFor(cubeMap.m_format, options.isSrgb)),
                       size, size);
    {
        const UnpackGuard guard;
        for (std::size_t index = 0; index < faces.size(); ++index) {
            // Bei Cube-Maps adressiert der z-Offset das Gesicht.
            glTextureSubImage3D(cubeMap.m_id, 0, 0, 0, static_cast<GLint>(index), size, size, 1,
                                PixelFormatFor(cubeMap.m_format), GL_UNSIGNED_BYTE,
                                faces[index].data());
        }
    }
    cubeMap.applyParameters();
    if (cubeMap.hasMipmaps()) {
        glGenerateTextureMipmap(cubeMap.m_id);
    }

    SLOGD << "CubeMap erzeugt: " << cubeMap.m_size << " px je Gesicht (" << levels << " Ebenen)";
    return cubeMap;
}

CubeMap CubeMap::FromCrossLayout(const Image& cross, const TextureOptions& options) {
    if (cross.isEmpty()) {
        throw TextureError("CubeMap: Kreuz-Layout ist leer");
    }
    const i32 width = cross.width();
    const i32 height = cross.height();

    // 4x3 (waagerecht) oder 3x4 (senkrecht)
    i32 cell = 0;
    bool horizontal = false;
    if (width % 4 == 0 && height % 3 == 0 && width / 4 == height / 3) {
        cell = width / 4;
        horizontal = true;
    } else if (width % 3 == 0 && height % 4 == 0 && width / 3 == height / 4) {
        cell = width / 3;
        horizontal = false;
    } else {
        throw TextureError("CubeMap: Kreuz-Layout muss 4x3 oder 3x4 Quadrate enthalten (" +
                           std::to_string(width) + "x" + std::to_string(height) + ")");
    }

    // Zellenpositionen je Gesicht (Spalte, Zeile), wie in den üblichen Tools
    const auto cellAt = [cell](i32 column, i32 row) {
        return Rectangle{static_cast<f32>(column * cell), static_cast<f32>(row * cell),
                         static_cast<f32>(cell), static_cast<f32>(cell)};
    };
    std::array<Rectangle, 6> cells{};
    if (horizontal) {
        cells[0] = cellAt(0, 1);  // +X
        cells[1] = cellAt(1, 1);  // -X
        cells[2] = cellAt(2, 1);  // +Y
        cells[3] = cellAt(3, 1);  // -Y
        cells[4] = cellAt(2, 0);  // +Z
        cells[5] = cellAt(2, 2);  // -Z
    } else {
        cells[0] = cellAt(2, 1);  // +X
        cells[1] = cellAt(0, 1);  // -X
        cells[2] = cellAt(1, 0);  // +Y
        cells[3] = cellAt(1, 3);  // -Y
        cells[4] = cellAt(1, 1);  // +Z
        cells[5] = cellAt(1, 2);  // -Z
    }

    std::array<Image, 6> faces{};
    for (std::size_t index = 0; index < faces.size(); ++index) {
        const Rectangle& area = cells[index];
        faces[index] = cross.cropped(static_cast<i32>(area.x), static_cast<i32>(area.y), cell,
                                     cell);
    }
    return FromImages(faces, options);
}

void CubeMap::use(const i32 unit) const {
    if (m_id == 0) {
        return;
    }
    glBindTextureUnit(static_cast<GLuint>(std::max(unit, 0)), m_id);
}

void CubeMap::Unbind(const i32 unit) {
    glBindTextureUnit(static_cast<GLuint>(std::max(unit, 0)), 0);
}

CubeMap::ScopedBind::ScopedBind(const CubeMap& cubeMap, i32 unit) : m_unit(unit) {
    glGetIntegeri_v(GL_TEXTURE_BINDING_CUBE_MAP, static_cast<GLuint>(std::max(unit, 0)),
                    &m_previousTexture);
    glBindTextureUnit(static_cast<GLuint>(std::max(unit, 0)), cubeMap.id());
}

CubeMap::ScopedBind::~ScopedBind() {
    glBindTextureUnit(static_cast<GLuint>(std::max(m_unit, 0)),
                      static_cast<GLuint>(m_previousTexture));
}

void CubeMap::uploadFace(CubeFace face, const Image& image) {
    if (m_id == 0) {
        throw TextureError("uploadFace: CubeMap ist ungültig");
    }
    if (image.isEmpty()) {
        throw TextureError("uploadFace: Bild ist leer");
    }
    if (image.format() != m_format) {
        throw TextureError("uploadFace: Bildformat passt nicht zur CubeMap");
    }
    if (static_cast<u32>(image.width()) != m_size ||
        static_cast<u32>(image.height()) != m_size) {
        throw TextureError("uploadFace: Bildmaße passen nicht zur CubeMap (" +
                           std::to_string(m_size) + " px erwartet)");
    }
    {
        const UnpackGuard guard;
        glTextureSubImage3D(m_id, 0, 0, 0, static_cast<GLint>(face), image.width(), image.height(),
                            1, PixelFormatFor(m_format), GL_UNSIGNED_BYTE, image.data());
    }
    if (hasMipmaps()) {
        glGenerateTextureMipmap(m_id);
    }
}

void CubeMap::generateMipmaps() {
    if (m_id == 0) {
        throw TextureError("generateMipmaps: CubeMap ist ungültig");
    }
    glGenerateTextureMipmap(m_id);
}

void CubeMap::setWrap(const TextureWrap s, const TextureWrap t, const TextureWrap r) {
    m_options.wrapS = s;
    m_options.wrapT = t;
    m_options.wrapR = r;
    if (m_id != 0) {
        applyParameters();
    }
}

void CubeMap::setFilter(const TextureFilter minFilter, const TextureFilter magFilter,
                        const MipmapMode mipmaps) {
    m_options.minFilter = minFilter;
    m_options.magFilter = magFilter;
    m_options.mipmaps = mipmaps;
    if (m_id != 0) {
        applyParameters();
        if (hasMipmaps()) {
            generateMipmaps();
        }
    }
}

void CubeMap::setAnisotropy(const f32 maxAnisotropy) {
    m_options.anisotropy = maxAnisotropy;
    if (m_id != 0) {
        ApplyAnisotropy(m_id, m_options.anisotropy);
    }
}

void CubeMap::setSeamless(const bool seamless) {
    m_options.isSeamless = seamless;
    if (m_id != 0) {
        glTextureParameteri(m_id, GL_TEXTURE_CUBE_MAP_SEAMLESS, seamless ? GL_TRUE : GL_FALSE);
    }
}

Image CubeMap::faceImage(CubeFace face) const {
    if (m_id == 0) {
        throw TextureError("faceImage: CubeMap ist ungültig");
    }
    return ReadTexture(m_id, static_cast<i32>(face), static_cast<i32>(m_size),
                       static_cast<i32>(m_size), m_format);
}

}  // namespace sgl
