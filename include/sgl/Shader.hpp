#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// GL-Programme mit Stufen, Includes und Uniform-Cache. Öffentliche Schnittstelle dieses Headers:
//
//   ToString, FileExtension, StageFromName, StageFromExtension, ShaderError
//   ResolvedSource, SplitStages, ResolveSource, ResolveFile, Shader
//   FromFile, FromFiles, FromSource, FromSources, reload
//   Unbind, ScopedBind, setUniform, IndexedName, setTexture
//   setUniformBlockBinding, setStorageBlockBinding, uniformLocation, hasUniform, uniformType
//   ensureCurrentProgram, build, release, queryActiveUniforms, resetCache
//
// Beispiel:
//
//   Shader shader = Shader::FromFile("assets/shaders/mesh.vert");
//   shader.use();
//   shader.setUniform("uMvp", mvp);
//   shader.setTexture("uAlbedo", texture.id(), 0);
// ─────────────────────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────────────────────
// sgl · Shader
//
// Verwaltung von OpenGL-Programmen.
//
//   * alle Stufen: Vertex, Tessellation-Control/-Evaluation, Geometry, Fragment,
//     Compute
//   * Quelltext aus Datei, aus String oder aus mehreren Stufen in EINER Datei
//     (Abschnitte über "#pragma stage <name>"); Text vor dem ersten Marker ist
//     eine gemeinsame Präambel und geht in jede Stufe ein
//   * eine reine Fragment-Datei erhält automatisch einen Vollbild-Dreieck-
//     Vertex-Shader (Post-Processing ohne zweite Datei)
//   * "#include" mit Basisverzeichnis, Suchpfaden, Rekursionstiefe und
//     Einmal-Einbinden (verhält sich wie "#pragma once")
//   * Zeilenzuordnung: Treibermeldungen werden auf Datei:Zeile der Original-
//     quellen zurückgeführt (inkl. Include-Dateien)
//   * Uniform-Locations werden gecacht, inkl. Array-Elementnamen ("u[3]")
//   * setUniform ist für Skalare, Vektoren, Matrizen, Arrays und Texturen
//     überladen; Fehler werden als ShaderError geworfen. Setter binden das
//     Programm bei Bedarf selbst (use()/ScopedBind bleibt der saubere Weg).
//   * RAII: Programm wird im Destruktor gelöscht, Kopieren verboten, Verschieben
//     erlaubt; reload() tauscht das Programm nach Änderungen auf der Platte aus,
//     ohne das alte bei einem Fehler zu verlieren
//
// Konventionen des Projekts: camelCase, bool-Getter mit "is", Member "m_".
//
// Beispiel:
//
//   sgl::ShaderSourceOptions options;
//   options.includePaths = {"assets/shaders"};
//   options.defines = {{"MAX_LICHTER", "8"}};
//
//   sgl::Shader shader = sgl::Shader::FromFile("assets/shaders/lit.glsl", options);
//   sgl::Shader::ScopedBind bound(shader);          // bindet und stellt danach wieder her
//   shader.setUniform("uModel", modelMatrix);          // mat4
//   shader.setUniform("uLightCount", 4);               // int
//   shader.setTexture("uAlbedo", texture, 0);          // Einheit 0
//   shader.setUniform("uWeights", 2, 0.5f);             // "uWeights[2]"
//   if (!shader.reload()) { /* alte Fassung läuft weiter */ }
//
// Mehrere Stufen in einer Datei:
//
//   #pragma stage vertex
//   ...
//   #pragma stage fragment
//   ...
// ─────────────────────────────────────────────────────────────────────────────

#include "Types.hpp"
#include "GL.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sgl {

// ─── Shader-Stufen ───────────────────────────────────────────────────────────

enum class ShaderStage : u32 {
    Vertex = GL_VERTEX_SHADER,
    TessControl = GL_TESS_CONTROL_SHADER,
    TessEvaluation = GL_TESS_EVALUATION_SHADER,
    Geometry = GL_GEOMETRY_SHADER,
    Fragment = GL_FRAGMENT_SHADER,
    Compute = GL_COMPUTE_SHADER,
};

// "vertex", "fragment", "geometry", "tesscontrol", "tessevaluation", "compute"
std::string_view ToString(ShaderStage stage);
// Dateiendung ohne Punkt: "vert", "frag", "geom", "tesc", "tese", "comp"
std::string_view FileExtension(ShaderStage stage);
// Akzeptiert "vertex"/"vert"/"glsl_vertex" usw. (Groß-/Kleinschreibung egal).
std::optional<ShaderStage> StageFromName(std::string_view name);
// Akzeptiert "vert", ".vert", "foo.vert".
std::optional<ShaderStage> StageFromExtension(std::string_view extension);

