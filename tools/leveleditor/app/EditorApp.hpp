#pragma once

#include "doc/Document.hpp"
#include "ui/Ui.hpp"

#include <sgl/Window.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace editor {

class EditorApp {
public:
    struct Options {
        std::string frame;
        std::string project;
        std::string level;
        int tab = -1;
        bool demo = false;
    };

    EditorApp() = default;
    ~EditorApp();

    int run(const Options &options);

public:
    enum class Tool { Select, Brush, Rect, Line, Fill, Eraser, Picker, Entity, Pan };

private:
    struct Document {
        Level level;
        LevelHistory history;
        fs::path path;
        std::string title;
        bool dirty = false;
    };

    enum class DialogKind {
        None,
        NewProject,
        OpenProject,
        SaveProjectAs,
        ImportTiled,
        ExportTiled,
        ImportTileset,
    };

    struct Dialog {
        DialogKind kind = DialogKind::None;
        fs::path directory;
        std::string name;
        std::string heading;
        std::string extension;
        std::vector<std::string> entries;
        int selected = -1;
    };

    struct TileTexture {
        ui::Texture texture;
        int columns = 0;
        int tileW = 32;
        int tileH = 32;
        int margin = 0;
        int spacing = 0;
        int tileCount = 0;
    };

    // Aufbau
    bool initWindow(std::string &error, bool visible);
    void installCallbacks();
    void frame();
    bool saveScreenshot(const fs::path &file);
    void handleShortcuts();
    void updateAutosave(f32 dt);

    // Zeichnen
    void drawMenuBar();
    void drawMenuPopup();
    void drawToolbar(const ui::Rect &rect);
    void drawProjectPanel(const ui::Rect &rect);
    void drawPalettePanel(const ui::Rect &rect);
    void drawEntityPanel(const ui::Rect &rect);
    void drawViewport(const ui::Rect &rect);
    void drawInspector(const ui::Rect &rect);
    void drawBottomPanel(const ui::Rect &rect);
    void drawStatusBar(const ui::Rect &rect);
    void drawDialog();
    void drawLevelTabs(const ui::Rect &rect);

    // Projekt und Dateien
    void newProject(const fs::path &root, const std::string &name);
    bool openProject(const fs::path &file);
    bool saveProject();
    bool saveProjectAs(const fs::path &file);
    bool saveActiveLevel();
    void loadAllLevels();
    void newLevel();
    void closeDocument(int index);
    bool openLevelFile(const fs::path &file, bool asImport);
    bool importTiled(const fs::path &file);
    bool exportTiled(const fs::path &file);
    void importTileset(const fs::path &file);
    void ensureTileTexture(const TileSet &tileset);

    // Dialoge
    void openDialog(DialogKind kind, std::string heading, std::string extension,
                    const fs::path &directory, std::string name);
    void refreshDialogEntries();
    void confirmDialog();
    void cancelDialog();
    void openProjectDialog();
    void saveProjectAsDialog();
    void importDialog();
    void exportDialog();
    void importTilesetDialog();

    // Dokument/Modell
    [[nodiscard]] Document *activeDocument();
    [[nodiscard]] const Document *activeDocument() const;
    [[nodiscard]] Level *activeLevel();
    [[nodiscard]] TileLayer *activeTileLayer();
    [[nodiscard]] ObjectLayer *activeObjectLayer();
    void markDirty();
    void commitHistory();
    void undo();
    void redo();
    void addTileLayer();
    void addObjectLayer();
    void duplicateLayer();
    void deleteLayer();
    void moveLayer(int direction);

    // Werkzeuge
    void applyToolAt(const ivec2 &cell, bool erase);
    void beginStroke(const ivec2 &cell, bool erase);
    void updateStroke(const ivec2 &cell, bool erase);
    void endStroke();
    void floodFill(const ivec2 &cell, u32 value);
    void paintRect(const ivec2 &a, const ivec2 &b, u32 value);
    void paintLine(const ivec2 &a, const ivec2 &b, u32 value);
    [[nodiscard]] ivec2 cellFromScreen(const vec2 &point) const;
    [[nodiscard]] vec2 screenFromWorld(const vec2 &world) const;
    [[nodiscard]] vec2 worldFromScreen(const vec2 &point) const;
    [[nodiscard]] bool viewportHovered() const;
    [[nodiscard]] Entity *findEntityAt(const vec2 &world);
    void selectEntityAt(const vec2 &world);

    void log(std::string message);

    // Fenster und UI
    std::unique_ptr<sgl::Window> m_window;
    ui::Renderer2d m_renderer;
    ui::Ui m_ui;
    ui::Theme m_theme;
    ui::InputState m_input;
    f32 m_contentScale = 1.0f;
    ivec2 m_windowSize{1440, 900};

    // Projekt
    Project m_project;
    std::vector<Document> m_documents;
    int m_active = -1;
    bool m_projectDirty = false;

    // Auswahl
    int m_activeTileLayer = 0;
    int m_activeObjectLayer = 0;
    int m_activeTileset = 0;
    u32 m_selectedTile = 0;
    std::string m_selectedEntityDef;
    int m_selectedEntityId = -1;
    bool m_layerSelectionIsObject = false;

    // Werkzeug und Ansicht
    Tool m_tool = Tool::Brush;
    bool m_showGrid = true;
    bool m_showMinimap = true;
    f32 m_cameraX = 0.0f;
    f32 m_cameraY = 0.0f;
    f32 m_zoom = 1.0f;
    ui::Rect m_viewport{};
    bool m_panning = false;
    vec2 m_panStart{};
    vec2 m_panCameraStart{};
    bool m_stroking = false;
    bool m_strokeErase = false;
    ivec2 m_strokeStart{};
    ivec2 m_strokeEnd{};
    bool m_movingEntity = false;
    vec2 m_entityDragStart{};
    vec2 m_entityOriginal{};
    int m_dragEntityId = -1;

    // Layout
    f32 m_leftWidth = 250.0f;
    f32 m_rightWidth = 300.0f;
    f32 m_bottomHeight = 190.0f;
    int m_dragSplitter = 0; // 1 links, 2 rechts, 3 unten
    int m_openMenu = -1;
    int m_bottomTab = 0; // 0 Ebenen, 1 Konsole, 2 Assets
    int m_leftTab = 0;   // 0 Projekt, 1 Kacheln, 2 Entities

    // Dialoge
    Dialog m_dialog;
    std::string m_dialogPath;
    std::string m_status;

    // Texturen und Log
    std::unordered_map<std::string, TileTexture> m_textures;
    std::unordered_map<std::string, bool> m_textureFailed;
    std::vector<std::string> m_log;
    f32 m_fps = 60.0f;
    f32 m_frameAccumulator = 0.0f;
    int m_frameCount = 0;
    f32 m_autosaveTimer = 0.0f;
    f64 m_lastTime = 0.0;
    f32 m_doubleClickTime[3]{};
    vec2 m_lastClickPos[3]{};
};

} // namespace editor
