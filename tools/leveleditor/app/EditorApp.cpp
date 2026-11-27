#include "EditorApp.hpp"

#include <sgl/Font.hpp>
#include <sgl/GL.hpp>
#include <sgl/Image.hpp>
#include <sgl/Init.hpp>
#include <sgl/Events.hpp>
#include <sgl/Log.hpp>
#include <sgl/Time.hpp>

#include "common/Json.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <system_error>

namespace editor {
namespace {

constexpr f32 kMenuHeight = 26.0f;
constexpr f32 kToolbarHeight = 38.0f;
constexpr f32 kStatusHeight = 24.0f;
constexpr f32 kTabsHeight = 26.0f;

const char* ToolName(EditorApp::Tool tool) {
    switch (tool) {
    case EditorApp::Tool::Select: return "Auswahl";
    case EditorApp::Tool::Brush: return "Stift";
    case EditorApp::Tool::Rect: return "Rechteck";
    case EditorApp::Tool::Line: return "Linie";
    case EditorApp::Tool::Fill: return "Füllen";
    case EditorApp::Tool::Eraser: return "Radierer";
    case EditorApp::Tool::Picker: return "Pipette";
    case EditorApp::Tool::Entity: return "Entity";
    case EditorApp::Tool::Pan: return "Pan";
    }
    return "?";
}

} // namespace

EditorApp::~EditorApp() {
    m_renderer.shutdown();
    if (m_window) {
        m_window.reset();
    }
    sgl::Terminate();
}

bool EditorApp::initWindow(std::string &error, bool visible) {
    sgl::SetErrorCallback([](const sgl::Error &value) {
        SLOGE("sgl: %s", value.message.c_str());
    });
    if (auto init = sgl::Init(); !init) {
        error = init.error().message;
        return false;
    }

    sgl::WindowHints hints;
    hints.width = 1440;
    hints.height = 900;
    hints.title = "sgl Leveleditor";
    hints.visible = visible;
    hints.contextVersionMajor = 3;
    hints.contextVersionMinor = 3;
    hints.contextProfile = sgl::ContextProfile::Core;
    hints.contextForwardCompat = true;
    hints.samples = 0;
    auto result = sgl::Window::Create(hints);
    if (!result) {
        error = result.error().message;
        return false;
    }
    m_window = std::move(*result);
    m_window->makeContextCurrent();
    m_window->setSwapInterval(1);
    const gl::LoadReport report = gl::Load();
    if (!report.complete()) {
        SLOGW("OpenGL: %zu/%zu Funktionen geladen (3.3 reicht für den Editor).",
               report.loaded, report.total);
    }
    m_renderer.init(error);
    if (!error.empty()) return false;

    m_theme = ui::Theme{};
    installCallbacks();
    m_lastTime = sgl::GetTime();
    log("Willkommen im sgl-Leveleditor. Datei → Neues Projekt…");
    return true;
}

void EditorApp::installCallbacks() {
    m_window->setCloseCallback([this] { m_window->setCloseRequested(true); });
    m_window->setContentScaleCallback([this](vec2 scale) {
        m_contentScale = scale.x > 0.0f ? scale.x : 1.0f;
    });
    m_window->setKeyCallback([this](sgl::Key key, i32, sgl::KeyAction action,
                                    sgl::Modifiers mods) {
        m_input.shift = sgl::HasModifier(mods, sgl::Modifiers::Shift);
        m_input.control = sgl::HasModifier(mods, sgl::Modifiers::Control);
        m_input.alt = sgl::HasModifier(mods, sgl::Modifiers::Alt);
        m_input.super = sgl::HasModifier(mods, sgl::Modifiers::Super);
        if (action != sgl::KeyAction::Release) {
            m_input.keyPresses.push_back(key);
        }
    });
    m_window->setCharCallback([this](u32 codepoint) { m_input.chars.push_back(codepoint); });
    m_window->setMouseButtonCallback([this](sgl::MouseButton button, sgl::KeyAction action,
                                            sgl::Modifiers) {
        const int index = std::clamp(static_cast<int>(button), 0, 2);
        if (action == sgl::KeyAction::Press) {
            m_input.down[index] = true;
            m_input.pressed[index] = true;
            const f64 now = sgl::GetTime();
            const bool near = std::abs(m_input.mouse.x - m_lastClickPos[index].x) < 6.0f &&
                              std::abs(m_input.mouse.y - m_lastClickPos[index].y) < 6.0f;
            if (near && (now - m_doubleClickTime[index]) < 0.35) {
                m_input.doubleClick[index] = true;
            }
            m_doubleClickTime[index] = static_cast<f32>(now);
            m_lastClickPos[index] = m_input.mouse;
        } else if (action == sgl::KeyAction::Release) {
            m_input.down[index] = false;
            m_input.released[index] = true;
        }
    });
    m_window->setCursorPosCallback([this](vec2 position) {
        m_input.mouseDelta = position - m_input.mouse;
        m_input.mouse = position;
    });
    m_window->setScrollCallback([this](vec2 offset) { m_input.wheel += offset.y; });
    m_window->setDropCallback([this](std::span<const std::string> paths) {
        for (const auto &path : paths) {
            const fs::path file = PathFromUtf8(path);
            const std::string extension = PathToUtf8(file.extension());
            if (extension == ".tmj" || extension == ".tmx") importTiled(file);
            else if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" ||
                     extension == ".bmp" || extension == ".tga") importTileset(file);
        }
    });
    m_window->setFramebufferSizeCallback([this](ivec2) {});
}

