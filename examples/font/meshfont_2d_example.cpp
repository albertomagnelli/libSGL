// ─────────────────────────────────────────────────────────────────────────────
// meshfont_2d_example – advanced Beispiel: 2D-Schrift als Dreiecksnetz auf der GPU
//
// Zeigt, wie Font2d-Glyphen (Dreiecksnetze, keine Texturen) zu *einem* Puffer
// zusammengefasst und mit Shadern gezeichnet werden. Drei Durchgänge auf
// derselben Geometrie ergeben Schatten, Kontur und Füllung; die Füllung wird im
// Fragment-Shader verlaufsgefärbt, die Buchstaben schwingen im Vertex-Shader.
//
//   Steuerung (Fenster):  Esc schließt
//   Headless:             ./meshfont_2d_example --frame bild.png [breite höhe]
//                         (verstecktes sgl-Fenster, kein sichtbares Fenster nötig)
//
// Aufbau:
//   1. Font2d laden, Satz in Glyphennetze zerlegen (inkl. Kerning)
//   2. Netze in Pixelkoordinaten bringen und in einen interleaved Puffer legen
//   3. Zeichnen: Schatten (versetzt, skaliert) -> Kontur (skaliert) -> Füllung
// ─────────────────────────────────────────────────────────────────────────────

#include "FontExampleCommon.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace sgl;
using namespace gl;

