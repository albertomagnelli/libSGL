// ─────────────────────────────────────────────────────────────────────────────
// meshfont_3d_example – advanced Beispiel: extrudierte Schrift mit Beleuchtung
//
// Font3d liefert Vorder-/Rückseite und Wandflächen samt Normalen (in EM-Einheiten).
// Das Beispiel baut daraus einen einzigen interleaved Puffer (Position, Normale,
// Materialfarbe), setzt darunter eine Bodenebene mit Gitter und weichem Schatten
// und beleuchtet mit zwei Lichtern (Blinn-Phong plus Rim-Light). Text und Camera
// bewegen sich zeitgesteuert.
//
//   Steuerung (Fenster):  Esc schließt, F toggelt Drahtgitter
//   Headless:             ./meshfont_3d_example --frame bild.png [breite höhe]
//
// Aufbau:
//   1. Font3d laden, Satz zu einem Netz zusammenbauen (Kerning, Extrusion)
//   2. Bodenebene als zweites Netz mit eigenem Shader
//   3. Zeichnen: Boden, dann Schrift mit Tiefentest und Rückseitenaussortierung
// ─────────────────────────────────────────────────────────────────────────────

#include "FontExampleCommon.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

using namespace sgl;
using namespace gl;

namespace {

const char* const kLine1 = "Alle Taxifahrer rasen wie wild im Kreis";
const char* const kLine2 = "und geben vollgaß ÜÄÖüäö";

// ─── Geometrie ──────────────────────────────────────────────────────────────

// position: Weltkoordinaten (Pixel), normal: aus Font3d, color: Materialfarbe
struct Vertex3d {
    vec3 position;
    vec3 normal;
    vec3 color;
};

struct Mesh {
    std::vector<Vertex3d> vertices;
    std::vector<u32> indices;
};

// Setzt den Text als ein Netz; Grundlinie bei y = 0, y zeigt nach oben,
// z ist die Extrusionstiefe in Pixeln.
Mesh BuildTextMesh(const Font3d& font, const std::vector<std::string>& lines, f32 pixelSize,
                   f32 depthEm) {
    Mesh mesh;
    u32 glyphIndex = 0;
    f32 baseline = 0.0f;

    // breiteste Zeile bestimmen, damit alle Zeilen mittig stehen
    f32 widest = 0.0f;
    for (const std::string& line : lines) {
        widest = std::max(widest, font.measure(line, pixelSize));
    }

    for (const std::string& text : lines) {
    f32 penX = -font.measure(text, pixelSize) * 0.5f;
    f32 previous = -1.0f;

    for (const u32 codepoint : DecodeUtf8(text)) {
        if (codepoint == ' ') {
            penX += font.advance(codepoint) * pixelSize;
            previous = -1.0f;
            ++glyphIndex;
            continue;
        }
        if (previous >= 0.0f) {
            penX += font.kerning(static_cast<u32>(previous), codepoint) * pixelSize;
        }

        const GlyphMesh3d glyph = font.glyphMesh3d(codepoint, depthEm, MeshQuality::Normal);
        if (!glyph.isEmpty()) {
            // Farbton wandert über den Satz, Helligkeit je Glyphe leicht variiert
            const f32 hue = std::fmod(static_cast<f32>(glyphIndex) * 7.0f, 360.0f);
            const vec4 tint = fontdemo::HsvToRgb(hue, 0.45f, 0.95f);
            const vec3 colour(tint);

            const u32 base = static_cast<u32>(mesh.vertices.size());
            for (std::size_t i = 0; i < glyph.positions.size(); ++i) {
                const vec3& point = glyph.positions[i];
                const vec3& normal = i < glyph.normals.size() ? glyph.normals[i] : vec3(0.0f, 0.0f, 1.0f);
                Vertex3d vertex;
                vertex.position = vec3(penX + point.x * pixelSize,
                                       baseline + point.y * pixelSize, point.z * pixelSize);
                vertex.normal = normal;
                vertex.color = colour;
                mesh.vertices.push_back(vertex);
            }
            for (const u32 index : glyph.indices) {
                mesh.indices.push_back(base + index);
            }
        }
        penX += font.advance(codepoint) * pixelSize;
        previous = static_cast<f32>(codepoint);
        ++glyphIndex;
    }
    baseline -= font.lineHeight() * pixelSize * 1.05f;
    }
    (void)widest;
    return mesh;
}

// Bodenebene in y = 0 mit grober Unterteilung (Gitter im Fragment-Shader)
Mesh BuildFloor(f32 halfSize, f32 y) {
    Mesh floor;
    const f32 extent[] = {-halfSize, halfSize};
    for (i32 z = 0; z < 2; ++z) {
        for (i32 x = 0; x < 2; ++x) {
            Vertex3d vertex;
            vertex.position = vec3(extent[x], y, extent[z]);
            vertex.normal = vec3(0.0f, 1.0f, 0.0f);
            vertex.color = vec3(0.10f, 0.11f, 0.14f);
            floor.vertices.push_back(vertex);
        }
    }
    floor.indices = {0, 2, 1, 1, 2, 3};
    return floor;
}

// ─── Shader ─────────────────────────────────────────────────────────────────

const char* const kTextShader = R"(
#pragma stage vertex
#version 460 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;

uniform mat4 uMvp;
uniform mat4 uModel;
uniform mat3 uNormalMatrix;

out vec3 vWorld;
out vec3 vNormal;
out vec3 vColor;

void main() {
    vec4 world = uModel * vec4(aPosition, 1.0);
    vWorld = world.xyz;
    vNormal = normalize(uNormalMatrix * aNormal);
    vColor = aColor;
    gl_Position = uMvp * vec4(aPosition, 1.0);
}
#pragma stage fragment
#version 460 core
in vec3 vWorld;
in vec3 vNormal;
in vec3 vColor;
out vec4 fragColor;

uniform vec3  uCamera;
uniform vec3  uKeyLight;      // Richtung zum Hauptlicht
uniform vec3  uFillLight;     // Richtung zum Fülllicht
uniform float uAmbient;
uniform vec3  uRimColor;

void main() {
    vec3 normal = normalize(vNormal);
    vec3 viewDir = normalize(uCamera - vWorld);

    // Hauptlicht: Blinn-Phong
    vec3 keyDir = normalize(uKeyLight);
    float keyDiffuse = max(dot(normal, keyDir), 0.0);
    vec3 halfDir = normalize(keyDir + viewDir);
    float keySpecular = pow(max(dot(normal, halfDir), 0.0), 48.0) * 0.45;

    // Fülllicht: weich von der Gegenseite
    float fillDiffuse = max(dot(normal, normalize(uFillLight)), 0.0) * 0.35;

    // Rim: hellt die Kanten auf, damit die Extrusion sichtbar wird
    float rim = pow(1.0 - max(dot(normal, viewDir), 0.0), 3.0) * 0.5;

    vec3 colour = vColor * (uAmbient + keyDiffuse * 1.05 + fillDiffuse);
    colour += vec3(keySpecular);
    colour += uRimColor * rim;
    fragColor = vec4(colour, 1.0);
}
)";

