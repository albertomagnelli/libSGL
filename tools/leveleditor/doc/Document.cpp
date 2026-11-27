#include "Document.hpp"

#include "JsonUtil.hpp"
#include "JsonWriter.hpp"

#include "common/Json.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace editor {
namespace {

using sgl::detail::Json;

std::string ReadTextFile(const fs::path &file) {
    std::ifstream input(file, std::ios::binary);
    if (!input) return {};
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

bool WriteTextFile(const fs::path &file, std::string_view text, std::string &error) {
    std::error_code ec;
    if (!file.parent_path().empty()) {
        fs::create_directories(file.parent_path(), ec);
        if (ec) {
            error = "Verzeichnis konnte nicht angelegt werden: " + PathToUtf8(file.parent_path());
            return false;
        }
    }
    std::ofstream output(file, std::ios::binary | std::ios::trunc);
    if (!output) {
        error = "Datei konnte nicht geschrieben werden: " + PathToUtf8(file);
        return false;
    }
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!output) {
        error = "Schreiben fehlgeschlagen: " + PathToUtf8(file);
        return false;
    }
    return true;
}

std::string JsonString(const Json *value, std::string_view fallback = {}) {
    return value != nullptr && value->isString() ? value->string() : std::string(fallback);
}

int JsonInt(const Json *value, int fallback = 0) {
    return value != nullptr ? value->intValue(fallback) : fallback;
}

float JsonFloat(const Json *value, float fallback = 0.0f) {
    return value != nullptr ? value->floatValue(fallback) : fallback;
}

bool JsonBool(const Json *value, bool fallback = false) {
    return value != nullptr ? value->boolValue(fallback) : fallback;
}

void WriteTileSet(JsonWriter &writer, const TileSet &tileset) {
    writer.beginObject();
    writer.key("name"); writer.value(tileset.name);
    writer.key("image"); writer.value(tileset.image);
    writer.key("firstgid"); writer.value(tileset.firstGid);
    writer.key("tilewidth"); writer.value(tileset.tileW);
    writer.key("tileheight"); writer.value(tileset.tileH);
    writer.key("columns"); writer.value(tileset.columns);
    writer.key("tilecount"); writer.value(tileset.tileCount);
    writer.key("margin"); writer.value(tileset.margin);
    writer.key("spacing"); writer.value(tileset.spacing);
    if (!tileset.wangsets.empty()) {
        writer.key("wangsets");
        writer.raw(tileset.wangsets);
    }
    if (!tileset.animations.empty()) {
        writer.key("tiles");
        writer.beginArray();
        for (const auto &[id, animation] : tileset.animations) {
            writer.beginObject();
            writer.key("id"); writer.value(id);
            writer.key("animation");
            writer.beginArray();
            for (const auto &frame : animation.frames) {
                writer.beginObject();
                writer.key("tileid"); writer.value(frame.tileId);
                writer.key("duration"); writer.value(frame.durationMs);
                writer.endObject();
            }
            writer.endArray();
            writer.endObject();
        }
        writer.endArray();
    }
    writer.endObject();
}

TileSet ReadTileSet(const Json &json) {
    TileSet tileset;
    // Animationen stehen in "tiles": [{ "id": ..., "animation": [...] }].
    if (const Json *tiles = json.find("tiles"); tiles != nullptr && tiles->isArray()) {
        for (int i = 0; i < tiles->size(); ++i) {
            const Json &tile = tiles->at(i);
            const int id = tile.find("id") != nullptr ? tile.find("id")->intValue() : 0;
            const Json *animationJson = tile.find("animation");
            if (animationJson == nullptr || !animationJson->isArray()) continue;
            TileAnimation animation;
            for (int f = 0; f < animationJson->size(); ++f) {
                const Json &frame = animationJson->at(f);
                TileAnimationFrame value;
                value.tileId = frame.find("tileid") != nullptr ? frame.find("tileid")->intValue() : 0;
                value.durationMs =
                    frame.find("duration") != nullptr ? frame.find("duration")->intValue(100) : 100;
                animation.frames.push_back(value);
            }
            if (!animation.frames.empty()) tileset.animations[id] = std::move(animation);
        }
    }
    tileset.name = JsonString(json.find("name"), "tileset");
    tileset.image = JsonString(json.find("image"));
    tileset.firstGid = JsonInt(json.find("firstgid"), 1);
    tileset.tileW = JsonInt(json.find("tilewidth"), 32);
    tileset.tileH = JsonInt(json.find("tileheight"), 32);
    tileset.columns = JsonInt(json.find("columns"), 0);
    tileset.tileCount = JsonInt(json.find("tilecount"), 0);
    tileset.margin = JsonInt(json.find("margin"), 0);
    tileset.spacing = JsonInt(json.find("spacing"), 0);
    if (const Json *wangsets = json.find("wangsets"); wangsets != nullptr) {
        if (wangsets->isString()) tileset.wangsets = wangsets->string();
        else if (wangsets->isArray()) tileset.wangsets = JsonToText(*wangsets);
    }
    return tileset;
}

void WriteEntityAttrs(JsonWriter &writer, const std::map<std::string, std::string> &attrs) {
    writer.key("attributes");
    writer.beginObject();
    for (const auto &[name, value] : attrs) {
        writer.key(name);
        writer.value(value);
    }
    writer.endObject();
}

std::map<std::string, std::string> ReadEntityAttrs(const Json &json) {
    std::map<std::string, std::string> attrs;
    if (const Json *values = json.find("attributes"); values != nullptr && values->isObject()) {
        for (int i = 0; i < values->size(); ++i) {
            const std::string &name = values->keyAt(i);
            const Json *value = values->find(name);
            attrs[name] = value != nullptr ? value->string() : std::string{};
        }
    }
    return attrs;
}

} // namespace