namespace {

const char* const kText = "Alle Taxifahrer rasen wie wild im Kreis und geben vollgaß ÜÄÖüäö – 2D-Meshschrift";
const char* const kSubText = "Font2d + Shader: Schatten, Kontur und Verlaufsfüllung in drei Durchgängen";

// Ein Eckpunkt: Position in Bildpixeln, normierte Lage im Glyphen, Farbe und
// Mittelpunkt des Glyphen (für das Skalieren in den Kontur-/Schattendurchgängen).
struct Vertex2d {
    vec2 position;
    vec2 local;    // 0..1 innerhalb des Glyphen
    vec4 color;
    vec2 center;   // Mittelpunkt des Glyphen in Bildpixeln
};

struct TextGeometry {
    std::vector<Vertex2d> vertices;
    std::vector<u32> indices;
    f32 width{0.0f};
    f32 height{0.0f};
};

// Bricht einen Text an Leerzeichen so um, dass eine Zeile in maxWidth passt.
std::vector<std::string> WrapText(const Font2d& font, const std::string& text, f32 pixelSize,
                                  f32 maxWidth) {
    std::vector<std::string> lines;
    std::string line;
    std::string word;
    for (std::size_t i = 0; i <= text.size(); ++i) {
        const char character = i < text.size() ? text[i] : ' ';
        if (character != ' ') {
            word.push_back(character);
            continue;
        }
        if (word.empty()) {
            continue;
        }
        const std::string candidate = line.empty() ? word : line + " " + word;
        if (!line.empty() && font.measure(candidate, pixelSize) > maxWidth) {
            lines.push_back(line);
            line = word;
        } else {
            line = candidate;
        }
        word.clear();
    }
    if (!line.empty()) {
        lines.push_back(line);
    }
    return lines;
}

// ─── Geometrie bauen ────────────────────────────────────────────────────────

TextGeometry BuildText(const Font2d& font, const std::vector<std::string>& lines, f32 pixelSize,
                       f32 originX, f32 originY) {
    TextGeometry geometry;
    // Farbe je Glyphe: Farbton wandert über den Text
    u32 glyphCounter = 0;
    f32 baseline = originY + font.ascent() * pixelSize;

    for (const std::string& line : lines) {
        f32 penX = originX;
        bool kerningPending = false;
        u32 previous = 0;

        for (const u32 codepoint : DecodeUtf8(line)) {
            if (codepoint == ' ') {
                penX += font.advance(codepoint) * pixelSize;
                kerningPending = false;
                continue;
            }
            if (kerningPending) {
                penX += font.kerning(previous, codepoint) * pixelSize;
            }

            const GlyphMesh mesh = font.glyphMesh(codepoint, MeshQuality::High);
            if (!mesh.isEmpty()) {
                // Grenzen des Glyphen in Bildpixeln
                f32 minX = mesh.positions[0].x;
                f32 maxX = minX;
                f32 minY = mesh.positions[0].y;
                f32 maxY = minY;
                for (const vec2& point : mesh.positions) {
                    minX = std::min(minX, point.x);
                    maxX = std::max(maxX, point.x);
                    minY = std::min(minY, point.y);
                    maxY = std::max(maxY, point.y);
                }

                const f32 baseX = penX + minX * pixelSize;
                const f32 baseY = baseline - maxY * pixelSize;
                const f32 glyphWidth = std::max((maxX - minX) * pixelSize, 1.0f);
                const f32 glyphHeight = std::max((maxY - minY) * pixelSize, 1.0f);
                const vec2 center(baseX + glyphWidth * 0.5f, baseY + glyphHeight * 0.5f);

                const f32 hue = std::fmod(static_cast<f32>(glyphCounter) * 11.0f, 360.0f);
                const vec4 tint = fontdemo::HsvToRgb(hue, 0.55f, 1.0f);

                const u32 base = static_cast<u32>(geometry.vertices.size());
                for (const vec2& point : mesh.positions) {
                    Vertex2d vertex;
                    vertex.position = vec2(penX + point.x * pixelSize, baseline - point.y * pixelSize);
                    vertex.local = vec2((vertex.position.x - baseX) / glyphWidth,
                                        (vertex.position.y - baseY) / glyphHeight);
                    vertex.color = tint;
                    vertex.center = center;
                    geometry.vertices.push_back(vertex);
                }
                for (const u32 index : mesh.indices) {
                    geometry.indices.push_back(base + index);
                }
                ++glyphCounter;
            }

            penX += font.advance(codepoint) * pixelSize;
            previous = codepoint;
            kerningPending = true;
        }
        baseline += font.lineHeight() * pixelSize * 1.15f;
    }

    geometry.width = 0.0f;
    geometry.height = baseline;
    for (const Vertex2d& vertex : geometry.vertices) {
        geometry.width = std::max(geometry.width, vertex.position.x);
    }
    return geometry;
}

// ─── Shader ─────────────────────────────────────────────────────────────────

const char* const kTextShader = R"(
#pragma stage vertex
#version 460 core
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aLocal;
layout(location = 2) in vec4 aColor;
layout(location = 3) in vec2 aCenter;

uniform mat4  uProjection;
uniform float uTime;
uniform float uWave;         // Amplitude der Schwingung in Pixeln
uniform float uScale;        // 1.0 = Füllung, >1 = Kontur/Schatten
uniform vec2  uOffset;       // Versatz für den Schatten

out vec2 vLocal;
out vec4 vColor;

void main() {
    // um den Glyphenmittelpunkt skalieren (Kontur und Schatten entstehen so
    // aus derselben Geometrie) und wellenförmig verschieben
    vec2 position = aCenter + (aPosition - aCenter) * uScale + uOffset;
    position.y += sin(uTime * 2.0 + aPosition.x * 0.012) * uWave;

    vLocal = aLocal;
    vColor = aColor;
    gl_Position = uProjection * vec4(position, 0.0, 1.0);
}
#pragma stage fragment
#version 460 core
in vec2 vLocal;
in vec4 vColor;
out vec4 fragColor;

uniform int   uMode;      // 0 = Füllung, 1 = Kontur, 2 = Schatten
uniform vec3  uOutlineColor;
uniform float uAlpha;

void main() {
    if (uMode == 1) {
        // Kontur: kräftige, helle Kante
        fragColor = vec4(uOutlineColor, uAlpha);
        return;
    }
    if (uMode == 2) {
        // Schatten: dunkel und weich nach unten
        fragColor = vec4(0.02, 0.03, 0.06, uAlpha * (0.55 - vLocal.y * 0.25));
        return;
    }
    // Füllung: senkrechter Verlauf plus leichte Aufhellung zur Mitte
    vec3 top = mix(vColor.rgb, vec3(1.0), 0.35);
    vec3 bottom = mix(vColor.rgb, vec3(0.15, 0.15, 0.25), 0.45);
    vec3 colour = mix(top, bottom, smoothstep(0.0, 1.0, vLocal.y));
    float shine = smoothstep(0.35, 0.0, abs(vLocal.x - 0.5)) * 0.12 * (1.0 - vLocal.y);
    fragColor = vec4(colour + shine, 1.0);
}
)";