const char* const kFloorShader = R"(
#pragma stage vertex
#version 460 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;

uniform mat4 uMvp;
out vec3 vWorld;

void main() {
    vWorld = aPosition;
    gl_Position = uMvp * vec4(aPosition, 1.0);
}
#pragma stage fragment
#version 460 core
in vec3 vWorld;
out vec4 fragColor;

uniform vec3  uBaseColor;
uniform float uTime;
uniform vec3  uTextCenter;
uniform vec2  uTextExtent;

void main() {
    // Gitterlinien mit Abstand 50 Einheiten
    vec2 grid = abs(fract(vWorld.xz / 50.0) - 0.5);
    float lines = smoothstep(0.47, 0.5, max(grid.x, grid.y)) * 0.25;

    // weicher Schatten unter dem Text (Ellipse mit weichem Rand)
    vec2 offset = (vWorld.xz - uTextCenter.xz) / uTextExtent;
    float distance = length(offset);
    float shadow = smoothstep(1.0, 0.15, distance) * 0.8;

    // mit der Entfernung ausblenden
    float fade = smoothstep(5200.0, 1200.0, length(vWorld.xz - uTextCenter.xz));

    vec3 colour = uBaseColor * (0.55 + lines);
    colour = mix(colour, colour * 0.25, shadow);
    fragColor = vec4(colour, fade);
}
)";

// ─── GPU-Zustand ────────────────────────────────────────────────────────────

struct GpuMesh {
    GLuint vao{0};
    GLuint vbo{0};
    GLuint ebo{0};
    GLsizei indexCount{0};
};