// ── Pfade ────────────────────────────────────────────────────────────────────

std::string PathToUtf8(const fs::path &path) {
#if defined(_WIN32)
    const std::u8string utf8 = path.generic_u8string();
    return std::string(reinterpret_cast<const char *>(utf8.data()), utf8.size());
#else
    return path.generic_string();
#endif
}

fs::path PathFromUtf8(std::string_view text) {
#if defined(_WIN32)
    const std::u8string utf8(reinterpret_cast<const char8_t *>(text.data()), text.size());
    return fs::path(utf8);
#else
    return fs::path(std::string(text));
#endif
}

std::string FileStem(const fs::path &path) {
    return PathToUtf8(path.stem());
}

fs::path ResolveAsset(const Project &project, const std::string &path) {
    const fs::path value = PathFromUtf8(path);
    if (value.is_absolute()) return value;
    return project.root / value;
}

sgl::Color ParseColor(std::string_view text, const sgl::Color &fallback) {
    if (text.empty() || text.front() != '#') return fallback;
    std::string hex(text.substr(1));
    auto channel = [&](std::size_t offset) -> u8 {
        if (offset + 2 > hex.size()) return 255;
        return static_cast<u8>(std::strtoul(hex.substr(offset, 2).c_str(), nullptr, 16));
    };
    if (hex.size() == 6) {
        return sgl::Color{channel(0), channel(2), channel(4), 255};
    }
    if (hex.size() == 8) {
        return sgl::Color{channel(0), channel(2), channel(4), channel(6)};
    }
    return fallback;
}

// ── Modellhelfer ────────────────────────────────────────────────────────────

Project CreateProject(std::string name, fs::path root) {
    Project project;
    project.name = std::move(name);
    project.root = std::move(root);
    AddDefaultEntityDefs(project);
    return project;
}

