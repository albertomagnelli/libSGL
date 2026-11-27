#include <sgl/Shader.hpp>

#include <glm/gtc/type_ptr.hpp>
#include <sgl/Log.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace sgl {

using namespace gl;
namespace {

// ─── Kleine Helfer ───────────────────────────────────────────────────────────

std::string_view TrimLeft(const std::string_view text) {
    std::size_t i = 0;
    while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) {
        ++i;
    }
    return text.substr(i);
}

std::string_view TrimRight(const std::string_view text) {
    std::size_t i = text.size();
    while (i > 0 && std::isspace(static_cast<unsigned char>(text[i - 1]))) {
        --i;
    }
    return text.substr(0, i);
}

std::string_view Trim(const std::string_view text) {
    return TrimRight(TrimLeft(text));
}

std::string ToLower(const std::string_view text) {
    std::string result(text);
    std::ranges::transform(result, result.begin(), [](const unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return result;
}

// Prüft, ob die Zeile mit einer Präprozessor-Anweisung beginnt ("#include",
// "#pragma" usw.). Nach dem Namen muss ein Leerraum oder Zeilenende folgen,
// damit "#included" nicht als "#include" durchgeht.
bool IsDirective(const std::string_view line, const std::string_view name) {
    std::string_view rest = TrimLeft(line);
    if (rest.empty() || rest.front() != '#') {
        return false;
    }
    rest.remove_prefix(1);
    rest = TrimLeft(rest);
    if (rest.substr(0, name.size()) != name) {
        return false;
    }
    if (rest.size() == name.size()) {
        return true;
    }
    const char next = rest[name.size()];
    return std::isspace(static_cast<unsigned char>(next)) != 0;
}

std::string ReadFileText(const std::filesystem::path& file) {
    std::ifstream input(file, std::ios::binary);
    if (!input) {
        throw ShaderError("Datei konnte nicht geöffnet werden: " + file.string());
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

// Beginnt der Quelltext mit einer #version-Anweisung? Kommentare und Leerzeilen
// davor sind erlaubt, alles andere würde die Injektion verhindern.
bool HasVersionDirective(const std::string_view text) {
    std::size_t i = 0;
    while (i < text.size()) {
        const char c = text[i];
        if (std::isspace(static_cast<unsigned char>(c))) {
            ++i;
        } else if (c == '/' && i + 1 < text.size() && text[i + 1] == '/') {
            i = text.find('\n', i);
            if (i == std::string_view::npos) {
                return false;
            }
        } else if (c == '/' && i + 1 < text.size() && text[i + 1] == '*') {
            const std::size_t end = text.find("*/", i + 2);
            if (end == std::string_view::npos) {
                return false;
            }
            i = end + 2;
        } else {
            break;
        }
    }
    return IsDirective(text.substr(i), "version");
}

// Enthält der Text außer Kommentaren und Leerraum noch echten Code? Wird für
// die Frage gebraucht, ob ein Abschnitt vor dem ersten Stufenmarker Inhalt hat.
bool HasCode(const std::string_view text) {
    std::size_t i = 0;
    while (i < text.size()) {
        if (std::isspace(static_cast<unsigned char>(text[i]))) {
            ++i;
            continue;
        }
        if (text.compare(i, 2, "//") == 0) {
            const std::size_t newline = text.find('\n', i);
            if (newline == std::string_view::npos) {
                return false;
            }
            i = newline + 1;
            continue;
        }
        if (text.compare(i, 2, "/*") == 0) {
            const std::size_t end = text.find("*/", i + 2);
            if (end == std::string_view::npos) {
                return false;
            }
            i = end + 2;
            continue;
        }
        return true;
    }
    return false;
}

// Zeilenweise Iteration, die den Zeileninhalt ohne '\n' und ohne '\r' liefert.
template <class Functor>
void ForEachLine(const std::string_view text, Functor&& functor) {
    std::size_t start = 0;
    i32 lineNumber = 1;
    while (start <= text.size()) {
        const std::size_t end = text.find('\n', start);
        std::string_view line = (end == std::string_view::npos) ? text.substr(start)
                                                               : text.substr(start, end - start);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        functor(line, lineNumber);
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
        ++lineNumber;
    }
}

[[noreturn]] void ThrowIncludeError(const std::string& message) {
    throw ShaderError(message);
}

// Entfernt ein angehängtes " (stufe)" aus dem Anzeigenamen, damit Meldungen
// "datei.glsl:12" statt "datei.glsl (fragment):12" lauten.
std::string SourceNameForMap(const std::string_view debugName) {
    std::string           name(debugName);
    constexpr ShaderStage stages[] = {ShaderStage::Vertex,       ShaderStage::TessControl,
                                      ShaderStage::TessEvaluation, ShaderStage::Geometry,
                                      ShaderStage::Fragment,     ShaderStage::Compute};
    for (const ShaderStage stage : stages) {
        if (const std::string suffix = " (" + std::string(ToString(stage)) + ")"; name.size() > suffix.size() &&
                                                                                  name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0) {
            name.erase(name.size() - suffix.size());
            break;
        }
    }
    return name;
}

// ─── Include-Auflösung ───────────────────────────────────────────────────────

// Löst #include rekursiv auf, führt eine Zeilenkarte und verhindert doppeltes
// Einbinden derselben Datei (verhält sich wie "#pragma once").
class IncludeExpander {
public:
    explicit IncludeExpander(const ShaderSourceOptions& options) : m_options(options) {}

    ResolvedSource expand(std::string_view text, const std::filesystem::path& baseDir,
                          const std::string& debugName) {
        const std::string sourceName = SourceNameForMap(debugName);
        std::string body;
        std::vector<std::pair<std::string, i32>> lineMap;
        expandInto(text, baseDir, sourceName, body, lineMap, 0);

        // #version und #define müssen vor dem eigentlichen Quelltext stehen.
        std::string prefix;
        std::vector<std::pair<std::string, i32>> prefixMap;
        const bool hasVersion = HasVersionDirective(body);
        if (m_options.version.has_value() && !hasVersion) {
            prefix += "#version " + std::to_string(*m_options.version);
            if (!m_options.versionProfile.empty()) {
                prefix += " " + m_options.versionProfile;
            }
            prefix += '\n';
            prefixMap.emplace_back(sourceName, 0);
        }
        for (const auto& [name, value] : m_options.defines) {
            prefix.append("#define ").append( name ).append(" ").append( value).append("\n");
            prefixMap.emplace_back(sourceName, 0);
        }

        // Steht #version schon im Quelltext, müssen die Defines dahinter
        // stehen -- "#version" muss laut GLSL die erste Zeile sein.
        if (hasVersion) {
            if (const std::size_t lineEnd = body.find('\n'); lineEnd != std::string::npos) {
                prefix = body.substr(0, lineEnd + 1) + prefix;
                body = body.substr(lineEnd + 1);
            }
        }

        prefix += body;
        prefixMap.insert(prefixMap.end(), lineMap.begin(), lineMap.end());
        return {std::move(prefix), std::move(prefixMap)};
    }

private:
    void expandInto(const std::string_view text, const std::filesystem::path& baseDir,
                    const std::string& debugName, std::string& out,
                    std::vector<std::pair<std::string, i32>>& lineMap, const std::size_t depth) {
        ForEachLine(text, [&](const std::string_view line, i32 lineNumber) {
            if (IsDirective(line, "include")) {
                includeLine(line, baseDir, debugName, out, lineMap, depth, lineNumber);
                return;
            }
            if (IsDirective(line, "pragma")) {
                // Stufenmarker wurden bereits ausgewertet, "once" erledigt der
                // Pfad-Vergleich im Expander.
                if (const std::string_view rest = Trim(TrimLeft(line).substr(1)); rest.substr(0, 5) == "stage" || rest.substr(0, 6) == "shader" ||
                                                                                  rest.substr(0, 4) == "once") {
                    lineMap.emplace_back(debugName, lineNumber);
                    out += '\n';
                    return;
                }
            }
            out.append(line.data(), line.size());
            out += '\n';
            lineMap.emplace_back(debugName, lineNumber);
        });
        (void)depth;
    }

    void includeLine(const std::string_view line, const std::filesystem::path& baseDir,
                     const std::string& debugName, std::string& out,
                     std::vector<std::pair<std::string, i32>>& lineMap, std::size_t depth,
                     i32 lineNumber) {
        std::string_view rest = TrimLeft(TrimLeft(line).substr(1));
        rest.remove_prefix(std::string_view("include").size());
        rest = Trim(rest);

        if (rest.size() < 3) {
            ThrowIncludeError(debugName + ":" + std::to_string(lineNumber) +
                              ": unvollständige #include-Anweisung");
        }
        const char opener = rest.front();
        if (opener != '"' && opener != '<') {
            ThrowIncludeError(debugName + ":" + std::to_string(lineNumber) +
                              ": #include erwartet \"datei\" oder <datei>");
        }
        const char closer = (opener == '"') ? '"' : '>';
        const std::size_t close = rest.find(closer, 1);
        if (close == std::string_view::npos) {
            ThrowIncludeError(debugName + ":" + std::to_string(lineNumber) +
                              ": schließendes " + std::string(1, closer) + " fehlt");
        }
        const std::string target(rest.substr(1, close - 1));

        if (depth + 1 > m_options.maxIncludeDepth) {
            ThrowIncludeError(debugName + ":" + std::to_string(lineNumber) +
                              ": Include-Tiefe " +
                              std::to_string(m_options.maxIncludeDepth) +
                              " überschritten (Rekursion?)");
        }
        const std::optional<std::filesystem::path> found =
            findInclude(target, opener == '<', baseDir);
        if (!found.has_value()) {
            std::string searched;
            for (const auto& dir : m_options.includePaths) {
                searched += "\n  " + dir.string();
            }
            ThrowIncludeError(debugName + ":" + std::to_string(lineNumber) +
                              ": Include '" + target + "' nicht gefunden (Basis: " +
                              baseDir.string() + ")" +
                              (searched.empty() ? "" : "; Suchpfade:" + searched));
        }

        std::error_code ec;
        const std::filesystem::path canonical =
            std::filesystem::weakly_canonical(*found, ec);
        if (const std::string key = (ec ? found->string() : canonical.string()); !m_included.insert(key).second) {
            // Bereits eingebunden -> überspringen, aber Zeilenkarte konsistent
            // halten, damit nachfolgende Zeilen weiter stimmen.
            lineMap.emplace_back(debugName, lineNumber);
            out += '\n';
            return;
        }

        const std::string includedText = ReadFileText(*found);
        expandInto(includedText, found->parent_path(),
                   found->string(), out, lineMap, depth + 1);
        lineMap.emplace_back(debugName, lineNumber);
        out += '\n';
    }

    std::optional<std::filesystem::path> findInclude(const std::string& target, const bool angled,
                                                     const std::filesystem::path& baseDir) const {
        std::error_code ec;
        const auto usable = [&ec](const std::filesystem::path& candidate) {
            return std::filesystem::is_regular_file(candidate, ec) ? std::optional(candidate)
                                                                   : std::nullopt;
        };
        if (!angled) {
            if (auto hit = usable(baseDir / target)) {
                return hit;
            }
        }
        for (const auto& dir : m_options.includePaths) {
            if (auto hit = usable(dir / target)) {
                return hit;
            }
        }
        return std::nullopt;
    }

    const ShaderSourceOptions& m_options;
    std::unordered_set<std::string> m_included{};
};

// Baut aus dem Stufennamen eine Zeilenkarte-freundliche Bezeichnung.
std::string StageDebugName(const std::string_view debugName, const ShaderStage stage) {
    std::string result(debugName);
    result += " (";
    result += ToString(stage);
    result += ')';
    return result;
}

}  // namespace

// ─── Stufen-Namen ────────────────────────────────────────────────────────────

std::string_view ToString(const ShaderStage stage) {
    switch (stage) {
        case ShaderStage::Vertex:
            return "vertex";
        case ShaderStage::TessControl:
            return "tesscontrol";
        case ShaderStage::TessEvaluation:
            return "tessevaluation";
        case ShaderStage::Geometry:
            return "geometry";
        case ShaderStage::Fragment:
            return "fragment";
        case ShaderStage::Compute:
            return "compute";
    }
    return "unbekannt";
}

std::string_view FileExtension(const ShaderStage stage) {
    switch (stage) {
        case ShaderStage::Vertex:
            return "vert";
        case ShaderStage::TessControl:
            return "tesc";
        case ShaderStage::TessEvaluation:
            return "tese";
        case ShaderStage::Geometry:
            return "geom";
        case ShaderStage::Fragment:
            return "frag";
        case ShaderStage::Compute:
            return "comp";
    }
    return "glsl";
}

std::optional<ShaderStage> StageFromName(const std::string_view name) {
    const std::string key = ToLower(name);
    if (key == "vertex" || key == "vert" || key == "vs") {
        return ShaderStage::Vertex;
    }
    if (key == "fragment" || key == "frag" || key == "fs" || key == "pixel") {
        return ShaderStage::Fragment;
    }
    if (key == "geometry" || key == "geom" || key == "gs") {
        return ShaderStage::Geometry;
    }
    if (key == "tesscontrol" || key == "tess_control" || key == "tesc" || key == "control") {
        return ShaderStage::TessControl;
    }
    if (key == "tessevaluation" || key == "tess_evaluation" || key == "tese" ||
        key == "evaluation") {
        return ShaderStage::TessEvaluation;
    }
    if (key == "compute" || key == "comp" || key == "cs") {
        return ShaderStage::Compute;
    }
    return std::nullopt;
}

std::optional<ShaderStage> StageFromExtension(const std::string_view extension) {
    std::string key = ToLower(extension);
    if (!key.empty() && key.front() == '.') {
        key.erase(key.begin());
    }
    if (const std::size_t slash = key.find_last_of("/\\."); slash != std::string::npos && key[slash] == '.') {
        key = key.substr(slash + 1);
    }
    return StageFromName(key);
}

// ─── ResolvedSource ──────────────────────────────────────────────────────────

ResolvedSource::ResolvedSource(std::string text,
                               std::vector<std::pair<std::string, i32>> lineMap)
    : m_text(std::move(text)), m_lineMap(std::move(lineMap)) {}

std::string ResolvedSource::originOf(const std::size_t generatedLine) const {
    if (m_lineMap.empty()) {
        return "<generiert>";
    }
    std::size_t index = (generatedLine == 0) ? 0 : generatedLine - 1;
    if (index >= m_lineMap.size()) {
        index = m_lineMap.size() - 1;
    }
    const auto& entry = m_lineMap[index];
    if (entry.first.empty()) {
        return "<generiert>";
    }
    return entry.first + ":" + std::to_string(entry.second);
}

// ─── Stufenaufteilung ────────────────────────────────────────────────────────

std::vector<StageSource> SplitStages(const std::string_view source,
                                    const std::filesystem::path& baseDir,
                                    const std::string_view debugName,
                                    const std::optional<ShaderStage> defaultStage,
                                    const std::filesystem::path& fileHint) {
    struct Section {
        std::optional<ShaderStage> stage;
        std::string text;
    };
    std::vector<Section> sections;
    sections.push_back({std::nullopt, {}});

    ForEachLine(source, [&](const std::string_view line, const i32 lineNumber) {
        std::string_view rest = TrimLeft(line);
        bool isMarker = false;
        std::string stageName;
        if (IsDirective(rest, "pragma")) {
            rest.remove_prefix(1);
            rest = TrimLeft(rest);
            rest.remove_prefix(std::string_view("pragma").size());
            rest = Trim(rest);
            for (const std::string_view keyword : {"stage", "shader"}) {
                if (rest.substr(0, keyword.size()) != keyword) {
                    continue;
                }
                if (rest.size() > keyword.size() &&
                    !std::isspace(static_cast<unsigned char>(rest[keyword.size()]))) {
                    continue;
                }
                std::string_view name = Trim(rest.substr(keyword.size()));
                const std::size_t space = name.find_first_of(" \t");
                if (space != std::string_view::npos) {
                    name = name.substr(0, space);
                }
                stageName.assign(name);
                isMarker = true;
                break;
            }
        }

        if (!isMarker) {
            sections.back().text.append(line.data(), line.size());
            sections.back().text += '\n';
            return;
        }

        const std::optional<ShaderStage> stage = StageFromName(stageName);
        if (!stage.has_value()) {
            throw ShaderError(std::string(debugName) + ":" + std::to_string(lineNumber) +
                              ": unbekannte Shader-Stufe '" + stageName +
                              "' (erwartet: vertex, tesscontrol, tessevaluation, geometry, "
                              "fragment, compute)");
        }
        sections.push_back({stage, {}});
    });

    const auto isBlank = [](const std::string_view text) { return !HasCode(text); };

    // Text vor dem ersten Stufenmarker ist eine gemeinsame Präambel (typisch:
    // ein geteilter #include oder #define) und geht in JEDE Stufe ein.
    std::string preamble;
    if (sections.size() > 1 && !isBlank(sections.front().text)) {
        preamble = sections.front().text;
    }

    std::vector<StageSource> result;
    for (std::size_t index = 0; index < sections.size(); ++index) {
        auto& section = sections[index];
        if (index == 0 && !preamble.empty()) {
            continue;  // Präambel, keine eigene Stufe
        }
        if (section.stage.has_value() && isBlank(section.text)) {
            continue;  // leerer Abschnitt, z. B. abschließender Marker
        }
        std::optional<ShaderStage> stage = section.stage;
        if (!stage.has_value()) {
            if (isBlank(section.text)) {
                continue;
            }
            stage = defaultStage;
            if (!stage.has_value() && !fileHint.empty()) {
                stage = StageFromExtension(fileHint.extension().string());
            }
            if (!stage.has_value()) {
                throw ShaderError(
                    std::string(debugName) +
                    ": Shader-Stufe nicht bestimmbar – '#pragma stage <stufe>' verwenden oder "
                    "Dateiendung .vert/.tesc/.tese/.geom/.frag/.comp");
            }
        }
        for (const auto& existing : result) {
            if (existing.stage == *stage) {
                throw ShaderError(std::string(debugName) + ": Stufe '" + std::string(ToString(*stage)) +
                                  "' ist mehrfach vorhanden");
            }
        }
        StageSource stageSource;
        stageSource.stage = *stage;
        stageSource.source = preamble + section.text;
        stageSource.baseDir = baseDir;
        stageSource.debugName = StageDebugName(debugName, *stage);
        if (!fileHint.empty()) {
            stageSource.file = fileHint;
        }
        result.push_back(std::move(stageSource));
    }

    if (result.empty()) {
        throw ShaderError(std::string(debugName) + ": keine Shader-Stufe gefunden");
    }
    return result;
}

// ─── Includes auflösen ───────────────────────────────────────────────────────

ResolvedSource ResolveSource(const std::string_view source, const std::filesystem::path& baseDir,
                             const ShaderSourceOptions& options, const std::string_view debugName) {
    IncludeExpander expander(options);
    const std::filesystem::path base = baseDir.empty() ? std::filesystem::path(".") : baseDir;
    return expander.expand(source, base, std::string(debugName));
}

ResolvedSource ResolveFile(const std::filesystem::path& filePath,
                           const ShaderSourceOptions& options) {
    const std::string text = ReadFileText(filePath);
    std::filesystem::path base = filePath.parent_path();
    if (base.empty()) {
        base = ".";
    }
    return ResolveSource(text, base, options, filePath.string());
}

// ─── Treiberprotokoll auf Originalzeilen zurückführen ───────────────────────

namespace {

std::string RemapLogLine(const std::string_view line, const ResolvedSource& source) {
    std::size_t index = 0;
    while (index < line.size() && std::isspace(static_cast<unsigned char>(line[index]))) {
        ++index;
    }
    std::size_t numberEnd = index;
    while (numberEnd < line.size() && std::isdigit(static_cast<unsigned char>(line[numberEnd]))) {
        ++numberEnd;
    }
    if (numberEnd == index) {
        return std::string(line);
    }

    // Mesa/AMD: "0:12(3): error: ..."  ·  NVIDIA: "0(12) : error: ..."
    if (numberEnd < line.size() && (line[numberEnd] == ':' || line[numberEnd] == '(')) {
        const std::size_t lineStart = numberEnd + 1;
        std::size_t lineEnd = lineStart;
        while (lineEnd < line.size() && std::isdigit(static_cast<unsigned char>(line[lineEnd]))) {
            ++lineEnd;
        }
        if (lineEnd == lineStart) {
            return std::string(line);
        }
        const i32 driverLine =
            std::stoi(std::string(line.substr(lineStart, lineEnd - lineStart)));
        std::string result = source.originOf(static_cast<std::size_t>(driverLine));
        result.append(line.substr(lineEnd));
        return result;
    }
    return std::string(line);
}

std::string RemapLog(const std::string_view log, const ResolvedSource& source) {
    std::string result;
    ForEachLine(log, [&](const std::string_view line, i32) {
        result += RemapLogLine(line, source);
        result += '\n';
    });
    while (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }
    return result;
}

// Löscht Shader-Objekte zuverlässig, auch wenn mittendrin geworfen wird.
struct ShaderObjectGuard {
    std::vector<GLuint> ids{};

    ~ShaderObjectGuard() {
        for (const GLuint id : ids) {
            glDeleteShader(id);
        }
    }

    void add(GLuint id) { ids.push_back(id); }

    void detachAll(GLuint program) const {
        for (const GLuint id : ids) {
            glDetachShader(program, id);
        }
    }
};

GLuint CompileStage(const StageSource& stage, const ShaderSourceOptions& options) {
    const ResolvedSource resolved =
        ResolveSource(stage.source, stage.baseDir, options, stage.debugName);
    const GLuint shader = glCreateShader(static_cast<GLenum>(stage.stage));
    if (shader == 0) {
        throw ShaderError("glCreateShader ist für Stufe '" + std::string(ToString(stage.stage)) +
                          "' fehlgeschlagen");
    }

    const char* text = resolved.text().c_str();
    const auto length = static_cast<GLint>(resolved.text().size());
    glShaderSource(shader, 1, &text, &length);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE) {
        GLint logLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        std::string log(logLength > 1 ? static_cast<std::size_t>(logLength - 1) : 0, '\0');
        if (logLength > 1) {
            glGetShaderInfoLog(shader, logLength, nullptr, log.data());
        }
        glDeleteShader(shader);
        throw ShaderError("Kompilieren der Stufe '" + std::string(ToString(stage.stage)) +
                          "' fehlgeschlagen (" + stage.debugName + "):\n" +
                          (log.empty() ? "(kein Protokoll)" : RemapLog(log, resolved)));
    }
    return shader;
}

// Vollbild-Dreieck über den gl_VertexID-Trick: erlaubt Fragment-only-Programme
// (Post-Processing), ohne eine zweite Shader-Datei zu verlangen.
StageSource MakeFullscreenVertexStage(const i32 version) {
    StageSource stage;
    stage.stage = ShaderStage::Vertex;
    stage.source = "#version " + std::to_string(version) + " core\n" +
                   "out vec2 vUv;\n" +
                   "void main() {\n" +
                   "    vec2 corner = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));\n" +
                   "    vUv = corner;\n" +
                   "    gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);\n" +
                   "}\n";
    stage.baseDir = ".";
    stage.debugName = "<generiert> vertex";
    return stage;
}

std::string ShortLabel(const std::vector<StageSource>& stages) {
    std::vector<std::string> parts;
    for (const auto& stage : stages) {
        const std::string name = stage.file.has_value()
                                     ? stage.file->filename().string()
                                     : std::string(ToString(stage.stage));
        if (std::ranges::find(parts, name) == parts.end()) {
            parts.push_back(name);
        }
    }
    std::string label;
    for (const auto& part : parts) {
        if (!label.empty()) {
            label += '+';
        }
        label += part;
    }
    return label.empty() ? "<shader>" : label;
}

}  // namespace

// ─── Erzeugen ───────────────────────────────────────────────────────────────

Shader Shader::FromFile(const std::filesystem::path& file, ShaderSourceOptions options) {
    return FromFiles({file}, std::move(options));
}

Shader Shader::FromFiles(std::initializer_list<std::filesystem::path> files,
                         ShaderSourceOptions options) {
    if (files.size() == 0) {
        throw ShaderError("Shader::FromFiles: leere Dateiliste");
    }
    std::vector<StageSource> stages;
    for (const auto& file : files) {
        const std::string text = ReadFileText(file);
        std::filesystem::path base = file.parent_path();
        if (base.empty()) {
            base = ".";
        }
        for (auto& stage : SplitStages(text, base, file.string(), std::nullopt, file)) {
            for (const auto& existing : stages) {
                if (existing.stage == stage.stage) {
                    throw ShaderError("Stufe '" + std::string(ToString(stage.stage)) +
                                      "' doppelt angegeben (" + existing.debugName + ", " +
                                      stage.debugName + ")");
                }
            }
            stages.push_back(std::move(stage));
        }
    }
    Shader shader;
    shader.m_options = std::move(options);
    shader.build(std::move(stages));
    return shader;
}

Shader Shader::FromSource(const std::string_view source, ShaderSourceOptions options) {
    std::error_code ec;
    std::filesystem::path base = std::filesystem::current_path(ec);
    if (ec) {
        base = ".";
    }
    std::vector<StageSource> stages = SplitStages(source, base, "<string>");
    Shader shader;
    shader.m_options = std::move(options);
    shader.build(std::move(stages));
    return shader;
}

Shader Shader::FromSource(ShaderStage stage, std::string_view source,
                          ShaderSourceOptions options) {
    StageSource stageSource;
    stageSource.stage = stage;
    stageSource.source = std::string(source);
    std::error_code ec;
    stageSource.baseDir = std::filesystem::current_path(ec);
    if (ec) {
        stageSource.baseDir = ".";
    }
    stageSource.debugName = "<string> " + std::string(ToString(stage));
    return FromSources({std::move(stageSource)}, std::move(options));
}

Shader Shader::FromSources(std::vector<StageSource> stages, ShaderSourceOptions options) {
    Shader shader;
    shader.m_options = std::move(options);
    shader.build(std::move(stages));
    return shader;
}

// ─── Lebenszeit ─────────────────────────────────────────────────────────────

Shader::~Shader() {
    release();
}

Shader::Shader(Shader&& other) noexcept
    : m_program(other.m_program),
      m_isLinked(other.m_isLinked),
      m_label(std::move(other.m_label)),
      m_stageInputs(std::move(other.m_stageInputs)),
      m_options(std::move(other.m_options)),
      m_uniforms(std::move(other.m_uniforms)),
      m_uniformCache(std::move(other.m_uniformCache)),
      m_uniformTypes(std::move(other.m_uniformTypes)),
      m_lastError(std::move(other.m_lastError)) {
    other.m_program = 0;
    other.m_isLinked = false;
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        release();
        m_program = other.m_program;
        m_isLinked = other.m_isLinked;
        m_label = std::move(other.m_label);
        m_stageInputs = std::move(other.m_stageInputs);
        m_options = std::move(other.m_options);
        m_uniforms = std::move(other.m_uniforms);
        m_uniformCache = std::move(other.m_uniformCache);
        m_uniformTypes = std::move(other.m_uniformTypes);
        m_lastError = std::move(other.m_lastError);
        other.m_program = 0;
        other.m_isLinked = false;
    }
    return *this;
}

void Shader::release() noexcept {
    if (m_program != 0) {
        glDeleteProgram(m_program);
    }
    m_program = 0;
    m_isLinked = false;
    m_uniforms.clear();
    resetCache();
}

void Shader::resetCache() const {
    m_uniformCache.clear();
    m_uniformTypes.clear();
}

// ─── Aufbau ────────────────────────────────────────────────────────────────

void Shader::build(std::vector<StageSource> stages) {
    if (stages.empty()) {
        throw ShaderError("Shader: keine Stufe übergeben");
    }
    // Reine Fragment-Programme (Post-Processing) bekommen den Vollbild-Vertex.
    if (stages.size() == 1 && stages.front().stage == ShaderStage::Fragment) {
        stages.insert(stages.begin(), MakeFullscreenVertexStage(m_options.version.value_or(460)));
        SLOGD << "Shader ohne Vertex-Stufe: Vollbild-Dreieck wird erzeugt";
    }
    const std::string label = ShortLabel(stages);

    ShaderObjectGuard shaders;
    for (const auto& stage : stages) {
        shaders.add(CompileStage(stage, m_options));
    }

    const GLuint program = glCreateProgram();
    if (program == 0) {
        throw ShaderError(label + ": glCreateProgram fehlgeschlagen");
    }
    for (const GLuint id : shaders.ids) {
        glAttachShader(program, id);
    }
    for (const auto& [name, location] : m_options.attributeBindings) {
        if (location < 0) {
            continue;
        }
        glBindAttribLocation(program, static_cast<GLuint>(location), name.c_str());
    }

    glLinkProgram(program);
    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        GLint logLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
        std::string log(logLength > 1 ? static_cast<std::size_t>(logLength - 1) : 0, '\0');
        if (logLength > 1) {
            glGetProgramInfoLog(program, logLength, nullptr, log.data());
        }
        shaders.detachAll(program);
        glDeleteProgram(program);
        throw ShaderError(label + ": Linken fehlgeschlagen:\n" +
                          (log.empty() ? "(kein Protokoll)" : log));
    }
    shaders.detachAll(program);

    release();
    m_program = program;
    m_isLinked = true;
    m_label = label;
    m_stageInputs = std::move(stages);
    queryActiveUniforms();

    SLOGD << "Shader gelinkt: " << m_label << " (" << m_uniforms.size() << " aktive Uniforms)";
}