int EditorApp::run(const Options &options) {
    std::string error;
    if (!initWindow(error, options.frame.empty())) {
        SLOGE("Editor konnte nicht starten: %s", error.c_str());
        return 1;
    }
    if (!options.project.empty()) {
        fs::path projectFile = PathFromUtf8(options.project);
        if (fs::is_directory(projectFile)) projectFile /= "project.sglproj";
        openProject(projectFile);
    } else if (options.demo) {
        newProject(fs::temp_directory_path() / "sgl-editor-demo", "Demo");
    }
    if (options.play) togglePlaytest();
    if (options.tab >= 0 && !m_leftPanels.empty())
        m_leftActive = std::clamp(options.tab, 0, static_cast<int>(m_leftPanels.size()) - 1);
    if (!options.level.empty()) {
        for (std::size_t i = 0; i < m_documents.size(); ++i) {
            if (m_documents[i].title == options.level) {
                m_active = static_cast<int>(i);
                break;
            }
        }
    }

    if (!options.frame.empty()) {
        frame();
        const bool ok = saveScreenshot(PathFromUtf8(options.frame));
        return ok ? 0 : 1;
    }

    while (!m_window->isCloseRequested()) {
        m_input.newFrame();
        sgl::PollEvents();
        const f64 now = sgl::GetTime();
        const f32 dt = static_cast<f32>(std::max(now - m_lastTime, 0.0));
        m_deltaTime = dt;
        m_lastTime = now;
        m_frameAccumulator += dt;
        ++m_frameCount;
        if (m_frameAccumulator >= 0.5f) {
            m_fps = static_cast<f32>(m_frameCount) / m_frameAccumulator;
            m_frameAccumulator = 0.0f;
            m_frameCount = 0;
        }
        updateAutosave(dt);

        frame();
        m_window->swapBuffers();
    }
    if (m_projectDirty) saveProject();
    if (!m_project.root.empty()) saveLayout();
    return 0;
}

void EditorApp::frame() {
    const ivec2 size = m_window->getSize();
    const ivec2 framebuffer = m_window->getFramebufferSize();
    m_windowSize = size;
    const f32 scale = size.x > 0 ? static_cast<f32>(framebuffer.x) / static_cast<f32>(size.x)
                                 : 1.0f;
    m_contentScale = scale;
    m_renderer.beginFrame(framebuffer.x, framebuffer.y, scale, static_cast<f32>(size.x),
                          static_cast<f32>(size.y));
    m_renderer.clear(m_theme.window);
    if (m_playMode) {
        drawPlaytest();
        m_renderer.endFrame();
        return;
    }
    m_ui.beginFrame(m_input, m_renderer, m_theme,
                    ui::Rect{0.0f, 0.0f, static_cast<f32>(size.x), static_cast<f32>(size.y)});

    handleShortcuts();

    const f32 width = static_cast<f32>(size.x);
    const f32 height = static_cast<f32>(size.y);
    const f32 centerTop = kMenuHeight + kToolbarHeight;
    const f32 centerBottom = height - kStatusHeight;
    const f32 centerHeight = std::max(centerBottom - centerTop, 80.0f);

    m_leftWidth = std::clamp(m_leftWidth, 190.0f, width * 0.4f);
    m_rightWidth = std::clamp(m_rightWidth, 220.0f, width * 0.45f);
    m_bottomHeight = std::clamp(m_bottomHeight, 100.0f, centerHeight * 0.6f);

    const ui::Rect toolbarRect{0.0f, kMenuHeight, width, kToolbarHeight};
    const ui::Rect leftRect{0.0f, centerTop, m_leftWidth, centerHeight};
    const ui::Rect rightRect{width - m_rightWidth, centerTop, m_rightWidth, centerHeight};
    const ui::Rect bottomRect{m_leftWidth, centerBottom - m_bottomHeight,
                              std::max(width - m_leftWidth - m_rightWidth, 120.0f),
                              m_bottomHeight};
    const ui::Rect centerRect{m_leftWidth, centerTop,
                              std::max(width - m_leftWidth - m_rightWidth, 120.0f),
                              std::max(centerHeight - m_bottomHeight, 80.0f)};

    m_ui.setInputBlocked(false);
    drawMenuBar();

    const bool modal = m_dialog.kind != DialogKind::None;
    m_ui.setInputBlocked(modal || m_openMenu >= 0);
    drawToolbar(toolbarRect);
    drawStatusBar({0.0f, height - kStatusHeight, width, kStatusHeight});

    drawDock(leftRect, m_leftPanels, m_leftActive, 0);
    drawDock(rightRect, m_rightPanels, m_rightActive, 1);
    drawDock(bottomRect, m_bottomPanels, m_bottomActive, 2);
    handleDockDrop();
    if (m_draggingPanel && m_dragPanel.has_value()) {
        const ui::Rect ghost{m_input.mouse.x + 10.0f, m_input.mouse.y + 10.0f, 112.0f, 26.0f};
        m_renderer.rect(ghost, m_theme.panelAlt);
        m_renderer.rectOutline(ghost, m_theme.accentHover, 1.0f);
        m_renderer.textRect(ghost, PanelName(*m_dragPanel), m_theme.fontNormal, m_theme.text, 1);
    }

    // Mitte: Level-Tabs + Viewport.
    drawLevelTabs({centerRect.x, centerRect.y, centerRect.w, kTabsHeight});
    m_viewport = {centerRect.x, centerRect.y + kTabsHeight, centerRect.w,
                  std::max(centerRect.h - kTabsHeight, 40.0f)};
    drawViewport(m_viewport);

    // Splitter.
    if (m_ui.inputBlocked() == false) {
        const bool nearLeft = std::abs(m_input.mouse.x - m_leftWidth) < 5.0f &&
                              m_input.mouse.y > centerTop && m_input.mouse.y < centerBottom;
        const bool nearRight = std::abs(m_input.mouse.x - (width - m_rightWidth)) < 5.0f &&
                               m_input.mouse.y > centerTop && m_input.mouse.y < centerBottom;
        const bool nearBottom = std::abs(m_input.mouse.y - (centerBottom - m_bottomHeight)) < 5.0f &&
                                m_input.mouse.x > m_leftWidth &&
                                m_input.mouse.x < width - m_rightWidth;
        if (m_input.pressed[0]) {
            if (nearLeft) m_dragSplitter = 1;
            else if (nearRight) m_dragSplitter = 2;
            else if (nearBottom) m_dragSplitter = 3;
        }
        if (m_input.down[0] && m_dragSplitter != 0) {
            if (m_dragSplitter == 1) m_leftWidth = m_input.mouse.x;
            else if (m_dragSplitter == 2) m_rightWidth = width - m_input.mouse.x;
            else m_bottomHeight = centerBottom - m_input.mouse.y;
        }
        if (m_input.released[0]) m_dragSplitter = 0;
    }
    m_renderer.rect({m_leftWidth - 1.0f, centerTop, 2.0f, centerHeight}, m_theme.border);
    m_renderer.rect({width - m_rightWidth - 1.0f, centerTop, 2.0f, centerHeight},
                    m_theme.border);
    m_renderer.rect({m_leftWidth, centerBottom - m_bottomHeight - 1.0f,
                     width - m_leftWidth - m_rightWidth, 2.0f},
                    m_theme.border);

    if (m_openMenu >= 0) {
        m_ui.setInputBlocked(false);
        drawMenuPopup();
    }
    if (modal) {
        m_ui.setInputBlocked(false);
        drawDialog();
    }

    m_ui.endFrame();
    m_renderer.endFrame();
}