// ─── Fehler ──────────────────────────────────────────────────────────────────

// Wird bei Include-, Kompilier- und Linkfehlern geworfen. Die Meldung enthält
// Stufe, Datei, Zeile und das Treiberprotokoll.
class ShaderError : public std::runtime_error {
public:
    explicit ShaderError(const std::string& message) : std::runtime_error(message) {}
};

// ─── Quelltext-Optionen ──────────────────────────────────────────────────────

struct ShaderSourceOptions {
    // Suchpfade für #include <...> und als Rückfall für #include "..."
    std::vector<std::filesystem::path> includePaths{};
    // Zusätzliche Präprozessor-Definitionen, werden nach #version eingefügt
    std::vector<std::pair<std::string, std::string>> defines{};
    // Wird vorangestellt, wenn der Quelltext mit keiner #version-Zeile beginnt.
    // std::nullopt = nichts injizieren.
    std::optional<i32> version{460};
    std::string versionProfile{"core"};
    // Schutz gegen Include-Rekursion
    std::size_t maxIncludeDepth{32};
    // Attribute vor dem Linken fest verdrahten (Name -> Location)
    std::unordered_map<std::string, i32> attributeBindings{};
};

// ─── Aufgelöster Quelltext ───────────────────────────────────────────────────

// Fertiger GLSL-Text einer Stufe plus Zuordnung "erzeugte Zeile -> Datei:Zeile".
class ResolvedSource {
public:
    ResolvedSource() = default;
    ResolvedSource(std::string text, std::vector<std::pair<std::string, i32>> lineMap);

    [[nodiscard]] const std::string& text() const noexcept { return m_text; }
    // Liefert "assets/lit.glsl:12"; bei generierten Zeilen den Stufennamen.
    std::string originOf(std::size_t generatedLine) const;

private:
    std::string m_text;
    std::vector<std::pair<std::string, i32>> m_lineMap;
};

// Eine Stufe, wie sie in ein Programm eingeht. source enthält den Rohtext
// (bei Dateien den Dateiinhalt), baseDir die Basis für relative Includes.
struct StageSource {
    ShaderStage stage{ShaderStage::Vertex};
    std::string source{};
    std::filesystem::path baseDir{};
    std::string debugName{};                      // z. B. "assets/lit.glsl (fragment)"
    std::optional<std::filesystem::path> file{};  // gesetzt, wenn aus Datei geladen
};

// ─── Freie Funktionen ────────────────────────────────────────────────────────

// Zerlegt einen Quelltext in Stufen. Abschnitte beginnen mit
//   #pragma stage vertex      (alternativ: #pragma shader vertex)
// Text vor dem ersten Abschnitt gehört zu defaultStage; ist keine Stufe
// angegeben, wird sie aus der Dateiendung von fileHint abgeleitet.
// Mehrfach vorkommende Stufen sind ein Fehler.
std::vector<StageSource> SplitStages(std::string_view source,
                                    const std::filesystem::path& baseDir,
                                    std::string_view debugName,
                                    std::optional<ShaderStage> defaultStage = std::nullopt,
                                    const std::filesystem::path& fileHint = {});

// Löst Includes auf, injiziert #version/#define und baut die Zeilenkarte.
// baseDir ist die Basis für #include "..."; debugName erscheint in Meldungen.
[[nodiscard]] ResolvedSource ResolveSource(std::string_view source,
                             const std::filesystem::path& baseDir,
                             const ShaderSourceOptions& options,
                             std::string_view debugName);

// Liest die Datei und ruft ResolveSource mit ihrem Verzeichnis als baseDir auf.
[[nodiscard]] ResolvedSource ResolveFile(const std::filesystem::path& filePath,
                           const ShaderSourceOptions& options);

// ─── Uniform-Beschreibung ────────────────────────────────────────────────────

struct UniformInfo {
    std::string name{};      // bei Arrays der Treibername, z. B. "uLights[0]"
    GLenum type{0};          // GL_FLOAT_VEC3 usw.
    GLint size{0};           // Anzahl Array-Elemente (1 = kein Array)
    GLint location{-1};
    bool isArray{false};
};

// ─── Shader ──────────────────────────────────────────────────────────────────