bool Shader::reload() {
    if (m_stageInputs.empty()) {
        m_lastError = "reload(): keine Stufen-Eingaben vorhanden";
        SLOGW << m_lastError;
        return false;
    }

    std::vector<StageSource> refreshed = m_stageInputs;
    try {
        for (auto& stage : refreshed) {
            if (stage.file.has_value()) {
                stage.source = ReadFileText(*stage.file);
                std::filesystem::path base = stage.file->parent_path();
                stage.baseDir = base.empty() ? std::filesystem::path(".") : base;
            }
        }
        build(std::move(refreshed));
    } catch (const ShaderError& error) {
        m_lastError = error.what();
        SLOGW << "Shader '" << m_label << "' nicht neu geladen: " << m_lastError;
        return false;
    }

    m_lastError.clear();
    SLOGD << "Shader neu geladen: " << m_label;
    return true;
}

// ─── Binden ────────────────────────────────────────────────────────────────

void Shader::use() const {
    if (m_program != 0) {
        glUseProgram(m_program);
    }
}

void Shader::ensureCurrentProgram() const {
    if (m_program == 0) {
        return;
    }
    GLint currentProgram = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
    if (static_cast<GLuint>(currentProgram) != m_program) {
        glUseProgram(m_program);
    }
}

void Shader::Unbind() {
    glUseProgram(0);
}