void EditorApp::togglePlaytest() {
    if (m_playMode) {
        m_playMode = false;
        return;
    }
    if (Level *level = activeLevel()) {
        m_playtest.reset(*level);
        m_playMode = true;
    }
}

void EditorApp::drawPlaytest() {
    Level *level = activeLevel();
    if (level == nullptr) {
        m_playMode = false;
        return;
    }
    if (m_input.keyPressed(sgl::Key::Escape) || m_input.keyPressed(sgl::Key::F5)) {
        m_playMode = false;
        return;
    }
    const f32 move = (m_window->isKeyPressed(sgl::Key::A) || m_window->isKeyPressed(sgl::Key::Left)
                          ? -1.0f
                          : 0.0f) +
                     (m_window->isKeyPressed(sgl::Key::D) || m_window->isKeyPressed(sgl::Key::Right)
                          ? 1.0f
                          : 0.0f);
    const bool jump = m_window->isKeyPressed(sgl::Key::Space) ||
                      m_window->isKeyPressed(sgl::Key::W) ||
                      m_window->isKeyPressed(sgl::Key::Up);
    m_playtest.step(*level, m_deltaTime, move, jump);

    const vec2 screenSize{static_cast<f32>(m_windowSize.x), static_cast<f32>(m_windowSize.y)};
    const f32 zoom = 2.0f;
    const vec2 player = m_playtest.position();
    const vec2 playerSize = m_playtest.size();
    const vec2 camera{player.x + playerSize.x * 0.5f,
                      player.y + playerSize.y * 0.5f - 80.0f};
    const auto screenFromWorld = [&](const vec2 &world) {
        return vec2{screenSize.x * 0.5f + (world.x - camera.x) * zoom,
                    screenSize.y * 0.5f + (world.y - camera.y) * zoom};
    };
    const vec2 topLeft = {-screenSize.x * 0.5f / zoom + camera.x,
                          -screenSize.y * 0.5f / zoom + camera.y};
    const vec2 bottomRight = {screenSize.x * 0.5f / zoom + camera.x,
                              screenSize.y * 0.5f / zoom + camera.y};
    const int x0 = std::max(static_cast<int>(std::floor(topLeft.x / level->tileW)) - 2, 0);
    const int y0 = std::max(static_cast<int>(std::floor(topLeft.y / level->tileH)) - 2, 0);
    const int x1 = std::min(static_cast<int>(std::ceil(bottomRight.x / level->tileW)) + 2,
                            level->width);
    const int y1 = std::min(static_cast<int>(std::ceil(bottomRight.y / level->tileH)) + 2,
                            level->height);
    for (const TileLayer &layer : level->tileLayers) {
        if (!layer.visible) continue;
        for (int ty = y0; ty < y1; ++ty) {
            for (int tx = x0; tx < x1; ++tx) {
                u32 gid = level->tileAt(layer, tx, ty);
                if (gid == 0) continue;
                gid = AnimatedTile(*level, gid, static_cast<f32>(sgl::GetTime()));
                const TileSet *tileset = FindTileSetForGid(*level, gid);
                if (tileset == nullptr) continue;
                ensureTileTexture(*tileset);
                const auto found = m_textures.find(tileset->image);
                if (found == m_textures.end() || !found->second.texture.isValid()) continue;
                const int columns = std::max(found->second.columns, 1);
                const int local = static_cast<int>(gid) - tileset->firstGid;
                const int sx = tileset->margin + (local % columns) * (tileset->tileW + tileset->spacing);
                const int sy = tileset->margin + (local / columns) * (tileset->tileH + tileset->spacing);
                const vec2 screen = screenFromWorld(
                    {static_cast<f32>(tx) * level->tileW, static_cast<f32>(ty) * level->tileH});
                m_renderer.imageSub({screen.x, screen.y, level->tileW * zoom, level->tileH * zoom},
                                    found->second.texture, sx, sy, tileset->tileW,
                                    tileset->tileH);
            }
        }
    }
    for (const ObjectLayer &layer : level->objectLayers) {
        if (!layer.visible) continue;
        for (const Entity &entity : layer.entities) {
            const vec2 screen = screenFromWorld({entity.x, entity.y});
            ui::Color color{120, 150, 190, 255};
            if (const EntityDef *def = FindEntityDef(m_project, entity.def)) color = def->color;
            m_renderer.rect({screen.x, screen.y, entity.w * zoom, entity.h * zoom},
                            ui::Color{color.r, color.g, color.b, 90});
            m_renderer.rectOutline({screen.x, screen.y, entity.w * zoom, entity.h * zoom}, color,
                                   1.5f);
        }
    }
    const vec2 playerScreen = screenFromWorld(player);
    m_renderer.rect({playerScreen.x, playerScreen.y, playerSize.x * zoom, playerSize.y * zoom},
                    ui::Color{80, 200, 120, 220});
    m_renderer.rectOutline({playerScreen.x, playerScreen.y, playerSize.x * zoom,
                            playerSize.y * zoom},
                           ui::Color{210, 255, 225, 255}, 2.0f);

    const ui::Rect hint{16.0f, static_cast<f32>(m_windowSize.y) - 58.0f,
                        static_cast<f32>(m_windowSize.x) - 32.0f, 42.0f};
    m_renderer.rect(hint, ui::Color{20, 22, 27, 210});
    m_renderer.rectOutline(hint, m_theme.border, 1.0f);
    m_renderer.textRect(hint, "Vorschau · A/D oder Pfeiltasten laufen · Leertaste/W springen · "
                             "Esc beendet",
                        m_theme.fontNormal, m_theme.text, 1);
}

bool EditorApp::saveScreenshot(const fs::path &file) {
    const ivec2 framebuffer = m_window->getFramebufferSize();
    if (framebuffer.x <= 0 || framebuffer.y <= 0) return false;
    std::vector<u8> pixels(static_cast<std::size_t>(framebuffer.x) * framebuffer.y * 4);
    gl::glPixelStorei(GL_PACK_ALIGNMENT, 1);
    gl::glReadBuffer(GL_BACK);
    gl::glReadPixels(0, 0, framebuffer.x, framebuffer.y, GL_RGBA, GL_UNSIGNED_BYTE,
                     pixels.data());
    sgl::Image image(framebuffer.x, framebuffer.y, sgl::ImageFormat::Rgba, pixels);
    image.flipVertically();
    try {
        sgl::ImageIO::Save(image, file);
    } catch (const std::exception &exception) {
        log(std::string("Screenshot: ") + exception.what());
        return false;
    }
    return true;
}