GpuMesh Upload(const Mesh& mesh) {
    GpuMesh gpu;
    glCreateVertexArrays(1, &gpu.vao);
    glCreateBuffers(1, &gpu.vbo);
    glCreateBuffers(1, &gpu.ebo);

    glNamedBufferData(gpu.vbo, static_cast<GLsizeiptr>(mesh.vertices.size() * sizeof(Vertex3d)),
                      mesh.vertices.data(), GL_STATIC_DRAW);
    glNamedBufferData(gpu.ebo, static_cast<GLsizeiptr>(mesh.indices.size() * sizeof(u32)),
                      mesh.indices.data(), GL_STATIC_DRAW);

    constexpr GLsizei stride = sizeof(Vertex3d);
    constexpr GLsizei offsetNormal = sizeof(vec3);
    constexpr GLsizei offsetColor = 2 * sizeof(vec3);

    glVertexArrayVertexBuffer(gpu.vao, 0, gpu.vbo, 0, stride);
    glVertexArrayAttribFormat(gpu.vao, 0, 3, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(gpu.vao, 0, 0);
    glEnableVertexArrayAttrib(gpu.vao, 0);
    glVertexArrayAttribFormat(gpu.vao, 1, 3, GL_FLOAT, GL_FALSE, offsetNormal);
    glVertexArrayAttribBinding(gpu.vao, 1, 0);
    glEnableVertexArrayAttrib(gpu.vao, 1);
    glVertexArrayAttribFormat(gpu.vao, 2, 3, GL_FLOAT, GL_FALSE, offsetColor);
    glVertexArrayAttribBinding(gpu.vao, 2, 0);
    glEnableVertexArrayAttrib(gpu.vao, 2);
    glVertexArrayElementBuffer(gpu.vao, gpu.ebo);

    gpu.indexCount = static_cast<GLsizei>(mesh.indices.size());
    return gpu;
}

struct Scene {
    GpuMesh text;
    GpuMesh floor;
    Shader textShader;
    Shader floorShader;
    vec3 textCenter{0.0f, 0.0f, 0.0f};
    vec2 textExtent{1000.0f, 200.0f};
};

void DrawScene(Scene& scene, i32 width, i32 height, f32 time, bool wireframe) {
    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.04f, 0.05f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Kamera: leicht über dem Text, schwenkt langsam
    // schräge Ansicht: von rechts oben, damit die Wandflächen sichtbar werden
    const f32 orbit = 0.55f + std::sin(time * 0.25f) * 0.18f;
    const vec3 eye(std::sin(orbit) * 2600.0f, 1100.0f, std::cos(orbit) * 2600.0f);
    const mat4 view = glm::lookAt(eye, scene.textCenter, vec3(0.0f, 1.0f, 0.0f));
    const mat4 projection =
        glm::perspective(glm::radians(38.0f), static_cast<f32>(width) / static_cast<f32>(height),
                         10.0f, 12000.0f);

    // Text dreht sich um die Hochachse und neigt sich leicht
    const mat4 model =
        glm::rotate(mat4(1.0f), std::sin(time * 0.35f) * 0.22f, vec3(0.0f, 1.0f, 0.0f)) *
        glm::rotate(mat4(1.0f), std::sin(time * 0.2f) * 0.06f, vec3(1.0f, 0.0f, 0.0f)) *
        glm::translate(mat4(1.0f), -scene.textCenter);

    // ── Boden ───────────────────────────────────────────────────────────────
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);      // Boden schreibt keinen Tiefenwert
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    scene.floorShader.use();
    scene.floorShader.setUniform("uMvp", projection * view);
    scene.floorShader.setUniform("uBaseColor", vec3(0.10f, 0.11f, 0.14f));
    scene.floorShader.setUniform("uTime", time);
    scene.floorShader.setUniform("uTextCenter", scene.textCenter);
    scene.floorShader.setUniform("uTextExtent", scene.textExtent);
    glBindVertexArray(scene.floor.vao);
    glDrawElements(GL_TRIANGLES, scene.floor.indexCount, GL_UNSIGNED_INT, nullptr);

    // ── Schrift ─────────────────────────────────────────────────────────────
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
    scene.textShader.use();
    scene.textShader.setUniform("uMvp", projection * view * model);
    scene.textShader.setUniform("uModel", model);
    scene.textShader.setUniform("uNormalMatrix", glm::transpose(glm::inverse(mat3(model))));
    scene.textShader.setUniform("uCamera", eye);
    scene.textShader.setUniform("uKeyLight", vec3(-0.45f, 0.85f, 0.55f));
    scene.textShader.setUniform("uFillLight", vec3(0.65f, 0.25f, -0.70f));
    scene.textShader.setUniform("uAmbient", 0.22f);
    scene.textShader.setUniform("uRimColor", vec3(0.30f, 0.45f, 0.85f));
    glBindVertexArray(scene.text.vao);
    glDrawElements(GL_TRIANGLES, scene.text.indexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

// ─── Headless ───────────────────────────────────────────────────────────────

bool RenderFrameToFile(const std::string& file, i32 width, i32 height, f32 time) {
    auto headless = fontdemo::CreateWindow(width, height, "sgl (headless)", 0, false);
    if (!headless) {
        SLOGE("Kein OpenGL-Kontext fuer den Headless-Modus verfuegbar.");
        return false;
    }

    // 2x rendern und verkleinern: weiche Kanten ohne MSAA-Ziel
    const i32 renderWidth = width * 2;
    const i32 renderHeight = height * 2;
    GLuint framebuffer = 0;
    GLuint colour = 0;
    GLuint depth = 0;
    glCreateFramebuffers(1, &framebuffer);
    glCreateRenderbuffers(1, &colour);
    glCreateRenderbuffers(1, &depth);
    glNamedRenderbufferStorage(colour, GL_RGBA8, renderWidth, renderHeight);
    glNamedRenderbufferStorage(depth, GL_DEPTH_COMPONENT24, renderWidth, renderHeight);
    glNamedFramebufferRenderbuffer(framebuffer, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colour);
    glNamedFramebufferRenderbuffer(framebuffer, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
    glNamedFramebufferDrawBuffer(framebuffer, GL_COLOR_ATTACHMENT0);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    const std::filesystem::path fontFile = FindSystemFont();
    if (fontFile.empty()) {
        SLOGE("Keine Schrift gefunden (SGL_FONT setzen).");
        return false;
    }
    const Font3d font = Font3d::FromFile(fontFile);

    Scene scene;
    scene.textShader = Shader::FromSource(kTextShader);
    scene.floorShader = Shader::FromSource(kFloorShader);

    // Schriftgröße so wählen, dass der Satz in die Kameraansicht passt
    const f32 widestEm = std::max(font.measure(kLine1, 1.0f), font.measure(kLine2, 1.0f));
    const f32 pixelSize = 1900.0f / widestEm;
    const f32 depthEm = 0.34f;
    Mesh textMesh = BuildTextMesh(font, {kLine1, kLine2}, pixelSize, depthEm);
    scene.textCenter = vec3(0.0f, font.lineHeight() * pixelSize * 0.5f, 0.0f);
    scene.textExtent = vec2(1900.0f * 0.55f, 1900.0f * 0.14f);
    scene.text = Upload(textMesh);
    scene.floor = Upload(BuildFloor(5200.0f, -120.0f));

    DrawScene(scene, renderWidth, renderHeight, time, false);

    std::vector<u8> pixels(static_cast<std::size_t>(renderWidth) * renderHeight * 4);
    glReadPixels(0, 0, renderWidth, renderHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    Image image(renderWidth, renderHeight, ImageFormat::Rgba);
    std::memcpy(image.data(), pixels.data(), pixels.size());
    image.flipVertically();
    ImageIO::Save(image.scaled(0.5, Interpolation::Bilinear), file);

    SLOGI("%s geschrieben (%dx%d, %zu Dreiecke, %zu Eckpunkte)", file.c_str(), width, height,
          textMesh.indices.size() / 3, textMesh.vertices.size());
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    if (!fontdemo::Initialize()) {
        return 1;
    }

    if (argc >= 3 && std::string(argv[1]) == "--frame") {
        const i32 width = argc >= 4 ? static_cast<i32>(std::strtol(argv[3], nullptr, 10)) : 1280;
        const i32 height = argc >= 5 ? static_cast<i32>(std::strtol(argv[4], nullptr, 10)) : 720;
        const bool ok = RenderFrameToFile(argv[2], width, height, 1.7f);
        sgl::Terminate();
        return ok ? 0 : 1;
    }

    auto window = fontdemo::CreateWindow(1280, 720, "sgl · Font3d mit Beleuchtung", 4, true);
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
    const Font3d font = Font3d::FromFile(fontFile);

    Scene scene;
    scene.textShader = Shader::FromSource(kTextShader);
    scene.floorShader = Shader::FromSource(kFloorShader);

    const f32 widestEm = std::max(font.measure(kLine1, 1.0f), font.measure(kLine2, 1.0f));
    const f32 pixelSize = 1900.0f / widestEm;
    scene.text = Upload(BuildTextMesh(font, {kLine1, kLine2}, pixelSize, 0.34f));
    scene.textCenter = vec3(0.0f, font.lineHeight() * pixelSize * 0.5f, 0.0f);
    scene.textExtent = vec2(1900.0f * 0.55f, 1900.0f * 0.14f);
    scene.floor = Upload(BuildFloor(5200.0f, -120.0f));

    bool wireframe = false;
    window->setKeyCallback([&](const Key key, i32, const KeyAction action, Modifiers) {
        if (action != KeyAction::Press) {
            return;
        }
        if (key == Key::Escape) {
            window->setCloseRequested(true);
        } else if (key == Key::F) {
            wireframe = !wireframe;
        }
    });

    while (!window->isCloseRequested()) {
        window->makeContextCurrent();
        const ivec2 framebuffer = window->getFramebufferSize();
        if (framebuffer.x > 0 && framebuffer.y > 0) {
            DrawScene(scene, framebuffer.x, framebuffer.y, sgl::GetTime(), wireframe);
            window->swapBuffers();
        }
        sgl::PollEvents();
    }

    window.reset();
    sgl::Terminate();
    return 0;
}