Shader::ScopedBind::ScopedBind(const Shader& shader) {
    GLint previous = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &previous);
    m_previousProgram = previous;
    shader.use();
}

Shader::ScopedBind::~ScopedBind() {
    glUseProgram(static_cast<GLuint>(m_previousProgram));
}

// ─── Uniform-Cache ──────────────────────────────────────────────────────────

void Shader::queryActiveUniforms() {
    m_uniforms.clear();
    resetCache();

    GLint uniformCount = 0;
    glGetProgramiv(m_program, GL_ACTIVE_UNIFORMS, &uniformCount);
    GLint maxLength = 0;
    glGetProgramiv(m_program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxLength);
    if (maxLength < 1) {
        maxLength = 128;
    }
    std::vector<char> nameBuffer(static_cast<std::size_t>(maxLength) + 1, '\0');

    for (GLint i = 0; i < uniformCount; ++i) {
        GLsizei written = 0;
        GLint size = 0;
        GLenum type = GL_NONE;
        glGetActiveUniform(m_program, static_cast<GLuint>(i), maxLength, &written, &size, &type,
                           nameBuffer.data());
        nameBuffer[static_cast<std::size_t>(written)] = '\0';

        UniformInfo info;
        info.name.assign(nameBuffer.data(), static_cast<std::size_t>(written));
        info.type = type;
        info.size = size;
        info.location = glGetUniformLocation(m_program, info.name.c_str());
        if (info.name.size() > 3 && info.name.compare(info.name.size() - 3, 3, "[0]") == 0) {
            info.isArray = true;
            const std::string base = info.name.substr(0, info.name.size() - 3);
            // Elementnamen ("uArr[0]") und Basisnamen ("uArr") cachen.
            m_uniformCache.emplace(base, info.location);
            m_uniformTypes.emplace(base, type);
        }
        m_uniformCache.emplace(info.name, info.location);
        m_uniformTypes.emplace(info.name, type);
        m_uniforms.push_back(std::move(info));
    }
}