void EditorApp::handleShortcuts() {
    if (m_dialog.kind != DialogKind::None) {
        if (m_input.keyPressed(sgl::Key::Escape)) cancelDialog();
        return;
    }
    if (m_openMenu >= 0 && m_input.keyPressed(sgl::Key::Escape)) {
        m_openMenu = -1;
        return;
    }

    const bool control = m_input.control;
    if (control && m_input.keyPressed(sgl::Key::S)) {
        if (m_input.shift) saveProjectAsDialog();
        else saveProject();
    }
    if (control && m_input.keyPressed(sgl::Key::O)) openProjectDialog();
    if (control && m_input.keyPressed(sgl::Key::N)) {
        openDialog(DialogKind::NewProject, "Neues Projekt", "", fs::current_path(), "MeinProjekt");
    }
    if (control && m_input.keyPressed(sgl::Key::Z)) {
        if (m_input.shift) redo();
        else undo();
    }
    if (control && m_input.keyPressed(sgl::Key::Y)) redo();
    if (control && m_input.keyPressed(sgl::Key::I)) importDialog();
    if (control && m_input.keyPressed(sgl::Key::E)) exportDialog();
    if (control && m_input.keyPressed(sgl::Key::W)) closeDocument(m_active);
    if (m_input.keyPressed(sgl::Key::F5)) togglePlaytest();
    if (m_ui.isAnyTextFocused()) return;

    if (m_input.keyPressed(sgl::Key::V)) m_tool = Tool::Select;
    if (m_input.keyPressed(sgl::Key::B)) m_tool = Tool::Brush;
    if (m_input.keyPressed(sgl::Key::R)) m_tool = Tool::Rect;
    if (m_input.keyPressed(sgl::Key::L)) m_tool = Tool::Line;
    if (m_input.keyPressed(sgl::Key::F)) m_tool = Tool::Fill;
    if (m_input.keyPressed(sgl::Key::E)) m_tool = Tool::Eraser;
    if (m_input.keyPressed(sgl::Key::I)) m_tool = Tool::Picker;
    if (m_input.keyPressed(sgl::Key::N)) m_tool = Tool::Entity;
    if (m_input.keyPressed(sgl::Key::G)) m_showGrid = !m_showGrid;
    if (m_input.keyPressed(sgl::Key::M)) m_showMinimap = !m_showMinimap;
    if (m_input.keyPressed(sgl::Key::Delete) && m_selectedEntityId >= 0) {
        ObjectLayer *layer = activeObjectLayer();
        if (layer != nullptr) {
            layer->entities.erase(std::remove_if(layer->entities.begin(), layer->entities.end(),
                                                 [this](const Entity &entity) {
                                                     return entity.id == m_selectedEntityId;
                                                 }),
                                  layer->entities.end());
            m_selectedEntityId = -1;
            markDirty();
            commitHistory();
        }
    }
    if (m_input.keyPressed(sgl::Key::Equal) || m_input.keyPressed(sgl::Key::KPAdd)) {
        m_zoom = std::min(m_zoom * 1.2f, 8.0f);
    }
    if (m_input.keyPressed(sgl::Key::Minus) || m_input.keyPressed(sgl::Key::KPSubtract)) {
        m_zoom = std::max(m_zoom / 1.2f, 0.12f);
    }
}

void EditorApp::drawMenuBar() {
    m_renderer.rect({0.0f, 0.0f, static_cast<f32>(m_windowSize.x), kMenuHeight}, m_theme.header);
    m_renderer.line({0.0f, kMenuHeight - 1.0f},
                    {static_cast<f32>(m_windowSize.x), kMenuHeight - 1.0f}, 1.0f,
                    m_theme.border);
    const char *labels[] = {"Datei", "Bearbeiten", "Ansicht", "Ebene", "Hilfe"};
    f32 x = 8.0f;
    for (int i = 0; i < 5; ++i) {
        const f32 width = i == 1 ? 92.0f : (i == 3 ? 62.0f : 70.0f);
        const ui::Rect rect{x, 0.0f, width, kMenuHeight};
        if (m_ui.selectable(rect, labels[i], m_openMenu == i, 0)) {
            m_openMenu = (m_openMenu == i) ? -1 : i;
        }
        x += width;
    }
}

