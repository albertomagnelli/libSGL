#pragma once

// Dokumentmodell des Leveleditors: Projekt, Level, Kachel-/Objekt-Layer,
// Entities mit Attributen sowie Undo/Redo über Level-Snapshots.

#include <sgl/Color.hpp>
#include <sgl/Types.hpp>

#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

namespace fs = std::filesystem;

struct TileSet {
    std::string name = "tileset";
    std::string image;              // relativ zum Projekt oder absolut
    int firstGid = 1;
    int tileW = 32;
    int tileH = 32;
    int columns = 0;
    int tileCount = 0;
    int margin = 0;
    int spacing = 0;
};

struct TileLayer {
    std::string name = "Ebene";
    bool visible = true;
    bool locked = false;
    f32 opacity = 1.0f;
    std::vector<u32> tiles;
};

struct Entity {
    int id = 0;
    std::string def = "Objekt";
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 w = 32.0f;
    f32 h = 32.0f;
    f32 rotation = 0.0f;
    std::map<std::string, std::string> attrs;
};

struct ObjectLayer {
    std::string name = "Objekte";
    bool visible = true;
    bool locked = false;
    std::vector<Entity> entities;
};

struct Level {
    std::string name = "level";
    int width = 40;
    int height = 25;
    int tileW = 32;
    int tileH = 32;
    int nextEntityId = 1;
    std::vector<TileSet> tilesets;
    std::vector<TileLayer> tileLayers;
    std::vector<ObjectLayer> objectLayers;

    [[nodiscard]] u32 *tilePtr(TileLayer &layer, int x, int y) {
        if (x < 0 || y < 0 || x >= width || y >= height) return nullptr;
        return &layer.tiles[static_cast<std::size_t>(y) * width + x];
    }
    [[nodiscard]] u32 tileAt(const TileLayer &layer, int x, int y) const {
        if (x < 0 || y < 0 || x >= width || y >= height) return 0;
        return layer.tiles[static_cast<std::size_t>(y) * width + x];
    }
};

struct AttributeDef {
    std::string name = "Attribut";
    std::string type = "string"; // bool, int, float, string, color, enum, asset
    std::string defaultValue;
    std::vector<std::string> values;
};

struct EntityDef {
    std::string name = "Objekt";
    sgl::Color color{80, 140, 220, 255};
    std::vector<AttributeDef> attributes;
};

struct Project {
    std::string name = "Neues Projekt";
    fs::path root;
    std::vector<std::string> levels;      // relative Pfade, z. B. levels/level01.tmj
    std::vector<TileSet> tilesets;
    std::vector<EntityDef> entityDefs;
    std::string version = "1";
};

// ── Pfad- und Namenshelfer ───────────────────────────────────────────────────

std::string PathToUtf8(const fs::path &path);
fs::path PathFromUtf8(std::string_view text);
std::string FileStem(const fs::path &path);
fs::path ResolveAsset(const Project &project, const std::string &path);
sgl::Color ParseColor(std::string_view text, const sgl::Color &fallback = {});

// ── Modellhelfer ────────────────────────────────────────────────────────────

Project CreateProject(std::string name, fs::path root);
Level CreateLevel(int width, int height, int tileW, int tileH, std::string name);
void AddDefaultLayers(Level &level);
void AddDefaultEntityDefs(Project &project);
EntityDef *FindEntityDef(Project &project, std::string_view name);
const EntityDef *FindEntityDef(const Project &project, std::string_view name);
std::string DefaultAttribute(const EntityDef &def, std::string_view name);
int NewEntityId(Level &level);
TileSet *FindTileSetForGid(Level &level, u32 gid);
const TileSet *FindTileSetForGid(const Level &level, u32 gid);
TileSet *FindTileSet(Level &level, std::string_view name);
void EnsureEntityDef(Project &project, const std::string &name);

// ── Persistenz ──────────────────────────────────────────────────────────────

bool SaveProject(const Project &project, const fs::path &file, std::string &error);
bool LoadProject(const fs::path &file, Project &project, std::string &error);
bool SaveLevel(const Level &level, const fs::path &file, std::string &error);
bool LoadLevel(const fs::path &file, Level &level, std::string &error);

// Tiled-Import/-Export (erkennt .tmj/.tmx an der Endung).
bool ImportTiled(const fs::path &file, Level &level, Project &project, std::string &error);
bool ExportTiled(const fs::path &file, const Level &level, const Project &project,
                 std::string &error);

// ── Undo/Redo ───────────────────────────────────────────────────────────────

class LevelHistory {
public:
    void reset(const Level &level);
    void commit(const Level &level);
    bool undo(Level &level);
    bool redo(Level &level);
    [[nodiscard]] bool canUndo() const;
    [[nodiscard]] bool canRedo() const;
    [[nodiscard]] std::size_t undoCount() const;
    [[nodiscard]] std::size_t redoCount() const;

private:
    static constexpr std::size_t kLimit = 80;
    std::vector<Level> m_states;
    std::size_t m_index = 0;
};

} // namespace editor