const char* const kBackdropShader = R"(
#pragma stage vertex
#version 460 core
out vec2 vUv;
void main() {
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    vUv = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
#pragma stage fragment
#version 460 core
in vec2 vUv;
out vec4 fragColor;
uniform float uTime;
void main() {
    vec3 top = vec3(0.06, 0.08, 0.13);
    vec3 bottom = vec3(0.12, 0.09, 0.16);
    vec3 colour = mix(top, bottom, vUv.y);
    // langsam wanderndes Gitter als Tiefe
    vec2 grid = abs(fract(vUv * vec2(28.0, 16.0) + vec2(uTime * 0.02, 0.0)) - 0.5);
    float lines = smoothstep(0.48, 0.5, max(grid.x, grid.y));
    fragColor = vec4(colour + lines * 0.03, 1.0);
}
)";

// ─── Zeichnen ───────────────────────────────────────────────────────────────

struct Scene {
    GLuint vao{0};
    GLuint vbo{0};
    GLuint ebo{0};
    GLsizei indexCount{0};
    Shader textShader;
    Shader backdropShader;
    GLuint emptyVao{0};
};

void UploadGeometry(Scene& scene, const TextGeometry& geometry) {
    glCreateVertexArrays(1, &scene.vao);
    glCreateBuffers(1, &scene.vbo);
    glCreateBuffers(1, &scene.ebo);

    glNamedBufferData(scene.vbo,
                      static_cast<GLsizeiptr>(geometry.vertices.size() * sizeof(Vertex2d)),
                      geometry.vertices.data(), GL_STATIC_DRAW);
    glNamedBufferData(scene.ebo,
                      static_cast<GLsizeiptr>(geometry.indices.size() * sizeof(u32)),
                      geometry.indices.data(), GL_STATIC_DRAW);

    // Offsets von Hand: Vertex2d enthält glm-Typen, offsetof wäre nicht portabel
    constexpr GLsizei stride = sizeof(Vertex2d);
    constexpr std::size_t offsetPosition = 0;
    constexpr std::size_t offsetLocal = sizeof(vec2);
    constexpr std::size_t offsetColor = offsetLocal + sizeof(vec2);
    constexpr std::size_t offsetCenter = offsetColor + sizeof(vec4);

    glVertexArrayVertexBuffer(scene.vao, 0, scene.vbo, 0, stride);
    glVertexArrayAttribFormat(scene.vao, 0, 2, GL_FLOAT, GL_FALSE,
                              static_cast<GLuint>(offsetPosition));
    glVertexArrayAttribBinding(scene.vao, 0, 0);
    glEnableVertexArrayAttrib(scene.vao, 0);
    glVertexArrayAttribFormat(scene.vao, 1, 2, GL_FLOAT, GL_FALSE,
                              static_cast<GLuint>(offsetLocal));
    glVertexArrayAttribBinding(scene.vao, 1, 0);
    glEnableVertexArrayAttrib(scene.vao, 1);
    glVertexArrayAttribFormat(scene.vao, 2, 4, GL_FLOAT, GL_FALSE,
                              static_cast<GLuint>(offsetColor));
    glVertexArrayAttribBinding(scene.vao, 2, 0);
    glEnableVertexArrayAttrib(scene.vao, 2);
    glVertexArrayAttribFormat(scene.vao, 3, 2, GL_FLOAT, GL_FALSE,
                              static_cast<GLuint>(offsetCenter));
    glVertexArrayAttribBinding(scene.vao, 3, 0);
    glEnableVertexArrayAttrib(scene.vao, 3);
    glVertexArrayElementBuffer(scene.vao, scene.ebo);

    glCreateVertexArrays(1, &scene.emptyVao);
    scene.indexCount = static_cast<GLsizei>(geometry.indices.size());
}