void EditorApp::drawMenuPopup() {
    const f32 widths[] = {70.0f, 92.0f, 70.0f, 62.0f, 70.0f};
    f32 x = 8.0f;
    for (int i = 0; i < m_openMenu && i < 5; ++i) x += widths[i];
    const ui::Rect popup{x, kMenuHeight, 240.0f, 300.0f};
    m_renderer.rect(popup, m_theme.panelAlt);
    m_renderer.rectOutline(popup, m_theme.border, 1.0f);

    struct Item {
        std::string label;
        std::function<void()> action;
        bool enabled = true;
        bool separator = false;
        bool checked = false;
    };
    std::vector<Item> items;
    Document *doc = activeDocument();
    const bool hasDoc = doc != nullptr;

    if (m_openMenu == 0) {
        items.push_back({"Neues Projekt…", [this] {
                             openDialog(DialogKind::NewProject, "Neues Projekt", "",
                                        fs::current_path(), "MeinProjekt");
                         }});
        items.push_back({"Projekt öffnen…", [this] { openProjectDialog(); }});
        items.push_back({"Projekt speichern", [this] { saveProject(); }, m_project.root.empty() == false});
        items.push_back({"Projekt speichern unter…", [this] { saveProjectAsDialog(); }, hasDoc});
        items.push_back({"", {}, true, true});
        items.push_back({"Level importieren (TMJ/TMX)…", [this] { importDialog(); }});
        items.push_back({"Level exportieren (TMJ/TMX)…", [this] { exportDialog(); }, hasDoc});
        items.push_back({"Level schließen", [this] { closeDocument(m_active); }, m_active >= 0});
        items.push_back({"", {}, true, true});
        items.push_back({"Tileset importieren…", [this] { importTilesetDialog(); }, hasDoc});
        items.push_back({"", {}, true, true});
        items.push_back({"Beenden", [this] { m_window->setCloseRequested(true); }});
    } else if (m_openMenu == 1) {
        items.push_back({"Rückgängig", [this] { undo(); }, hasDoc && doc->history.canUndo()});
        items.push_back({"Wiederholen", [this] { redo(); }, hasDoc && doc->history.canRedo()});
        items.push_back({"", {}, true, true});
        items.push_back({"Ebene duplizieren", [this] { duplicateLayer(); }, hasDoc});
        items.push_back({"Ebene löschen", [this] { deleteLayer(); }, hasDoc});
    } else if (m_openMenu == 2) {
        items.push_back({"Raster", [this] { m_showGrid = !m_showGrid; }, true, false, m_showGrid});
        items.push_back({"Minimap", [this] { m_showMinimap = !m_showMinimap; }, true, false,
                         m_showMinimap});
        items.push_back({"", {}, true, true});
        items.push_back({"Vorschau (F5)", [this] { togglePlaytest(); }, hasDoc});
        items.push_back({"", {}, true, true});
        items.push_back({"Zoom zurücksetzen", [this] { m_zoom = 1.0f; }, hasDoc});
        items.push_back({"Layout zurücksetzen", [this] { resetLayout(); saveLayout(); }});
        items.push_back({"", {}, true, true});
        items.push_back({"Ansicht einpassen", [this] {
                             Level *level = activeLevel();
                             if (level == nullptr) return;
                             const f32 zoomX = m_viewport.w / std::max(static_cast<f32>(level->width * level->tileW), 1.0f);
                             const f32 zoomY = m_viewport.h / std::max(static_cast<f32>(level->height * level->tileH), 1.0f);
                             m_zoom = std::clamp(std::min(zoomX, zoomY) * 0.92f, 0.12f, 8.0f);
                             m_cameraX = level->width * level->tileW * 0.5f;
                             m_cameraY = level->height * level->tileH * 0.5f;
                         }, hasDoc});
    } else if (m_openMenu == 3) {
        items.push_back({"Kachelebene hinzufügen", [this] { addTileLayer(); }, hasDoc});
        items.push_back({"Objektebene hinzufügen", [this] { addObjectLayer(); }, hasDoc});
        items.push_back({"", {}, true, true});
        items.push_back({"Ebene nach oben", [this] { moveLayer(-1); }, hasDoc});
        items.push_back({"Ebene nach unten", [this] { moveLayer(1); }, hasDoc});
        items.push_back({"", {}, true, true});
        items.push_back({"Ebene duplizieren", [this] { duplicateLayer(); }, hasDoc});
        items.push_back({"Ebene löschen", [this] { deleteLayer(); }, hasDoc});
    } else {
        items.push_back({"Tastenkürzel anzeigen", [this] {
                             log("V Auswahl · B Stift · R Rechteck · L Linie · F Füllen · E Radierer");
                             log("I Pipette · N Entity · G Raster · M Minimap · Strg+Z/Y Undo/Redo");
                         }});
        items.push_back({"Über den Editor", [this] {
                             m_status = "sgl Leveleditor 0.1 – In-House-UI auf OpenGL 3.3";
                         }});
    }

    f32 y = popup.y + 4.0f;
    for (const Item &item : items) {
        if (item.separator) {
            m_renderer.line({popup.x + 8.0f, y + 5.0f}, {popup.x + popup.w - 8.0f, y + 5.0f},
                            1.0f, m_theme.border);
            y += 11.0f;
            continue;
        }
        const ui::Rect row{popup.x + 2.0f, y, popup.w - 4.0f, 24.0f};
        if (item.enabled && m_ui.selectable(row, item.label, false, 0)) {
            item.action();
            m_openMenu = -1;
        }
        if (item.checked) {
            m_renderer.icon(ui::Icon::Check, {row.x + row.w - 24.0f, row.y, 20.0f, row.h},
                             m_theme.accentHover);
        }
        y += 24.0f;
    }
    if (m_input.pressed[0] && !popup.contains(m_input.mouse) &&
        m_input.mouse.y > kMenuHeight) {
        m_openMenu = -1;
    }
}

void EditorApp::drawToolbar(const ui::Rect &rect) {
    m_renderer.rect(rect, m_theme.header);
    m_renderer.line({rect.x, rect.y + rect.h - 1.0f}, {rect.x + rect.w, rect.y + rect.h - 1.0f},
                    1.0f, m_theme.border);
    f32 x = rect.x + 6.0f;
    const f32 y = rect.y + 5.0f;
    const f32 button = 28.0f;
    struct ToolButton {
        Tool tool;
        ui::Icon icon;
        const char *tip;
    };
    const ToolButton tools[] = {
        {Tool::Select, ui::Icon::Cursor, "Auswahl (V)"},
        {Tool::Brush, ui::Icon::Brush, "Stift (B)"},
        {Tool::Rect, ui::Icon::Rect, "Rechteck (R)"},
        {Tool::Line, ui::Icon::Line, "Linie (L)"},
        {Tool::Fill, ui::Icon::Fill, "Füllen (F)"},
        {Tool::Eraser, ui::Icon::Eraser, "Radierer (E)"},
        {Tool::Picker, ui::Icon::Picker, "Pipette (I)"},
        {Tool::Entity, ui::Icon::Entity, "Entity platzieren (N)"},
    };
    for (const ToolButton &item : tools) {
        if (m_ui.iconButton({x, y, button, button}, item.icon, item.tip, m_tool == item.tool)) {
            m_tool = item.tool;
        }
        x += button + 2.0f;
    }
    x += 8.0f;
    m_renderer.line({x, y}, {x, y + button}, 1.0f, m_theme.border);
    x += 8.0f;
    if (m_ui.iconButton({x, y, button, button}, ui::Icon::Undo, "Rückgängig (Strg+Z)",
                        false, activeDocument() != nullptr && activeDocument()->history.canUndo())) {
        undo();
    }
    x += button + 2.0f;
    if (m_ui.iconButton({x, y, button, button}, ui::Icon::Redo, "Wiederholen (Strg+Y)",
                        false, activeDocument() != nullptr && activeDocument()->history.canRedo())) {
        redo();
    }
    x += button + 8.0f;
    m_renderer.line({x, y}, {x, y + button}, 1.0f, m_theme.border);
    x += 8.0f;
    if (m_ui.iconButton({x, y, button, button}, ui::Icon::Save, "Projekt speichern (Strg+S)")) {
        saveProject();
    }
    x += button + 2.0f;
    if (m_ui.iconButton({x, y, button, button}, ui::Icon::Open, "Projekt öffnen (Strg+O)")) {
        openProjectDialog();
    }
    x += button + 8.0f;
    m_renderer.line({x, y}, {x, y + button}, 1.0f, m_theme.border);
    x += 8.0f;
    if (m_ui.iconButton({x, y, button, button}, ui::Icon::Grid, "Raster (G)", m_showGrid)) {
        m_showGrid = !m_showGrid;
    }
    x += button + 2.0f;
    if (m_ui.iconButton({x, y, button, button}, ui::Icon::ZoomOut, "Herauszoomen (-)")) {
        m_zoom = std::max(m_zoom / 1.2f, 0.12f);
    }
    x += button + 2.0f;
    char zoomText[32];
    std::snprintf(zoomText, sizeof(zoomText), "%d %%", static_cast<int>(std::lround(m_zoom * 100.0f)));
    m_renderer.textRect({x, y, 54.0f, button}, zoomText, m_theme.fontSmall, m_theme.textDim, 1);
    x += 56.0f;
    if (m_ui.iconButton({x, y, button, button}, ui::Icon::ZoomIn, "Hineinzoomen (+)")) {
        m_zoom = std::min(m_zoom * 1.2f, 8.0f);
    }
    x += button + 8.0f;
    m_renderer.line({x, y}, {x, y + button}, 1.0f, m_theme.border);
    x += 8.0f;
    if (m_ui.iconButton({x, y, button, button}, ui::Icon::Play, "Vorschau (F5)")) {
        togglePlaytest();
    }
    if (Document *doc = activeDocument(); doc != nullptr) {
        m_renderer.textRect({rect.x + rect.w - 330.0f, y, 320.0f, button},
                            std::string(doc->title) + (doc->dirty ? " *" : ""),
                            m_theme.fontSmall, m_theme.textDim, 2);
    }
}