Level CreateLevel(int width, int height, int tileW, int tileH, std::string name) {
    Level level;
    level.name = std::move(name);
    level.width = std::max(width, 1);
    level.height = std::max(height, 1);
    level.tileW = std::max(tileW, 1);
    level.tileH = std::max(tileH, 1);
    AddDefaultLayers(level);
    return level;
}

void AddDefaultLayers(Level &level) {
    TileLayer tiles;
    tiles.name = "Kacheln";
    tiles.tiles.assign(static_cast<std::size_t>(level.width) * level.height, 0);
    level.tileLayers.push_back(std::move(tiles));

    ObjectLayer objects;
    objects.name = "Objekte";
    level.objectLayers.push_back(std::move(objects));
}

void AddDefaultEntityDefs(Project &project) {
    auto make = [](std::string name, sgl::Color color) {
        EntityDef def;
        def.name = std::move(name);
        def.color = color;
        def.attributes.push_back(AttributeDef{"Name", "string", def.name, {}});
        return def;
    };
    project.entityDefs.push_back(make("Startpunkt", sgl::Color{80, 200, 120, 255}));
    project.entityDefs.push_back(make("Gegner", sgl::Color{220, 90, 90, 255}));
    project.entityDefs.push_back(make("Beute", sgl::Color{230, 190, 70, 255}));
    project.entityDefs.push_back(make("Trigger", sgl::Color{140, 120, 230, 255}));
}

EntityDef *FindEntityDef(Project &project, std::string_view name) {
    for (auto &def : project.entityDefs) {
        if (def.name == name) return &def;
    }
    return nullptr;
}

const EntityDef *FindEntityDef(const Project &project, std::string_view name) {
    for (const auto &def : project.entityDefs) {
        if (def.name == name) return &def;
    }
    return nullptr;
}

std::string DefaultAttribute(const EntityDef &def, std::string_view name) {
    for (const auto &attribute : def.attributes) {
        if (attribute.name == name) return attribute.defaultValue;
    }
    return {};
}

int NewEntityId(Level &level) {
    return level.nextEntityId++;
}

u32 AnimatedTile(const Level &level, u32 gid, f32 seconds) {
    if (gid == 0) return 0;
    const TileSet *tileset = FindTileSetForGid(level, gid);
    if (tileset == nullptr) return gid;
    const int local = static_cast<int>(gid) - tileset->firstGid;
    const auto found = tileset->animations.find(local);
    if (found == tileset->animations.end() || found->second.frames.empty()) return gid;
    int total = 0;
    for (const auto &frame : found->second.frames) total += std::max(frame.durationMs, 1);
    int time = static_cast<int>(std::fmod(std::max(seconds, 0.0f) * 1000.0f, static_cast<f32>(total)));
    for (const auto &frame : found->second.frames) {
        if (time < frame.durationMs) {
            return static_cast<u32>(tileset->firstGid + frame.tileId);
        }
        time -= std::max(frame.durationMs, 1);
    }
    return gid;
}

TileSet *FindTileSetForGid(Level &level, u32 gid) {
    TileSet *best = nullptr;
    for (auto &tileset : level.tilesets) {
        if (tileset.firstGid <= static_cast<int>(gid) &&
            (best == nullptr || tileset.firstGid > best->firstGid)) {
            best = &tileset;
        }
    }
    return best;
}

const TileSet *FindTileSetForGid(const Level &level, u32 gid) {
    return FindTileSetForGid(const_cast<Level &>(level), gid);
}

TileSet *FindTileSet(Level &level, std::string_view name) {
    for (auto &tileset : level.tilesets) {
        if (tileset.name == name) return &tileset;
    }
    return nullptr;
}

void EnsureEntityDef(Project &project, const std::string &name) {
    if (FindEntityDef(project, name) != nullptr) return;
    EntityDef def;
    def.name = name.empty() ? "Objekt" : name;
    def.color = sgl::Color{120, 150, 190, 255};
    def.attributes.push_back(AttributeDef{"Name", "string", def.name, {}});
    project.entityDefs.push_back(std::move(def));
}