GLint Shader::uniformLocation(const std::string_view name) const {
    const std::string key(name);
    if (const auto cached = m_uniformCache.find(key); cached != m_uniformCache.end()) {
        return cached->second;
    }
    GLint location = -1;
    if (m_program != 0) {
        location = glGetUniformLocation(m_program, key.c_str());
        if (location < 0) {
            SLOGD << "Uniform '" << key << "' existiert in '" << m_label
                 << "' nicht (oder wurde vom Linker entfernt)";
        }
    }
    m_uniformCache.emplace(key, location);
    return location;
}

bool Shader::hasUniform(const std::string_view name) const {
    return uniformLocation(name) >= 0;
}

GLenum Shader::uniformType(const std::string_view name) const {
    const std::string key(name);
    if (const auto cached = m_uniformTypes.find(key); cached != m_uniformTypes.end()) {
        return cached->second;
    }
    for (const auto& info : m_uniforms) {
        if (info.name == key) {
            return info.type;
        }
    }
    return GL_NONE;
}

std::string Shader::IndexedName(const std::string_view name, const std::size_t index) {
    std::string result(name);
    result += '[';
    result += std::to_string(index);
    result += ']';
    return result;
}

// ─── Uniforms: Skalare ──────────────────────────────────────────────────────

void Shader::setUniform(const std::string_view name, const bool value) const {
    const GLint location = uniformLocation(name);
    ensureCurrentProgram();
    if (location < 0) {
        return;
    }
    glUniform1i(location, value ? 1 : 0);
}