void EditorApp::drawProjectPanel(const ui::Rect &rect) {
    f32 y = rect.y + 6.0f;
    m_renderer.textRect({rect.x + 8.0f, y, rect.w - 16.0f, 20.0f}, m_project.name,
                        m_theme.fontLarge, m_theme.text);
    y += 26.0f;
    m_renderer.textRect({rect.x + 8.0f, y, rect.w - 16.0f, 18.0f},
                        m_project.root.empty() ? "Kein Projekt geöffnet"
                                               : PathToUtf8(m_project.root),
                        m_theme.fontSmall, m_theme.textFaint);
    y += 24.0f;
    m_renderer.line({rect.x + 8.0f, y}, {rect.x + rect.w - 8.0f, y}, 1.0f, m_theme.border);
    y += 6.0f;
    m_renderer.textRect({rect.x + 8.0f, y, rect.w - 16.0f, 18.0f}, "Levels",
                        m_theme.fontSmall, m_theme.textDim);
    y += 20.0f;

    const ui::Rect list{rect.x + 4.0f, y, rect.w - 8.0f, std::max(rect.h - (y - rect.y) - 90.0f, 80.0f)};
    const ui::Rect content = m_ui.beginScroll(list, static_cast<f32>(m_project.levels.size()) * 24.0f + 8.0f);
    f32 rowY = content.y + 4.0f;
    for (std::size_t i = 0; i < m_project.levels.size(); ++i) {
        const std::string &path = m_project.levels[i];
        const bool current = m_active >= 0 && m_active < static_cast<int>(m_documents.size()) &&
                             PathToUtf8(m_documents[static_cast<std::size_t>(m_active)].path) ==
                                 PathToUtf8(ResolveAsset(m_project, path));
        if (m_ui.selectable({content.x + 2.0f, rowY, content.w - 8.0f, 24.0f}, FileStem(PathFromUtf8(path)),
                            current)) {
            for (std::size_t d = 0; d < m_documents.size(); ++d) {
                if (PathToUtf8(m_documents[d].path) ==
                    PathToUtf8(ResolveAsset(m_project, path))) {
                    m_active = static_cast<int>(d);
                    break;
                }
            }
        }
        rowY += 24.0f;
    }
    m_ui.endScroll();

    const f32 buttonsY = rect.y + rect.h - 36.0f;
    if (m_ui.button({rect.x + 8.0f, buttonsY, rect.w - 16.0f, 28.0f}, "Neues Level", true, true)) {
        newLevel();
    }
    if (m_ui.button({rect.x + 8.0f, buttonsY - 32.0f, rect.w - 16.0f, 26.0f},
                    "Projekt öffnen…")) {
        openProjectDialog();
    }
}

void EditorApp::drawEntityPanel(const ui::Rect &rect) {
    f32 y = rect.y + 6.0f;
    m_renderer.textRect({rect.x + 8.0f, y, rect.w - 16.0f, 20.0f}, "Entity-Definitionen",
                        m_theme.fontLarge, m_theme.text);
    y += 26.0f;
    m_renderer.textRect({rect.x + 8.0f, y, rect.w - 16.0f, 18.0f},
                        "Auswahl mit dem Entity-Werkzeug platzieren.", m_theme.fontSmall,
                        m_theme.textFaint);
    y += 24.0f;
    const ui::Rect content = m_ui.beginScroll({rect.x + 4.0f, y, rect.w - 8.0f, rect.h - 100.0f},
                                              static_cast<f32>(m_project.entityDefs.size()) * 26.0f + 8.0f);
    f32 rowY = content.y + 4.0f;
    for (const auto &def : m_project.entityDefs) {
        const bool selected = m_selectedEntityDef == def.name;
        if (m_ui.selectable({content.x + 2.0f, rowY, content.w - 8.0f, 24.0f}, def.name, selected)) {
            m_selectedEntityDef = def.name;
            m_tool = Tool::Entity;
        }
        m_renderer.rect({content.x + content.w - 26.0f, rowY + 5.0f, 14.0f, 14.0f}, def.color);
        rowY += 26.0f;
    }
    m_ui.endScroll();
    if (m_ui.button({rect.x + 8.0f, rect.y + rect.h - 34.0f, rect.w - 16.0f, 26.0f},
                    "Neue Definition")) {
        EntityDef def;
        def.name = "Objekt " + std::to_string(m_project.entityDefs.size() + 1);
        def.color = sgl::Color{120, 150, 190, 255};
        def.attributes.push_back(AttributeDef{"Name", "string", def.name, {}});
        m_project.entityDefs.push_back(def);
        m_selectedEntityDef = def.name;
        m_projectDirty = true;
        log("Entity-Definition angelegt: " + def.name);
    }
}