// ── Projekt speichern/laden ─────────────────────────────────────────────────

bool SaveProject(const Project &project, const fs::path &file, std::string &error) {
    JsonWriter writer;
    writer.beginObject();
    writer.key("version"); writer.value(project.version);
    writer.key("name"); writer.value(project.name);
    writer.key("levels");
    writer.beginArray();
    for (const auto &level : project.levels) writer.value(level);
    writer.endArray();
    writer.key("tilesets");
    writer.beginArray();
    for (const auto &tileset : project.tilesets) WriteTileSet(writer, tileset);
    writer.endArray();
    writer.key("entities");
    writer.beginArray();
    for (const auto &def : project.entityDefs) {
        writer.beginObject();
        writer.key("name"); writer.value(def.name);
        writer.key("color"); writer.color(def.color);
        writer.key("attributes");
        writer.beginArray();
        for (const auto &attribute : def.attributes) {
            writer.beginObject();
            writer.key("name"); writer.value(attribute.name);
            writer.key("type"); writer.value(attribute.type);
            writer.key("default"); writer.value(attribute.defaultValue);
            writer.key("values");
            writer.beginArray();
            for (const auto &value : attribute.values) writer.value(value);
            writer.endArray();
            writer.endObject();
        }
        writer.endArray();
        writer.endObject();
    }
    writer.endArray();
    writer.endObject();
    return WriteTextFile(file, writer.take(), error);
}

bool LoadProject(const fs::path &file, Project &project, std::string &error) {
    const std::string text = ReadTextFile(file);
    if (text.empty()) {
        error = "Projektdatei ist leer oder nicht lesbar: " + PathToUtf8(file);
        return false;
    }
    const Json json = Json::Parse(text);
    if (!json.isValid()) {
        error = "Projektdatei ist kein gültiges JSON: " + json.error();
        return false;
    }
    project = {};
    project.root = file.parent_path();
    project.name = JsonString(json.find("name"), "Projekt");
    project.version = JsonString(json.find("version"), "1");
    if (const Json *levels = json.find("levels"); levels != nullptr && levels->isArray()) {
        for (int i = 0; i < levels->size(); ++i) {
            project.levels.push_back(levels->at(i).string());
        }
    }
    if (const Json *tilesets = json.find("tilesets"); tilesets != nullptr && tilesets->isArray()) {
        for (int i = 0; i < tilesets->size(); ++i) {
            project.tilesets.push_back(ReadTileSet(tilesets->at(i)));
        }
    }
    if (const Json *defs = json.find("entities"); defs != nullptr && defs->isArray()) {
        for (int i = 0; i < defs->size(); ++i) {
            const Json &item = defs->at(i);
            EntityDef def;
            def.name = JsonString(item.find("name"), "Objekt");
            def.color = ParseColor(JsonString(item.find("color"), "#7896beff"),
                                   sgl::Color{120, 150, 190, 255});
            if (const Json *attrs = item.find("attributes"); attrs != nullptr && attrs->isArray()) {
                for (int a = 0; a < attrs->size(); ++a) {
                    const Json &attributeJson = attrs->at(a);
                    AttributeDef attribute;
                    attribute.name = JsonString(attributeJson.find("name"), "Attribut");
                    attribute.type = JsonString(attributeJson.find("type"), "string");
                    attribute.defaultValue = JsonString(attributeJson.find("default"));
                    if (const Json *values = attributeJson.find("values");
                        values != nullptr && values->isArray()) {
                        for (int v = 0; v < values->size(); ++v) {
                            attribute.values.push_back(values->at(v).string());
                        }
                    }
                    def.attributes.push_back(std::move(attribute));
                }
            }
            project.entityDefs.push_back(std::move(def));
        }
    }
    if (project.entityDefs.empty()) AddDefaultEntityDefs(project);
    return true;
}