void Shader::setUniform(const std::string_view name, const i32 value) const {
    const GLint location = uniformLocation(name);
    ensureCurrentProgram();
    if (location < 0) {
        return;
    }
    glUniform1i(location, value);
}

void Shader::setUniform(const std::string_view name, const u32 value) const {
    const GLint location = uniformLocation(name);
    ensureCurrentProgram();
    if (location < 0) {
        return;
    }
    glUniform1ui(location, value);
}

void Shader::setUniform(const std::string_view name, const f32 value) const {
    const GLint location = uniformLocation(name);
    ensureCurrentProgram();
    if (location < 0) {
        return;
    }
    glUniform1f(location, value);
}

void Shader::setUniform(const std::string_view name, const f64 value) const {
    const GLint location = uniformLocation(name);
    ensureCurrentProgram();
    if (location < 0) {
        return;
    }
    glUniform1d(location, value);
}

// ─── Uniforms: Vektoren ─────────────────────────────────────────────────────

#define LIBRAY_UNIFORM_VECTOR(cppType, glFunction)                      \
    void Shader::setUniform(std::string_view name, const cppType& value) const { \
        const GLint location = uniformLocation(name);                   \
        ensureCurrentProgram();                                         \
        if (location < 0) {                                             \
            return;                                                     \
        }                                                               \
        glFunction(location, 1, glm::value_ptr(value));                  \
    }