void EditorApp::drawPalettePanel(const ui::Rect &rect) {
    Level *level = activeLevel();
    f32 y = rect.y + 6.0f;
    m_renderer.textRect({rect.x + 8.0f, y, rect.w - 16.0f, 20.0f}, "Kachelpalette",
                        m_theme.fontLarge, m_theme.text);
    y += 26.0f;
    if (level == nullptr || level->tilesets.empty()) {
        m_renderer.textRect({rect.x + 8.0f, y, rect.w - 16.0f, 40.0f},
                            "Kein Tileset. Bitte eine Grafik importieren.", m_theme.fontSmall,
                            m_theme.textDim);
        if (m_ui.button({rect.x + 8.0f, y + 44.0f, rect.w - 16.0f, 28.0f}, "Tileset importieren…",
                        true, true)) {
            importTilesetDialog();
        }
        return;
    }
    std::vector<std::string> names;
    names.reserve(level->tilesets.size());
    for (const auto &tileset : level->tilesets) names.push_back(tileset.name);
    m_activeTileset = std::clamp(m_activeTileset, 0, static_cast<int>(names.size()) - 1);
    if (m_ui.combo({rect.x + 8.0f, y, rect.w - 16.0f, 26.0f}, "tileset", names, m_activeTileset)) {
    }
    y += 32.0f;
    TileSet &tileset = level->tilesets[static_cast<std::size_t>(m_activeTileset)];
    ensureTileTexture(tileset);
    const auto found = m_textures.find(tileset.image);
    const ui::Rect gridRect{rect.x + 4.0f, y, rect.w - 8.0f, rect.h - (y - rect.y) - 84.0f};
    const f32 cell = 40.0f;
    const int columns = std::max(tileset.columns, 1);
    const int count = std::max(tileset.tileCount, 0);
    const int rows = (count + columns - 1) / std::max(columns, 1);
    const ui::Rect content = m_ui.beginScroll(gridRect, static_cast<f32>(rows) * (cell + 2.0f) + 8.0f);
    for (int i = 0; i < count; ++i) {
        const int cx = i % columns;
        const int cy = i / columns;
        const ui::Rect cellRect{content.x + cx * (cell + 2.0f) + 2.0f,
                                content.y + cy * (cell + 2.0f) + 2.0f, cell, cell};
        const u32 gid = static_cast<u32>(tileset.firstGid + i);
        if (gid == m_selectedTile) {
            m_renderer.rect(cellRect.grown(1.0f), m_theme.accent);
        }
        m_renderer.rect(cellRect, m_theme.panelAlt);
        if (found != m_textures.end() && found->second.texture.isValid()) {
            const int sx = tileset.margin + cx * (tileset.tileW + tileset.spacing);
            const int sy = tileset.margin + cy * (tileset.tileH + tileset.spacing);
            m_renderer.imageSub(cellRect.inset(1.0f), found->second.texture, sx, sy,
                                tileset.tileW, tileset.tileH);
        }
        if (!m_ui.inputBlocked() && cellRect.contains(m_input.mouse) && m_input.pressed[0]) {
            m_selectedTile = gid;
        }
    }
    m_ui.endScroll();
    if (m_ui.button({rect.x + 8.0f, rect.y + rect.h - 32.0f, rect.w - 16.0f, 26.0f},
                    "Weiteres Tileset importieren…")) {
        importTilesetDialog();
    }
    char info[96];
    std::snprintf(info, sizeof(info), "GID %u · %d×%d · %zu Animationen", m_selectedTile,
                  tileset.tileW, tileset.tileH, tileset.animations.size());
    m_renderer.textRect({rect.x + 8.0f, rect.y + rect.h - 56.0f, rect.w - 16.0f, 18.0f}, info,
                        m_theme.fontSmall, m_theme.textDim);
    if (!tileset.wangsets.empty()) {
        const auto json = sgl::detail::Json::Parse(tileset.wangsets);
        std::string names;
        if (json.isValid() && json.isArray()) {
            for (int i = 0; i < json.size() && i < 3; ++i) {
                if (const auto *name = json.at(i).find("name"); name != nullptr && name->isString()) {
                    if (!names.empty()) names += ", ";
                    names += name->string();
                }
            }
        }
        m_renderer.textRect({rect.x + 8.0f, rect.y + rect.h - 76.0f, rect.w - 16.0f, 18.0f},
                            "Wang: " + names, m_theme.fontSmall, m_theme.success);
    }
}

void EditorApp::drawLayersPanel(const ui::Rect &rect) {
    Level *level = activeLevel();
    if (level == nullptr) {
        m_renderer.textRect(rect.inset(8.0f), "Kein Level geöffnet.", m_theme.fontSmall,
                            m_theme.textDim);
        return;
    }
    const f32 rowHeight = 24.0f;
    const ui::Rect content = m_ui.beginScroll(
        rect.inset(4.0f),
        static_cast<f32>(level->tileLayers.size() + level->objectLayers.size()) * rowHeight + 8.0f);
    f32 y = content.y + 4.0f;
    auto drawRow = [&](const std::string &name, bool object, int index, bool visible, bool locked) {
        m_ui.pushId(name + (object ? "##o" : "##t"));
        const bool selected = object ? (m_layerSelectionIsObject && m_activeObjectLayer == index)
                                     : (!m_layerSelectionIsObject && m_activeTileLayer == index);
        const ui::Rect row{content.x + 2.0f, y, content.w - 8.0f, rowHeight};
        if (m_ui.selectable({row.x, row.y, row.w - 56.0f, row.h}, name, selected)) {
            m_layerSelectionIsObject = object;
            if (object) m_activeObjectLayer = index;
            else m_activeTileLayer = index;
            m_selectedEntityId = -1;
        }
        if (m_ui.iconButton({row.x + row.w - 54.0f, row.y + 3.0f, 22.0f, 20.0f},
                            visible ? ui::Icon::Eye : ui::Icon::EyeOff, "Sichtbarkeit")) {
            if (object) level->objectLayers[static_cast<std::size_t>(index)].visible = !visible;
            else level->tileLayers[static_cast<std::size_t>(index)].visible = !visible;
            markDirty();
            commitHistory();
        }
        if (m_ui.iconButton({row.x + row.w - 28.0f, row.y + 3.0f, 22.0f, 20.0f},
                            locked ? ui::Icon::Lock : ui::Icon::Unlock, "Sperren")) {
            if (object) level->objectLayers[static_cast<std::size_t>(index)].locked = !locked;
            else level->tileLayers[static_cast<std::size_t>(index)].locked = !locked;
            markDirty();
            commitHistory();
        }
        m_ui.popId();
        y += rowHeight;
    };
    for (std::size_t i = 0; i < level->tileLayers.size(); ++i) {
        drawRow(level->tileLayers[i].name, false, static_cast<int>(i), level->tileLayers[i].visible,
                level->tileLayers[i].locked);
    }
    for (std::size_t i = 0; i < level->objectLayers.size(); ++i) {
        drawRow(level->objectLayers[i].name, true, static_cast<int>(i),
                level->objectLayers[i].visible, level->objectLayers[i].locked);
    }
    m_ui.endScroll();
}