// ── Level speichern/laden ───────────────────────────────────────────────────

bool SaveLevel(const Level &level, const fs::path &file, std::string &error) {
    JsonWriter writer;
    writer.beginObject();
    writer.key("version"); writer.value("1");
    writer.key("name"); writer.value(level.name);
    writer.key("width"); writer.value(level.width);
    writer.key("height"); writer.value(level.height);
    writer.key("tilewidth"); writer.value(level.tileW);
    writer.key("tileheight"); writer.value(level.tileH);
    writer.key("nextEntityId"); writer.value(level.nextEntityId);
    writer.key("tilesets");
    writer.beginArray();
    for (const auto &tileset : level.tilesets) WriteTileSet(writer, tileset);
    writer.endArray();
    writer.key("tileLayers");
    writer.beginArray();
    for (const auto &layer : level.tileLayers) {
        writer.beginObject();
        writer.key("name"); writer.value(layer.name);
        writer.key("visible"); writer.value(layer.visible);
        writer.key("locked"); writer.value(layer.locked);
        writer.key("opacity"); writer.value(static_cast<double>(layer.opacity));
        writer.key("data");
        writer.beginArray();
        for (const u32 tile : layer.tiles) writer.value(static_cast<int>(tile));
        writer.endArray();
        writer.endObject();
    }
    writer.endArray();
    writer.key("objectLayers");
    writer.beginArray();
    for (const auto &layer : level.objectLayers) {
        writer.beginObject();
        writer.key("name"); writer.value(layer.name);
        writer.key("visible"); writer.value(layer.visible);
        writer.key("locked"); writer.value(layer.locked);
        writer.key("entities");
        writer.beginArray();
        for (const auto &entity : layer.entities) {
            writer.beginObject();
            writer.key("id"); writer.value(entity.id);
            writer.key("def"); writer.value(entity.def);
            writer.key("x"); writer.value(static_cast<double>(entity.x));
            writer.key("y"); writer.value(static_cast<double>(entity.y));
            writer.key("w"); writer.value(static_cast<double>(entity.w));
            writer.key("h"); writer.value(static_cast<double>(entity.h));
            writer.key("rotation"); writer.value(static_cast<double>(entity.rotation));
            if (!entity.templatePath.empty()) {
                writer.key("template"); writer.value(entity.templatePath);
            }
            WriteEntityAttrs(writer, entity.attrs);
            writer.endObject();
        }
        writer.endArray();
        writer.endObject();
    }
    writer.endArray();
    writer.endObject();
    return WriteTextFile(file, writer.take(), error);
}