class Shader {
public:
    Shader() = default;  // leeres Programm, isValid() == false
    ~Shader();

    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    // ── Erzeugen ─────────────────────────────────────────────────────────────
    // Eine Datei; enthält sie mehrere Stufenabschnitte, werden alle verwendet,
    // sonst entscheidet die Endung (.vert/.frag/.geom/.tesc/.tese/.comp).
    [[nodiscard]] static Shader FromFile(const std::filesystem::path& file,
                           ShaderSourceOptions options = {});
    // Mehrere Dateien; jede liefert ihre Stufe über Endung bzw. Abschnitte.
    [[nodiscard]] static Shader FromFiles(std::initializer_list<std::filesystem::path> files,
                            ShaderSourceOptions options = {});
    // Quelltext als String, mehrere Stufen über "#pragma stage ..." möglich.
    [[nodiscard]] static Shader FromSource(std::string_view source,
                             ShaderSourceOptions options = {});
    // Quelltext als String für genau eine Stufe.
    [[nodiscard]] static Shader FromSource(ShaderStage stage,
                             std::string_view source,
                             ShaderSourceOptions options = {});
    // Fertig vorbereitete Stufen (Mehrfachstufen-Dateien, eigene Loader).
    [[nodiscard]] static Shader FromSources(std::vector<StageSource> stages,
                              ShaderSourceOptions options = {});

    // Liest alle Stufen erneut ein (Hot-Reload). Schlägt der Neuaufbau fehl,
    // bleibt das bisherige Programm unverändert und lastError() beschreibt den
    // Fehler. Liefert true bei Erfolg.
    [[nodiscard]] bool reload();

    // ── Zustand ──────────────────────────────────────────────────────────────
    [[nodiscard]] bool isValid() const noexcept { return m_program != 0 && m_isLinked; }
    [[nodiscard]] bool isLinked() const noexcept { return m_isLinked; }
    [[nodiscard]] GLuint id() const noexcept { return m_program; }
    [[nodiscard]] const std::string& label() const noexcept { return m_label; }
    [[nodiscard]] const std::string& lastError() const noexcept { return m_lastError; }
    [[nodiscard]] const std::vector<UniformInfo>& activeUniforms() const noexcept { return m_uniforms; }

    void use() const;          // glUseProgram
    static void Unbind();      // glUseProgram(0)

    // Bindet das Programm für die Lebenszeit des Objekts und stellt danach das
    // vorher aktive Programm wieder her (RAII).
    class ScopedBind {
    public:
        explicit ScopedBind(const Shader& shader);
        ~ScopedBind();
        ScopedBind(const ScopedBind&) = delete;
        ScopedBind& operator=(const ScopedBind&) = delete;

    private:
        GLint m_previousProgram{0};
    };

    // ── Uniforms: Skalare ────────────────────────────────────────────────────
    void setUniform(std::string_view name, bool value) const;
    void setUniform(std::string_view name, i32 value) const;
    void setUniform(std::string_view name, u32 value) const;
    void setUniform(std::string_view name, f32 value) const;
    void setUniform(std::string_view name, f64 value) const;

    // ── Uniforms: Vektoren ───────────────────────────────────────────────────
    void setUniform(std::string_view name, const vec2& value) const;
    void setUniform(std::string_view name, const vec3& value) const;
    void setUniform(std::string_view name, const vec4& value) const;
    void setUniform(std::string_view name, const ivec2& value) const;
    void setUniform(std::string_view name, const ivec3& value) const;
    void setUniform(std::string_view name, const ivec4& value) const;
    void setUniform(std::string_view name, const uvec2& value) const;
    void setUniform(std::string_view name, const uvec3& value) const;
    void setUniform(std::string_view name, const uvec4& value) const;
    void setUniform(std::string_view name, const glm::bvec2& value) const;
    void setUniform(std::string_view name, const glm::bvec3& value) const;
    void setUniform(std::string_view name, const glm::bvec4& value) const;
    void setUniform(std::string_view name, const dvec2& value) const;
    void setUniform(std::string_view name, const dvec3& value) const;
    void setUniform(std::string_view name, const dvec4& value) const;

    // ── Uniforms: Matrizen (transpose nur für dmat-Sonderfälle nötig) ───────
    void setUniform(std::string_view name, const mat2& value) const;
    void setUniform(std::string_view name, const mat3& value) const;
    void setUniform(std::string_view name, const mat4& value) const;
    void setUniform(std::string_view name, const glm::mat2x3& value) const;
    void setUniform(std::string_view name, const glm::mat2x4& value) const;
    void setUniform(std::string_view name, const glm::mat3x2& value) const;
    void setUniform(std::string_view name, const glm::mat3x4& value) const;
    void setUniform(std::string_view name, const glm::mat4x2& value) const;
    void setUniform(std::string_view name, const glm::mat4x3& value) const;
    void setUniform(std::string_view name, const glm::dmat2& value) const;
    void setUniform(std::string_view name, const glm::dmat3& value) const;
    void setUniform(std::string_view name, const glm::dmat4& value) const;
    void setUniform(std::string_view name, const glm::dmat2x3& value) const;
    void setUniform(std::string_view name, const glm::dmat2x4& value) const;
    void setUniform(std::string_view name, const glm::dmat3x2& value) const;
    void setUniform(std::string_view name, const glm::dmat3x4& value) const;
    void setUniform(std::string_view name, const glm::dmat4x2& value) const;
    void setUniform(std::string_view name, const glm::dmat4x3& value) const;