void EditorApp::drawConsolePanel(const ui::Rect &rect) {
    const ui::Rect content = m_ui.beginScroll(
        rect.inset(4.0f), static_cast<f32>(m_log.size()) * 18.0f + 8.0f);
    f32 y = content.y + 4.0f;
    for (const auto &line : m_log) {
        m_renderer.textRect({content.x + 6.0f, y, content.w - 12.0f, 18.0f}, line,
                            m_theme.fontSmall, m_theme.textDim);
        y += 18.0f;
    }
    m_ui.endScroll();
}

void EditorApp::drawAssetsPanel(const ui::Rect &rect) {
    f32 y = rect.y + 6.0f;
    m_renderer.textRect({rect.x + 8.0f, y, rect.w - 16.0f, 18.0f}, "Projekt-Assets",
                        m_theme.fontSmall, m_theme.textDim);
    y += 22.0f;
    m_renderer.textRect({rect.x + 8.0f, y, rect.w - 16.0f, 18.0f},
                        std::to_string(m_project.tilesets.size()) + " Tilesets · " +
                            std::to_string(m_project.entityDefs.size()) + " Entity-Definitionen",
                        m_theme.fontSmall, m_theme.text);
    y += 22.0f;
    for (const auto &tileset : m_project.tilesets) {
        m_renderer.textRect({rect.x + 14.0f, y, rect.w - 28.0f, 18.0f},
                            "Tileset: " + tileset.name + "  (" + tileset.image + ")",
                            m_theme.fontSmall, m_theme.textDim);
        y += 18.0f;
    }
    for (const auto &layer : m_documents) {
        m_renderer.textRect({rect.x + 14.0f, y, rect.w - 28.0f, 18.0f},
                            "Level: " + layer.title, m_theme.fontSmall, m_theme.textDim);
        y += 18.0f;
    }
}

void EditorApp::drawStatusBar(const ui::Rect &rect) {
    m_renderer.rect(rect, m_theme.header);
    m_renderer.line({rect.x, rect.y}, {rect.x + rect.w, rect.y}, 1.0f, m_theme.border);
    std::string text = m_status.empty() ? "Bereit" : m_status;
    if (Level *level = activeLevel(); level != nullptr && viewportHovered()) {
        const ivec2 cell = cellFromScreen(m_input.mouse);
        char buffer[96];
        std::snprintf(buffer, sizeof(buffer), "Zelle %d, %d", cell.x, cell.y);
        text += "  ·  ";
        text += buffer;
    }
    char right[160];
    std::snprintf(right, sizeof(right), "%s  ·  Zoom %d %%  ·  %.0f FPS%s", ToolName(m_tool),
                  static_cast<int>(std::lround(m_zoom * 100.0f)), static_cast<double>(m_fps),
                  (activeDocument() != nullptr && activeDocument()->dirty) ? "  ·  ungespeichert"
                                                                          : "");
    m_renderer.textRect({rect.x + 8.0f, rect.y, rect.w - 320.0f, rect.h}, text,
                        m_theme.fontSmall, m_theme.textDim);
    m_renderer.textRect({rect.x + rect.w - 420.0f, rect.y, 412.0f, rect.h}, right,
                        m_theme.fontSmall, m_theme.textDim, 2);
}

void EditorApp::drawLevelTabs(const ui::Rect &rect) {
    m_renderer.rect(rect, m_theme.header);
    m_renderer.line({rect.x, rect.y + rect.h - 1.0f}, {rect.x + rect.w, rect.y + rect.h - 1.0f},
                    1.0f, m_theme.border);
    if (m_documents.empty()) {
        m_renderer.textRect(rect.inset(8.0f, 0.0f), "Datei → Neues Projekt… oder Projekt öffnen…",
                            m_theme.fontSmall, m_theme.textFaint);
        return;
    }
    f32 x = rect.x + 4.0f;
    for (std::size_t i = 0; i < m_documents.size(); ++i) {
        const std::string label = m_documents[i].title + (m_documents[i].dirty ? " *" : "");
        const f32 width = std::min(std::max(m_renderer.textWidth(label, m_theme.fontNormal) + 28.0f,
                                            90.0f),
                                   180.0f);
        if (m_ui.tab({x, rect.y, width, rect.h}, label, m_active == static_cast<int>(i))) {
            m_active = static_cast<int>(i);
            m_selectedEntityId = -1;
        }
        x += width + 2.0f;
        if (x > rect.x + rect.w - 80.0f) break;
    }
}

void EditorApp::log(std::string message) {
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    char stamp[16];
    std::strftime(stamp, sizeof(stamp), "%H:%M:%S", &local);
    m_log.push_back(std::string(stamp) + "  " + std::move(message));
    if (m_log.size() > 200) m_log.erase(m_log.begin(), m_log.begin() + 50);
}

void EditorApp::updateAutosave(f32 dt) {
    if (m_project.root.empty()) return;
    m_autosaveTimer += dt;
    if (m_autosaveTimer < 30.0f) return;
    m_autosaveTimer = 0.0f;
    bool anyDirty = false;
    for (Document &doc : m_documents) {
        if (!doc.dirty) continue;
        anyDirty = true;
        std::string error;
        const fs::path target = m_project.root / ".sgl" / "autosave" /
                                (doc.title + ".json");
        SaveLevel(doc.level, target, error);
    }
    if (anyDirty) log("Autosave geschrieben.");
}

} // namespace editor