bool LoadLevel(const fs::path &file, Level &level, std::string &error) {
    const std::string text = ReadTextFile(file);
    if (text.empty()) {
        error = "Leveldatei ist leer oder nicht lesbar: " + PathToUtf8(file);
        return false;
    }
    const Json json = Json::Parse(text);
    if (!json.isValid()) {
        error = "Leveldatei ist kein gültiges JSON: " + json.error();
        return false;
    }
    level = {};
    level.name = JsonString(json.find("name"), FileStem(file));
    level.width = std::max(JsonInt(json.find("width"), 40), 1);
    level.height = std::max(JsonInt(json.find("height"), 25), 1);
    level.tileW = std::max(JsonInt(json.find("tilewidth"), 32), 1);
    level.tileH = std::max(JsonInt(json.find("tileheight"), 32), 1);
    level.nextEntityId = std::max(JsonInt(json.find("nextEntityId"), 1), 1);
    if (const Json *tilesets = json.find("tilesets"); tilesets != nullptr && tilesets->isArray()) {
        for (int i = 0; i < tilesets->size(); ++i) {
            level.tilesets.push_back(ReadTileSet(tilesets->at(i)));
        }
    }
    if (const Json *layers = json.find("tileLayers"); layers != nullptr && layers->isArray()) {
        for (int i = 0; i < layers->size(); ++i) {
            const Json &item = layers->at(i);
            TileLayer layer;
            layer.name = JsonString(item.find("name"), "Kacheln");
            layer.visible = JsonBool(item.find("visible"), true);
            layer.locked = JsonBool(item.find("locked"), false);
            layer.opacity = JsonFloat(item.find("opacity"), 1.0f);
            layer.tiles.assign(static_cast<std::size_t>(level.width) * level.height, 0);
            if (const Json *data = item.find("data"); data != nullptr && data->isArray()) {
                const int count = std::min(data->size(), static_cast<int>(layer.tiles.size()));
                for (int t = 0; t < count; ++t) {
                    layer.tiles[static_cast<std::size_t>(t)] =
                        static_cast<u32>(data->at(t).intValue());
                }
            }
            level.tileLayers.push_back(std::move(layer));
        }
    }
    if (const Json *layers = json.find("objectLayers"); layers != nullptr && layers->isArray()) {
        for (int i = 0; i < layers->size(); ++i) {
            const Json &item = layers->at(i);
            ObjectLayer layer;
            layer.name = JsonString(item.find("name"), "Objekte");
            layer.visible = JsonBool(item.find("visible"), true);
            layer.locked = JsonBool(item.find("locked"), false);
            if (const Json *entities = item.find("entities"); entities != nullptr && entities->isArray()) {
                for (int e = 0; e < entities->size(); ++e) {
                    const Json &entityJson = entities->at(e);
                    Entity entity;
                    entity.id = JsonInt(entityJson.find("id"), 0);
                    entity.def = JsonString(entityJson.find("def"), "Objekt");
                    entity.x = JsonFloat(entityJson.find("x"), 0.0f);
                    entity.y = JsonFloat(entityJson.find("y"), 0.0f);
                    entity.w = JsonFloat(entityJson.find("w"), 32.0f);
                    entity.h = JsonFloat(entityJson.find("h"), 32.0f);
                    entity.rotation = JsonFloat(entityJson.find("rotation"), 0.0f);
                    entity.templatePath = JsonString(entityJson.find("template"));
                    entity.attrs = ReadEntityAttrs(entityJson);
                    if (entity.id >= level.nextEntityId) level.nextEntityId = entity.id + 1;
                    layer.entities.push_back(std::move(entity));
                }
            }
            level.objectLayers.push_back(std::move(layer));
        }
    }
    if (level.tileLayers.empty() && level.objectLayers.empty()) AddDefaultLayers(level);
    return true;
}

// ── Undo/Redo ───────────────────────────────────────────────────────────────

void LevelHistory::reset(const Level &level) {
    m_states.clear();
    m_states.push_back(level);
    m_index = 0;
}

void LevelHistory::commit(const Level &level) {
    if (m_states.empty()) {
        reset(level);
        return;
    }
    if (m_index + 1 < m_states.size()) {
        m_states.erase(m_states.begin() + static_cast<std::ptrdiff_t>(m_index) + 1, m_states.end());
    }
    m_states.push_back(level);
    if (m_states.size() > kLimit) {
        m_states.erase(m_states.begin());
    }
    m_index = m_states.size() - 1;
}

bool LevelHistory::undo(Level &level) {
    if (m_index == 0) return false;
    --m_index;
    level = m_states[m_index];
    return true;
}

bool LevelHistory::redo(Level &level) {
    if (m_index + 1 >= m_states.size()) return false;
    ++m_index;
    level = m_states[m_index];
    return true;
}

bool LevelHistory::canUndo() const { return m_index > 0; }
bool LevelHistory::canRedo() const { return m_index + 1 < m_states.size(); }
std::size_t LevelHistory::undoCount() const { return m_index; }
std::size_t LevelHistory::redoCount() const { return m_states.empty() ? 0 : m_states.size() - m_index - 1; }

} // namespace editor