    // ── Uniforms: Arrays ────────────────────────────────────────────────────
    void setUniform(std::string_view name, const f32* values, GLsizei count) const;
    void setUniform(std::string_view name, const i32* values, GLsizei count) const;
    void setUniform(std::string_view name, const u32* values, GLsizei count) const;
    void setUniform(std::string_view name, const vec2* values, GLsizei count) const;
    void setUniform(std::string_view name, const vec3* values, GLsizei count) const;
    void setUniform(std::string_view name, const vec4* values, GLsizei count) const;
    void setUniform(std::string_view name, const ivec2* values, GLsizei count) const;
    void setUniform(std::string_view name, const ivec3* values, GLsizei count) const;
    void setUniform(std::string_view name, const ivec4* values, GLsizei count) const;
    void setUniform(std::string_view name, const uvec2* values, GLsizei count) const;
    void setUniform(std::string_view name, const uvec3* values, GLsizei count) const;
    void setUniform(std::string_view name, const uvec4* values, GLsizei count) const;
    void setUniform(std::string_view name, const mat2* values, GLsizei count,
                    bool transpose = false) const;
    void setUniform(std::string_view name, const mat3* values, GLsizei count,
                    bool transpose = false) const;
    void setUniform(std::string_view name, const mat4* values, GLsizei count,
                    bool transpose = false) const;
    void setUniform(std::string_view name, const glm::dmat4* values, GLsizei count,
                    bool transpose = false) const;

    // Beliebige zusammenpassende Bereiche, z. B. std::span<const vec3>.
    template <class T>
    void setUniform(std::string_view name, std::span<const T> values) const {
        setUniform(name, values.data(), static_cast<GLsizei>(values.size()));
    }

    // Einzelnes Array-Element: setUniform("uWeights", 3, 0.5f) -> "uWeights[3]"
    template <class T>
    void setUniform(std::string_view name, std::size_t index, const T& value) const {
        const std::string indexed = IndexedName(name, index);
        setUniform(indexed, value);
    }

    // ── Sampler und Blöcke ───────────────────────────────────────────────────
    // Bindet die Textur an die Einheit und setzt den Sampler-Uniform auf unit.
    void setTexture(std::string_view name, GLuint texture, i32 unit,
                    GLenum target = GL_TEXTURE_2D) const;
    // glUniformBlockBinding / glShaderStorageBlockBinding
    [[nodiscard]] bool setUniformBlockBinding(std::string_view blockName, GLuint binding) const;
    [[nodiscard]] bool setStorageBlockBinding(std::string_view blockName, GLuint binding) const;

    // ── Abfragen (Ergebnisse werden gecacht) ─────────────────────────────────
    // -1, wenn der Name nicht existiert; setzt zugleich den Cache-Eintrag.
    [[nodiscard]] GLint uniformLocation(std::string_view name) const;
    [[nodiscard]] bool hasUniform(std::string_view name) const;
    [[nodiscard]] GLenum uniformType(std::string_view name) const;
    // Anzahl der Cache-Einträge (Diagnose/Tests)
    [[nodiscard]] std::size_t uniformCacheSize() const noexcept { return m_uniformCache.size(); }

private:
    static std::string IndexedName(std::string_view name, std::size_t index);

    // Bindet das Programm, falls es gerade nicht aktiv ist. glUniform* würde
    // sonst still auf das falsche Programm wirken (GL_INVALID_OPERATION).
    void ensureCurrentProgram() const;

    // Baut Programm + Uniform-Cache, wirft bei Fehlern ShaderError.
    void build(std::vector<StageSource> stages);
    // Entlädt das GL-Programm, ohne die Stufen-Eingaben zu verwerfen.
    void release() noexcept;
    void queryActiveUniforms();
    void resetCache() const;

    GLuint m_program{0};
    bool m_isLinked{false};
    std::string m_label{};
    std::vector<StageSource> m_stageInputs{};
    ShaderSourceOptions m_options{};
    std::vector<UniformInfo> m_uniforms{};
    mutable std::unordered_map<std::string, GLint> m_uniformCache{};
    mutable std::unordered_map<std::string, GLenum> m_uniformTypes{};
    std::string m_lastError{};
};

}  // namespace sgl