LIBRAY_UNIFORM_VECTOR(vec2, glUniform2fv)
LIBRAY_UNIFORM_VECTOR(vec3, glUniform3fv)
LIBRAY_UNIFORM_VECTOR(vec4, glUniform4fv)
LIBRAY_UNIFORM_VECTOR(ivec2, glUniform2iv)
LIBRAY_UNIFORM_VECTOR(ivec3, glUniform3iv)
LIBRAY_UNIFORM_VECTOR(ivec4, glUniform4iv)
LIBRAY_UNIFORM_VECTOR(uvec2, glUniform2uiv)
LIBRAY_UNIFORM_VECTOR(uvec3, glUniform3uiv)
LIBRAY_UNIFORM_VECTOR(uvec4, glUniform4uiv)
LIBRAY_UNIFORM_VECTOR(dvec2, glUniform2dv)
LIBRAY_UNIFORM_VECTOR(dvec3, glUniform3dv)
LIBRAY_UNIFORM_VECTOR(dvec4, glUniform4dv)

#undef LIBRAY_UNIFORM_VECTOR

// bvec wird als int-Vektor übertragen – bool-Arrays sind im GLSL-Speicher ints.
void Shader::setUniform(const std::string_view name, const glm::bvec2& value) const {
    const GLint location = uniformLocation(name);
    ensureCurrentProgram();
    if (location < 0) {
        return;
    }
    const GLint converted[2] = {value.x ? 1 : 0, value.y ? 1 : 0};
    glUniform2iv(location, 1, converted);
}