// designWidth/Height: Koordinatenraum der Geometrie; viewportWidth/Height: Ziel
// im Framebuffer (so wird auf HiDPI proportional skaliert)
void DrawScene(Scene& scene, i32 designWidth, i32 designHeight, i32 viewportWidth,
               i32 viewportHeight, f32 time) {
    glViewport(0, 0, viewportWidth, viewportHeight);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Hintergrund
    glBindVertexArray(scene.emptyVao);
    scene.backdropShader.use();
    scene.backdropShader.setUniform("uTime", time);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    // Ortho-Projektion in Bildpixeln (y nach unten)
    const mat4 projection = glm::ortho(0.0f, static_cast<f32>(designWidth),
                                       static_cast<f32>(designHeight), 0.0f, -1.0f, 1.0f);

    scene.textShader.use();
    scene.textShader.setUniform("uProjection", projection);
    scene.textShader.setUniform("uTime", time);
    glBindVertexArray(scene.vao);

    // 1. Schatten: versetzt, leicht größer, halbtransparent
    scene.textShader.setUniform("uScale", 1.02f);
    scene.textShader.setUniform("uOffset", vec2(5.0f, 6.0f));
    scene.textShader.setUniform("uMode", 2);
    scene.textShader.setUniform("uAlpha", 0.9f);
    scene.textShader.setUniform("uWave", 0.0f);
    glDrawElements(GL_TRIANGLES, scene.indexCount, GL_UNSIGNED_INT, nullptr);

    // 2. Kontur: 1,08-fach skaliert in Akzentfarbe
    scene.textShader.setUniform("uScale", 1.08f);
    scene.textShader.setUniform("uOffset", vec2(0.0f, 0.0f));
    scene.textShader.setUniform("uMode", 1);
    scene.textShader.setUniform("uOutlineColor", vec3(0.10f, 0.12f, 0.18f));
    scene.textShader.setUniform("uAlpha", 1.0f);
    glDrawElements(GL_TRIANGLES, scene.indexCount, GL_UNSIGNED_INT, nullptr);

    // 3. Füllung mit Schwingung
    scene.textShader.setUniform("uScale", 1.0f);
    scene.textShader.setUniform("uMode", 0);
    scene.textShader.setUniform("uWave", 3.0f);
    glDrawElements(GL_TRIANGLES, scene.indexCount, GL_UNSIGNED_INT, nullptr);

    glBindVertexArray(0);
}

// ─── Headless: ein Bild über verstecktes sgl-Fenster und FBO ────────────────────────────────────

bool RenderFrameToFile(const std::string& file, i32 width, i32 height, f32 time) {
    auto headless = fontdemo::CreateWindow(width, height, "sgl (headless)", 0, false);
    if (!headless) {
        SLOGE("Kein OpenGL-Kontext fuer den Headless-Modus verfuegbar.");
        return false;
    }

    // doppelte Auflösung rendern und herunterskalieren = weiche Kanten
    const i32 renderWidth = width * 2;
    const i32 renderHeight = height * 2;
    GLuint framebuffer = 0;
    GLuint colour = 0;
    glCreateFramebuffers(1, &framebuffer);
    glCreateRenderbuffers(1, &colour);
    glNamedRenderbufferStorage(colour, GL_RGBA8, renderWidth, renderHeight);
    glNamedFramebufferRenderbuffer(framebuffer, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colour);
    glNamedFramebufferDrawBuffer(framebuffer, GL_COLOR_ATTACHMENT0);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    Scene scene;
    scene.textShader = Shader::FromSource(kTextShader);
    scene.backdropShader = Shader::FromSource(kBackdropShader);

    const std::filesystem::path fontFile = FindSystemFont();
    if (fontFile.empty()) {
        SLOGE("Keine Schrift gefunden (SGL_FONT setzen).");
        return false;
    }
    const Font2d font = Font2d::FromFile(fontFile);

    // Geometrie in doppelter Auflösung aufbauen, damit sie scharf bleibt;
    // beide Texte werden auf die verfügbare Breite umbrochen und der Block
    // senkrecht mittig gesetzt
    const f32 pixelSize = 34.0f * 2.0f;
    std::vector<std::string> lines = WrapText(font, kText, pixelSize, width * 2.0f - 160.0f);
    const std::vector<std::string> subLines = WrapText(font, kSubText, pixelSize * 0.7f,
                                                      width * 2.0f - 160.0f);
    const f32 blockHeight = static_cast<f32>(lines.size()) * font.lineHeight() * pixelSize * 1.15f +
                            static_cast<f32>(subLines.size()) * font.lineHeight() * pixelSize * 0.7f *
                                1.15f;
    const f32 originY = std::max((height * 2.0f - blockHeight) * 0.5f, 40.0f);
    const TextGeometry mainGeometry = BuildText(font, lines, pixelSize, 80.0f * 2.0f, originY);
    TextGeometry geometry = mainGeometry;

    // Unterzeile kleiner darunter setzen
    const f32 subY = originY + static_cast<f32>(lines.size()) * font.lineHeight() * pixelSize * 1.15f;
    const TextGeometry subGeometry = BuildText(font, subLines, pixelSize * 0.7f, 80.0f * 2.0f, subY);
    const u32 offset = static_cast<u32>(geometry.vertices.size());
    geometry.vertices.insert(geometry.vertices.end(), subGeometry.vertices.begin(),
                             subGeometry.vertices.end());
    for (const u32 index : subGeometry.indices) {
        geometry.indices.push_back(offset + index);
    }
    UploadGeometry(scene, geometry);
    DrawScene(scene, width * 2, height * 2, renderWidth, renderHeight, time);

    std::vector<u8> pixels(static_cast<std::size_t>(renderWidth) * renderHeight * 4);
    glReadPixels(0, 0, renderWidth, renderHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    Image image(renderWidth, renderHeight, ImageFormat::Rgba);
    std::memcpy(image.data(), pixels.data(), pixels.size());
    image.flipVertically();
    ImageIO::Save(image.scaled(0.5, Interpolation::Bilinear), file);

    glDeleteRenderbuffers(1, &colour);
    glDeleteFramebuffers(1, &framebuffer);
    SLOGI("%s geschrieben (%dx%d, %zu Dreiecke, %zu Eckpunkte)", file.c_str(), width,
          height, geometry.indices.size() / 3, geometry.vertices.size());
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    if (!fontdemo::Initialize()) {
        return 1;
    }

    // Headless-Modus: --frame <datei> [breite hoehe]
    if (argc >= 3 && std::string(argv[1]) == "--frame") {
        const i32 width = argc >= 4 ? static_cast<i32>(std::strtol(argv[3], nullptr, 10)) : 1280;
        const i32 height = argc >= 5 ? static_cast<i32>(std::strtol(argv[4], nullptr, 10)) : 720;
        const bool ok = RenderFrameToFile(argv[2], width, height, 2.4f);
        sgl::Terminate();
        return ok ? 0 : 1;
    }

    // Fenstermodus
    auto window = fontdemo::CreateWindow(1280, 720, "sgl · Font2d mit Shadern", 4, true);
    if (!window) {
        SLOGE("Fenster nicht moeglich. Headless: %s --frame bild.png", argv[0]);
        sgl::Terminate();
        return 1;
    }

    const std::filesystem::path fontFile = FindSystemFont();
    if (fontFile.empty()) {
        SLOGE("Keine Schrift gefunden (SGL_FONT setzen).");
        window.reset();
        sgl::Terminate();
        return 1;
    }
    const Font2d font = Font2d::FromFile(fontFile);
    if (!font.isValid()) {
        SLOGE("Schrift konnte nicht geladen werden: %s", fontFile.string().c_str());
        window.reset();
        sgl::Terminate();
        return 1;
    }

    Scene scene;
    scene.textShader = Shader::FromSource(kTextShader);
    scene.backdropShader = Shader::FromSource(kBackdropShader);

    const f32 pixelSize = 30.0f;
    const std::vector<std::string> lines = WrapText(font, kText, pixelSize, 1280.0f - 160.0f);
    const f32 blockHeight = static_cast<f32>(lines.size()) * font.lineHeight() * pixelSize * 1.15f;
    const TextGeometry geometry = BuildText(font, lines, pixelSize, 40.0f,
                                            std::max((720.0f - blockHeight) * 0.5f, 40.0f));
    UploadGeometry(scene, geometry);

    window->setKeyCallback([&](const Key key, i32, const KeyAction action, Modifiers) {
        if (key == Key::Escape && action == KeyAction::Press) {
            window->setCloseRequested(true);
        }
    });

    while (!window->isCloseRequested()) {
        window->makeContextCurrent();
        const ivec2 framebuffer = window->getFramebufferSize();
        if (framebuffer.x > 0 && framebuffer.y > 0) {
            // Zeichenflaeche bleibt 1280x720, der Viewport folgt dem Framebuffer
            DrawScene(scene, 1280, 720, framebuffer.x, framebuffer.y, sgl::GetTime());
            window->swapBuffers();
        }
        sgl::PollEvents();
    }

    window.reset();
    sgl::Terminate();
    return 0;
}