void Shader::setUniform(const std::string_view name, const glm::bvec3& value) const {
    const GLint location = uniformLocation(name);
    ensureCurrentProgram();
    if (location < 0) {
        return;
    }
    const GLint converted[3] = {value.x ? 1 : 0, value.y ? 1 : 0, value.z ? 1 : 0};
    glUniform3iv(location, 1, converted);
}

void Shader::setUniform(const std::string_view name, const glm::bvec4& value) const {
    const GLint location = uniformLocation(name);
    ensureCurrentProgram();
    if (location < 0) {
        return;
    }
    const GLint converted[4] = {value.x ? 1 : 0, value.y ? 1 : 0, value.z ? 1 : 0,
                                value.w ? 1 : 0};
    glUniform4iv(location, 1, converted);
}

// ─── Uniforms: Matrizen ─────────────────────────────────────────────────────

#define LIBRAY_UNIFORM_MATRIX(cppType, glFunction)                          \
    void Shader::setUniform(std::string_view name, const cppType& value) const { \
        const GLint location = uniformLocation(name);                       \
        ensureCurrentProgram();                                             \
        if (location < 0) {                                                 \
            return;                                                         \
        }                                                                   \
        glFunction(location, 1, GL_FALSE, glm::value_ptr(value));           \
    }

LIBRAY_UNIFORM_MATRIX(mat2, glUniformMatrix2fv)
LIBRAY_UNIFORM_MATRIX(mat3, glUniformMatrix3fv)
LIBRAY_UNIFORM_MATRIX(mat4, glUniformMatrix4fv)
LIBRAY_UNIFORM_MATRIX(glm::mat2x3, glUniformMatrix2x3fv)
LIBRAY_UNIFORM_MATRIX(glm::mat2x4, glUniformMatrix2x4fv)
LIBRAY_UNIFORM_MATRIX(glm::mat3x2, glUniformMatrix3x2fv)
LIBRAY_UNIFORM_MATRIX(glm::mat3x4, glUniformMatrix3x4fv)
LIBRAY_UNIFORM_MATRIX(glm::mat4x2, glUniformMatrix4x2fv)
LIBRAY_UNIFORM_MATRIX(glm::mat4x3, glUniformMatrix4x3fv)
LIBRAY_UNIFORM_MATRIX(glm::dmat2, glUniformMatrix2dv)
LIBRAY_UNIFORM_MATRIX(glm::dmat3, glUniformMatrix3dv)
LIBRAY_UNIFORM_MATRIX(glm::dmat4, glUniformMatrix4dv)
LIBRAY_UNIFORM_MATRIX(glm::dmat2x3, glUniformMatrix2x3dv)
LIBRAY_UNIFORM_MATRIX(glm::dmat2x4, glUniformMatrix2x4dv)
LIBRAY_UNIFORM_MATRIX(glm::dmat3x2, glUniformMatrix3x2dv)
LIBRAY_UNIFORM_MATRIX(glm::dmat3x4, glUniformMatrix3x4dv)
LIBRAY_UNIFORM_MATRIX(glm::dmat4x2, glUniformMatrix4x2dv)
LIBRAY_UNIFORM_MATRIX(glm::dmat4x3, glUniformMatrix4x3dv)

#undef LIBRAY_UNIFORM_MATRIX

// ─── Uniforms: Arrays ──────────────────────────────────────────────────────

#define LIBRAY_UNIFORM_ARRAY(cppType, glFunction, cppValueType)                       \
    void Shader::setUniform(std::string_view name, const cppType* values,             \
                            GLsizei count) const {                                    \
        const GLint location = uniformLocation(name);                                 \
        ensureCurrentProgram();                                                       \
        if (location < 0 || values == nullptr || count <= 0) {                        \
            return;                                                                    \
        }                                                                              \
        glFunction(location, count, reinterpret_cast<const cppValueType*>(values));    \
    }

LIBRAY_UNIFORM_ARRAY(f32, glUniform1fv, f32)
LIBRAY_UNIFORM_ARRAY(i32, glUniform1iv, GLint)
LIBRAY_UNIFORM_ARRAY(u32, glUniform1uiv, GLuint)
LIBRAY_UNIFORM_ARRAY(vec2, glUniform2fv, f32)
LIBRAY_UNIFORM_ARRAY(vec3, glUniform3fv, f32)
LIBRAY_UNIFORM_ARRAY(vec4, glUniform4fv, f32)
LIBRAY_UNIFORM_ARRAY(ivec2, glUniform2iv, GLint)
LIBRAY_UNIFORM_ARRAY(ivec3, glUniform3iv, GLint)
LIBRAY_UNIFORM_ARRAY(ivec4, glUniform4iv, GLint)
LIBRAY_UNIFORM_ARRAY(uvec2, glUniform2uiv, GLuint)
LIBRAY_UNIFORM_ARRAY(uvec3, glUniform3uiv, GLuint)
LIBRAY_UNIFORM_ARRAY(uvec4, glUniform4uiv, GLuint)

#undef LIBRAY_UNIFORM_ARRAY

#define LIBRAY_UNIFORM_MATRIX_ARRAY(cppType, glFunction, cppValueType)                  \
    void Shader::setUniform(std::string_view name, const cppType* values, GLsizei count, \
                            bool transpose) const {                                     \
        const GLint location = uniformLocation(name);                                   \
        ensureCurrentProgram();                                                         \
        if (location < 0 || values == nullptr || count <= 0) {                          \
            return;                                                                      \
        }                                                                                \
        glFunction(location, count, transpose ? GL_TRUE : GL_FALSE,                      \
                   reinterpret_cast<const cppValueType*>(values));                       \
    }

LIBRAY_UNIFORM_MATRIX_ARRAY(mat2, glUniformMatrix2fv, f32)
LIBRAY_UNIFORM_MATRIX_ARRAY(mat3, glUniformMatrix3fv, f32)
LIBRAY_UNIFORM_MATRIX_ARRAY(mat4, glUniformMatrix4fv, f32)
LIBRAY_UNIFORM_MATRIX_ARRAY(glm::dmat4, glUniformMatrix4dv, f64)

#undef LIBRAY_UNIFORM_MATRIX_ARRAY

// ─── Sampler und Blöcke ─────────────────────────────────────────────────────

void Shader::setTexture(const std::string_view name, const GLuint texture, const i32 unit,
                        const GLenum target) const {
    const GLint location = uniformLocation(name);
    ensureCurrentProgram();
    if (location < 0) {
        return;
    }
    if (unit < 0 || unit >= 32) {
        SLOGW << "Shader '" << m_label << "': Textureinheit " << unit
             << " liegt außerhalb des gültigen Bereichs";
        return;
    }
    glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + unit));
    glBindTexture(target, texture);
    glUniform1i(location, unit);
}

bool Shader::setUniformBlockBinding(const std::string_view blockName, const GLuint binding) const {
    if (m_program == 0) {
        return false;
    }
    const std::string key(blockName);
    const GLuint index = glGetUniformBlockIndex(m_program, key.c_str());
    if (index == GL_INVALID_INDEX) {
        SLOGD << "Uniform-Block '" << key << "' existiert in '" << m_label << "' nicht";
        return false;
    }
    glUniformBlockBinding(m_program, index, binding);
    return true;
}

bool Shader::setStorageBlockBinding(const std::string_view blockName, const GLuint binding) const {
    if (m_program == 0) {
        return false;
    }
    const std::string key(blockName);
    const GLuint index =
        glGetProgramResourceIndex(m_program, GL_SHADER_STORAGE_BLOCK, key.c_str());
    if (index == GL_INVALID_INDEX) {
        SLOGD << "Storage-Block '" << key << "' existiert in '" << m_label << "' nicht";
        return false;
    }
    glShaderStorageBlockBinding(m_program, index, binding);
    return true;
}

// @@APPEND@@
}  // namespace sgl
