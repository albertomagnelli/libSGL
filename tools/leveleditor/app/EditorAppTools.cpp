#include "EditorApp.hpp"

#include <sgl/Image.hpp>
#include <sgl/Time.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <system_error>

namespace editor {
namespace {

bool HasExtension(const std::string &name, const std::string &extension) {
    if (extension.empty()) return true;
    return name.size() >= extension.size() &&
           name.compare(name.size() - extension.size(), extension.size(), extension) == 0;
}

std::string EnsureExtension(std::string name, const std::string &extension) {
    if (name.empty()) return name;
    if (name.size() < extension.size() ||
        name.compare(name.size() - extension.size(), extension.size(), extension) != 0) {
        name += extension;
    }
    return name;
}

std::string MakeProjectRelativeSafe(const Project &project, const fs::path &file) {
    if (project.root.empty()) return PathToUtf8(file);
    std::error_code ec;
    const fs::path relative = fs::relative(file, project.root, ec);
    if (!ec && !relative.empty() && PathToUtf8(relative).rfind("..", 0) != 0) {
        return PathToUtf8(relative);
    }
    return PathToUtf8(file);
}


std::string RelativeLevelPath(const Project &project, const fs::path &file) {
    std::error_code ec;
    const fs::path relative = fs::relative(file, project.root, ec);
    if (!ec && !relative.empty()) return PathToUtf8(relative);
    return PathToUtf8(file);
}

void ResizeLevel(Level &level, int width, int height) {
    width = std::clamp(width, 1, 4096);
    height = std::clamp(height, 1, 4096);
    if (width == level.width && height == level.height) return;
    for (TileLayer &layer : level.tileLayers) {
        std::vector<u32> tiles(static_cast<std::size_t>(width) * height, 0);
        const int copyW = std::min(width, level.width);
        const int copyH = std::min(height, level.height);
        for (int y = 0; y < copyH; ++y) {
            for (int x = 0; x < copyW; ++x) {
                tiles[static_cast<std::size_t>(y) * width + x] =
                    layer.tiles[static_cast<std::size_t>(y) * level.width + x];
            }
        }
        layer.tiles = std::move(tiles);
    }
    level.width = width;
    level.height = height;
}

} // namespace

// ── Dokument ────────────────────────────────────────────────────────────────

EditorApp::Document *EditorApp::activeDocument() {
    if (m_active < 0 || m_active >= static_cast<int>(m_documents.size())) return nullptr;
    return &m_documents[static_cast<std::size_t>(m_active)];
}

const EditorApp::Document *EditorApp::activeDocument() const {
    if (m_active < 0 || m_active >= static_cast<int>(m_documents.size())) return nullptr;
    return &m_documents[static_cast<std::size_t>(m_active)];
}

Level *EditorApp::activeLevel() {
    Document *doc = activeDocument();
    return doc != nullptr ? &doc->level : nullptr;
}

TileLayer *EditorApp::activeTileLayer() {
    Level *level = activeLevel();
    if (level == nullptr || level->tileLayers.empty()) return nullptr;
    m_activeTileLayer = std::clamp(m_activeTileLayer, 0, static_cast<int>(level->tileLayers.size()) - 1);
    return &level->tileLayers[static_cast<std::size_t>(m_activeTileLayer)];
}

ObjectLayer *EditorApp::activeObjectLayer() {
    Level *level = activeLevel();
    if (level == nullptr || level->objectLayers.empty()) return nullptr;
    m_activeObjectLayer = std::clamp(m_activeObjectLayer, 0, static_cast<int>(level->objectLayers.size()) - 1);
    return &level->objectLayers[static_cast<std::size_t>(m_activeObjectLayer)];
}

void EditorApp::markDirty() {
    if (Document *doc = activeDocument()) doc->dirty = true;
    m_projectDirty = true;
}

void EditorApp::commitHistory() {
    if (Document *doc = activeDocument()) doc->history.commit(doc->level);
}

void EditorApp::undo() {
    Document *doc = activeDocument();
    if (doc == nullptr || !doc->history.undo(doc->level)) return;
    doc->dirty = true;
    m_projectDirty = true;
    m_selectedEntityId = -1;
    log("Rückgängig.");
}

void EditorApp::redo() {
    Document *doc = activeDocument();
    if (doc == nullptr || !doc->history.redo(doc->level)) return;
    doc->dirty = true;
    m_projectDirty = true;
    m_selectedEntityId = -1;
    log("Wiederholt.");
}

void EditorApp::addTileLayer() {
    Level *level = activeLevel();
    if (level == nullptr) return;
    TileLayer layer;
    layer.name = "Kacheln " + std::to_string(level->tileLayers.size() + 1);
    layer.tiles.assign(static_cast<std::size_t>(level->width) * level->height, 0);
    level->tileLayers.push_back(std::move(layer));
    m_activeTileLayer = static_cast<int>(level->tileLayers.size()) - 1;
    m_layerSelectionIsObject = false;
    markDirty();
    commitHistory();
    log("Kachelebene hinzugefügt.");
}

void EditorApp::addObjectLayer() {
    Level *level = activeLevel();
    if (level == nullptr) return;
    ObjectLayer layer;
    layer.name = "Objekte " + std::to_string(level->objectLayers.size() + 1);
    level->objectLayers.push_back(std::move(layer));
    m_activeObjectLayer = static_cast<int>(level->objectLayers.size()) - 1;
    m_layerSelectionIsObject = true;
    markDirty();
    commitHistory();
    log("Objektebene hinzugefügt.");
}

void EditorApp::duplicateLayer() {
    Level *level = activeLevel();
    if (level == nullptr) return;
    if (m_layerSelectionIsObject) {
        if (m_activeObjectLayer < 0 || m_activeObjectLayer >= static_cast<int>(level->objectLayers.size())) return;
        ObjectLayer copy = level->objectLayers[static_cast<std::size_t>(m_activeObjectLayer)];
        copy.name += " Kopie";
        for (Entity &entity : copy.entities) entity.id = NewEntityId(*level);
        level->objectLayers.insert(level->objectLayers.begin() + m_activeObjectLayer + 1, std::move(copy));
        ++m_activeObjectLayer;
    } else {
        if (m_activeTileLayer < 0 || m_activeTileLayer >= static_cast<int>(level->tileLayers.size())) return;
        TileLayer copy = level->tileLayers[static_cast<std::size_t>(m_activeTileLayer)];
        copy.name += " Kopie";
        level->tileLayers.insert(level->tileLayers.begin() + m_activeTileLayer + 1, std::move(copy));
        ++m_activeTileLayer;
    }
    markDirty();
    commitHistory();
    log("Ebene dupliziert.");
}

void EditorApp::deleteLayer() {
    Level *level = activeLevel();
    if (level == nullptr) return;
    if (m_layerSelectionIsObject) {
        if (m_activeObjectLayer < 0 || m_activeObjectLayer >= static_cast<int>(level->objectLayers.size())) return;
        level->objectLayers.erase(level->objectLayers.begin() + m_activeObjectLayer);
        m_activeObjectLayer = std::clamp(m_activeObjectLayer - 1, 0,
                                         static_cast<int>(level->objectLayers.size()) - 1);
    } else {
        if (m_activeTileLayer < 0 || m_activeTileLayer >= static_cast<int>(level->tileLayers.size())) return;
        level->tileLayers.erase(level->tileLayers.begin() + m_activeTileLayer);
        m_activeTileLayer = std::clamp(m_activeTileLayer - 1, 0,
                                       static_cast<int>(level->tileLayers.size()) - 1);
    }
    m_selectedEntityId = -1;
    markDirty();
    commitHistory();
    log("Ebene gelöscht.");
}

void EditorApp::moveLayer(int direction) {
    Level *level = activeLevel();
    if (level == nullptr) return;
    if (m_layerSelectionIsObject) {
        const int target = m_activeObjectLayer + direction;
        if (m_activeObjectLayer < 0 || m_activeObjectLayer >= static_cast<int>(level->objectLayers.size()) ||
            target < 0 || target >= static_cast<int>(level->objectLayers.size())) return;
        std::swap(level->objectLayers[static_cast<std::size_t>(m_activeObjectLayer)],
                  level->objectLayers[static_cast<std::size_t>(target)]);
        m_activeObjectLayer = target;
    } else {
        const int target = m_activeTileLayer + direction;
        if (m_activeTileLayer < 0 || m_activeTileLayer >= static_cast<int>(level->tileLayers.size()) ||
            target < 0 || target >= static_cast<int>(level->tileLayers.size())) return;
        std::swap(level->tileLayers[static_cast<std::size_t>(m_activeTileLayer)],
                  level->tileLayers[static_cast<std::size_t>(target)]);
        m_activeTileLayer = target;
    }
    markDirty();
    commitHistory();
}

// ── Kamera und Werkzeuge ────────────────────────────────────────────────────

ivec2 EditorApp::cellFromScreen(const vec2 &point) const {
    const Level *level = const_cast<EditorApp *>(this)->activeLevel();
    if (level == nullptr) return {};
    const vec2 world = worldFromScreen(point);
    return {static_cast<int>(std::floor(world.x / level->tileW)),
            static_cast<int>(std::floor(world.y / level->tileH))};
}

vec2 EditorApp::screenFromWorld(const vec2 &world) const {
    return {m_viewport.x + m_viewport.w * 0.5f + (world.x - m_cameraX) * m_zoom,
            m_viewport.y + m_viewport.h * 0.5f + (world.y - m_cameraY) * m_zoom};
}

vec2 EditorApp::worldFromScreen(const vec2 &point) const {
    return {(point.x - (m_viewport.x + m_viewport.w * 0.5f)) / m_zoom + m_cameraX,
            (point.y - (m_viewport.y + m_viewport.h * 0.5f)) / m_zoom + m_cameraY};
}

bool EditorApp::viewportHovered() const {
    return m_dialog.kind == DialogKind::None && m_openMenu < 0 && !m_ui.inputBlocked() &&
           m_viewport.contains(m_input.mouse);
}

Entity *EditorApp::findEntityAt(const vec2 &world) {
    Level *level = activeLevel();
    if (level == nullptr) return nullptr;
    if (!m_layerSelectionIsObject && level->objectLayers.empty()) return nullptr;
    for (auto layerIt = level->objectLayers.rbegin(); layerIt != level->objectLayers.rend(); ++layerIt) {
        if (!layerIt->visible || layerIt->locked) continue;
        for (auto entityIt = layerIt->entities.rbegin(); entityIt != layerIt->entities.rend(); ++entityIt) {
            Entity &entity = *entityIt;
            if (world.x >= entity.x && world.y >= entity.y && world.x <= entity.x + entity.w &&
                world.y <= entity.y + entity.h) {
                return &entity;
            }
        }
    }
    return nullptr;
}

void EditorApp::selectEntityAt(const vec2 &world) {
    Level *level = activeLevel();
    if (level == nullptr) return;
    for (std::size_t l = 0; l < level->objectLayers.size(); ++l) {
        ObjectLayer &layer = level->objectLayers[l];
        if (!layer.visible || layer.locked) continue;
        for (auto it = layer.entities.rbegin(); it != layer.entities.rend(); ++it) {
            const Entity &entity = *it;
            if (world.x >= entity.x && world.y >= entity.y && world.x <= entity.x + entity.w &&
                world.y <= entity.y + entity.h) {
                m_selectedEntityId = entity.id;
                m_layerSelectionIsObject = true;
                m_activeObjectLayer = static_cast<int>(l);
                return;
            }
        }
    }
    m_selectedEntityId = -1;
}

void EditorApp::applyToolAt(const ivec2 &cell, bool erase) {
    Level *level = activeLevel();
    TileLayer *layer = activeTileLayer();
    if (level == nullptr || layer == nullptr || layer->locked || !layer->visible) return;
    if (cell.x < 0 || cell.y < 0 || cell.x >= level->width || cell.y >= level->height) return;
    u32 *tile = level->tilePtr(*layer, cell.x, cell.y);
    if (tile == nullptr) return;
    const u32 value = erase ? 0u : m_selectedTile;
    if (*tile != value) {
        *tile = value;
        markDirty();
    }
}

void EditorApp::beginStroke(const ivec2 &cell, bool erase) {
    Level *level = activeLevel();
    if (level == nullptr) return;
    m_stroking = true;
    m_strokeErase = erase;
    m_strokeStart = cell;
    m_strokeEnd = cell;
    if (m_tool == Tool::Brush || m_tool == Tool::Eraser) {
        applyToolAt(cell, erase || m_tool == Tool::Eraser);
    }
}

void EditorApp::updateStroke(const ivec2 &cell, bool erase) {
    if (!m_stroking) return;
    m_strokeEnd = cell;
    if (m_tool == Tool::Brush || m_tool == Tool::Eraser) {
        applyToolAt(cell, erase || m_tool == Tool::Eraser);
    }
}

void EditorApp::endStroke() {
    if (!m_stroking) return;
    Level *level = activeLevel();
    TileLayer *layer = activeTileLayer();
    if (level != nullptr && layer != nullptr) {
        if (m_tool == Tool::Rect) {
            paintRect(m_strokeStart, m_strokeEnd, m_strokeErase ? 0u : m_selectedTile);
        } else if (m_tool == Tool::Line) {
            paintLine(m_strokeStart, m_strokeEnd, m_strokeErase ? 0u : m_selectedTile);
        }
    }
    m_stroking = false;
    markDirty();
    commitHistory();
}

void EditorApp::paintRect(const ivec2 &a, const ivec2 &b, u32 value) {
    Level *level = activeLevel();
    TileLayer *layer = activeTileLayer();
    if (level == nullptr || layer == nullptr || layer->locked) return;
    const int x0 = std::clamp(std::min(a.x, b.x), 0, level->width - 1);
    const int x1 = std::clamp(std::max(a.x, b.x), 0, level->width - 1);
    const int y0 = std::clamp(std::min(a.y, b.y), 0, level->height - 1);
    const int y1 = std::clamp(std::max(a.y, b.y), 0, level->height - 1);
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            if (u32 *tile = level->tilePtr(*layer, x, y); tile != nullptr) *tile = value;
        }
    }
}

void EditorApp::paintLine(const ivec2 &a, const ivec2 &b, u32 value) {
    int x0 = a.x;
    int y0 = a.y;
    const int x1 = b.x;
    const int y1 = b.y;
    const int dx = std::abs(x1 - x0);
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0);
    const int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    Level *level = activeLevel();
    TileLayer *layer = activeTileLayer();
    if (level == nullptr || layer == nullptr || layer->locked) return;
    for (;;) {
        if (u32 *tile = level->tilePtr(*layer, x0, y0); tile != nullptr) *tile = value;
        if (x0 == x1 && y0 == y1) break;
        const int e2 = 2 * error;
        if (e2 >= dy) { error += dy; x0 += sx; }
        if (e2 <= dx) { error += dx; y0 += sy; }
    }
}

void EditorApp::floodFill(const ivec2 &cell, u32 value) {
    Level *level = activeLevel();
    TileLayer *layer = activeTileLayer();
    if (level == nullptr || layer == nullptr || layer->locked) return;
    if (cell.x < 0 || cell.y < 0 || cell.x >= level->width || cell.y >= level->height) return;
    const u32 original = level->tileAt(*layer, cell.x, cell.y);
    if (original == value) return;
    std::vector<ivec2> stack;
    stack.push_back(cell);
    std::size_t guard = 0;
    while (!stack.empty() && guard < 250000) {
        const ivec2 point = stack.back();
        stack.pop_back();
        if (point.x < 0 || point.y < 0 || point.x >= level->width || point.y >= level->height) continue;
        u32 *tile = level->tilePtr(*layer, point.x, point.y);
        if (tile == nullptr || *tile != original) continue;
        *tile = value;
        ++guard;
        stack.push_back({point.x + 1, point.y});
        stack.push_back({point.x - 1, point.y});
        stack.push_back({point.x, point.y + 1});
        stack.push_back({point.x, point.y - 1});
    }
    markDirty();
}

// ── Viewport ────────────────────────────────────────────────────────────────

void EditorApp::drawViewport(const ui::Rect &rect) {
    m_viewport = rect;
    m_renderer.rect(rect, m_theme.viewport);
    Level *level = activeLevel();

    if (level == nullptr) {
        m_renderer.textRect(rect, "Kein Level geöffnet. Datei → Neues Projekt… oder Projekt öffnen…",
                            m_theme.fontLarge, m_theme.textFaint, 1);
        return;
    }

    const ui::Rect minimap{rect.x + rect.w - 200.0f - 12.0f,
                           rect.y + rect.h - 130.0f - 12.0f, 200.0f, 130.0f};
    const bool overMinimap = m_showMinimap && minimap.contains(m_input.mouse);

    // Eingabe vor dem Zeichnen, damit Malen ohne Frame-Verzögerung wirkt.
    const bool hovered = viewportHovered() && !overMinimap;
    const bool leftPressed = m_input.pressed[0] && hovered;
    const bool rightPressed = m_input.pressed[1] && hovered;
    const bool leftDown = m_input.down[0];
    const bool leftReleased = m_input.released[0];

    if (hovered && m_input.wheel != 0.0f) {
        const vec2 before = worldFromScreen(m_input.mouse);
        m_zoom = std::clamp(m_zoom * std::exp(m_input.wheel * 0.12f), 0.12f, 8.0f);
        const vec2 after = worldFromScreen(m_input.mouse);
        m_cameraX += before.x - after.x;
        m_cameraY += before.y - after.y;
        m_input.wheel = 0.0f;
    }

    const bool panButton = hovered && m_input.pressed[2];
    if (panButton || (hovered && m_tool == Tool::Pan && leftPressed)) {
        m_panning = true;
        m_panStart = m_input.mouse;
        m_panCameraStart = {m_cameraX, m_cameraY};
    }
    if (m_panning) {
        if (leftDown || m_input.down[2]) {
            m_cameraX = m_panCameraStart.x - (m_input.mouse.x - m_panStart.x) / m_zoom;
            m_cameraY = m_panCameraStart.y - (m_input.mouse.y - m_panStart.y) / m_zoom;
        }
    }
    if (m_input.released[2] || (m_tool == Tool::Pan && leftReleased)) m_panning = false;

    if (!m_panning && hovered && (leftPressed || rightPressed)) {
        const ivec2 cell = cellFromScreen(m_input.mouse);
        const bool erase = rightPressed;
        switch (m_tool) {
        case Tool::Select: {
            if (rightPressed) {
                m_selectedEntityId = -1;
            } else {
                selectEntityAt(worldFromScreen(m_input.mouse));
                if (m_selectedEntityId >= 0) {
                    ObjectLayer *layer = activeObjectLayer();
                    if (layer != nullptr) {
                        for (const Entity &entity : layer->entities) {
                            if (entity.id == m_selectedEntityId) {
                                m_movingEntity = true;
                                m_dragEntityId = entity.id;
                                m_entityOriginal = {entity.x, entity.y};
                                m_entityDragStart = m_input.mouse;
                                break;
                            }
                        }
                    }
                }
            }
            break;
        }
        case Tool::Entity: {
            if (!erase && !m_selectedEntityDef.empty()) {
                ObjectLayer *layer = activeObjectLayer();
                if (layer != nullptr && !layer->locked) {
                    Entity entity;
                    entity.id = NewEntityId(*level);
                    entity.def = m_selectedEntityDef;
                    if (const EntityDef *def = FindEntityDef(m_project, entity.def)) {
                        for (const auto &attribute : def->attributes) {
                            entity.attrs[attribute.name] = attribute.defaultValue;
                        }
                    }
                    entity.x = static_cast<f32>(cell.x) * level->tileW;
                    entity.y = static_cast<f32>(cell.y) * level->tileH;
                    entity.w = static_cast<f32>(level->tileW);
                    entity.h = static_cast<f32>(level->tileH);
                    layer->entities.push_back(entity);
                    m_selectedEntityId = entity.id;
                    markDirty();
                    commitHistory();
                }
            }
            break;
        }
        case Tool::Picker: {
            TileLayer *layer = activeTileLayer();
            if (layer != nullptr) {
                const u32 tile = level->tileAt(*layer, cell.x, cell.y);
                if (tile != 0) {
                    m_selectedTile = tile;
                    log("Kachel gewählt: GID " + std::to_string(tile));
                }
            }
            break;
        }
        case Tool::Fill: {
            floodFill(cell, erase ? 0u : m_selectedTile);
            commitHistory();
            break;
        }
        case Tool::Brush:
        case Tool::Eraser:
        case Tool::Rect:
        case Tool::Line:
            beginStroke(cell, erase);
            break;
        case Tool::Pan:
            break;
        }
    }
    if (m_stroking && (leftDown || m_input.down[1])) {
        updateStroke(cellFromScreen(m_input.mouse), m_input.down[1]);
    }
    if (m_stroking && leftReleased) {
        endStroke();
    } else if (m_stroking && m_input.released[1] && !m_input.down[0]) {
        endStroke();
    }

    if (m_movingEntity) {
        if (leftDown) {
            ObjectLayer *layer = activeObjectLayer();
            if (layer != nullptr) {
                for (Entity &entity : layer->entities) {
                    if (entity.id != m_dragEntityId) continue;
                    const vec2 delta{(m_input.mouse.x - m_entityDragStart.x) / m_zoom,
                                     (m_input.mouse.y - m_entityDragStart.y) / m_zoom};
                    entity.x = m_entityOriginal.x + delta.x;
                    entity.y = m_entityOriginal.y + delta.y;
                    if (!m_input.alt) {
                        entity.x = std::round(entity.x / level->tileW) * level->tileW;
                        entity.y = std::round(entity.y / level->tileH) * level->tileH;
                    }
                    markDirty();
                    break;
                }
            }
        }
        if (leftReleased) {
            m_movingEntity = false;
            commitHistory();
        }
    }

    // ── Zeichnen ──────────────────────────────────────────────────────────────
    m_renderer.pushClip(rect);
    const vec2 topLeft = worldFromScreen({rect.x, rect.y});
    const vec2 bottomRight = worldFromScreen({rect.x + rect.w, rect.y + rect.h});
    const int x0 = std::max(static_cast<int>(std::floor(topLeft.x / level->tileW)) - 1, 0);
    const int y0 = std::max(static_cast<int>(std::floor(topLeft.y / level->tileH)) - 1, 0);
    const int x1 = std::min(static_cast<int>(std::ceil(bottomRight.x / level->tileW)) + 1, level->width);
    const int y1 = std::min(static_cast<int>(std::ceil(bottomRight.y / level->tileH)) + 1, level->height);

    for (const TileLayer &layer : level->tileLayers) {
        if (!layer.visible) continue;
        const ui::Color tint{255, 255, 255,
                             static_cast<u8>(std::lround(std::clamp(layer.opacity, 0.0f, 1.0f) * 255.0f))};
        for (int y = y0; y < y1; ++y) {
            for (int x = x0; x < x1; ++x) {
                const u32 gid = level->tileAt(layer, x, y);
                if (gid == 0) continue;
                TileSet *tileset = FindTileSetForGid(*level, gid);
                if (tileset == nullptr) continue;
                ensureTileTexture(*tileset);
                const auto found = m_textures.find(tileset->image);
                if (found == m_textures.end() || !found->second.texture.isValid()) continue;
                const auto &texture = found->second;
                const int columns = std::max(texture.columns, 1);
                const int local = static_cast<int>(gid) - tileset->firstGid;
                const int sx = tileset->margin + (local % columns) * (tileset->tileW + tileset->spacing);
                const int sy = tileset->margin + (local / columns) * (tileset->tileH + tileset->spacing);
                const vec2 screen = screenFromWorld({static_cast<f32>(x) * level->tileW,
                                                     static_cast<f32>(y) * level->tileH});
                m_renderer.imageSub({screen.x, screen.y, level->tileW * m_zoom,
                                     level->tileH * m_zoom},
                                    texture.texture, sx, sy, tileset->tileW, tileset->tileH, tint);
            }
        }
    }

    if (m_showGrid) {
        const f32 stepX = level->tileW * m_zoom;
        const f32 stepY = level->tileH * m_zoom;
        if (stepX >= 6.0f && stepY >= 6.0f) {
            const vec2 origin = screenFromWorld({0.0f, 0.0f});
            for (int x = 0; x <= level->width; ++x) {
                const f32 sx = origin.x + x * stepX;
                if (sx < rect.x || sx > rect.x + rect.w) continue;
                const ui::Color color = (x % 8 == 0) ? m_theme.gridMajor : m_theme.grid;
                m_renderer.line({sx, rect.y}, {sx, rect.y + rect.h}, 1.0f, color);
            }
            for (int y = 0; y <= level->height; ++y) {
                const f32 sy = origin.y + y * stepY;
                if (sy < rect.y || sy > rect.y + rect.h) continue;
                const ui::Color color = (y % 8 == 0) ? m_theme.gridMajor : m_theme.grid;
                m_renderer.line({rect.x, sy}, {rect.x + rect.w, sy}, 1.0f, color);
            }
        }
    }

    for (const ObjectLayer &layer : level->objectLayers) {
        if (!layer.visible) continue;
        for (const Entity &entity : layer.entities) {
            const vec2 screen = screenFromWorld({entity.x, entity.y});
            const ui::Rect box{screen.x, screen.y, entity.w * m_zoom, entity.h * m_zoom};
            ui::Color color{120, 150, 190, 255};
            if (const EntityDef *def = FindEntityDef(m_project, entity.def)) color = def->color;
            m_renderer.rect(box, ui::Color{color.r, color.g, color.b, 70});
            m_renderer.rectOutline(box, color, 1.5f);
            if (entity.id == m_selectedEntityId) {
                m_renderer.rectOutline(box.grown(2.0f), m_theme.accentHover, 2.0f);
                m_renderer.rect({box.x - 3.0f, box.y - 3.0f, 6.0f, 6.0f}, m_theme.accentHover);
                m_renderer.rect({box.x + box.w - 3.0f, box.y + box.h - 3.0f, 6.0f, 6.0f},
                                m_theme.accentHover);
            }
            if (box.w > 38.0f && box.h > 16.0f) {
                m_renderer.textRect(box.inset(3.0f), entity.attrs.count("Name") != 0
                                                        ? entity.attrs.at("Name")
                                                        : entity.def,
                                    m_theme.fontSmall, m_theme.text);
            }
        }
    }

    // Vorschau für Rechteck/Linie und Hover-Zelle.
    if (m_stroking && (m_tool == Tool::Rect || m_tool == Tool::Line)) {
        const int px0 = std::min(m_strokeStart.x, m_strokeEnd.x);
        const int py0 = std::min(m_strokeStart.y, m_strokeEnd.y);
        const int px1 = std::max(m_strokeStart.x, m_strokeEnd.x);
        const int py1 = std::max(m_strokeStart.y, m_strokeEnd.y);
        if (m_tool == Tool::Rect) {
            const vec2 a = screenFromWorld({px0 * level->tileW, py0 * level->tileH});
            m_renderer.rectOutline({a.x, a.y, (px1 - px0 + 1) * level->tileW * m_zoom,
                                    (py1 - py0 + 1) * level->tileH * m_zoom},
                                   m_theme.accentHover, 2.0f);
        } else {
            const vec2 a = screenFromWorld({m_strokeStart.x * level->tileW + level->tileW * 0.5f,
                                            m_strokeStart.y * level->tileH + level->tileH * 0.5f});
            const vec2 b = screenFromWorld({m_strokeEnd.x * level->tileW + level->tileW * 0.5f,
                                            m_strokeEnd.y * level->tileH + level->tileH * 0.5f});
            m_renderer.line(a, b, 2.0f, m_theme.accentHover);
        }
    } else if (hovered) {
        const ivec2 cell = cellFromScreen(m_input.mouse);
        if (cell.x >= 0 && cell.y >= 0 && cell.x < level->width && cell.y < level->height) {
            const vec2 screen = screenFromWorld({cell.x * level->tileW, cell.y * level->tileH});
            m_renderer.rectOutline({screen.x, screen.y, level->tileW * m_zoom, level->tileH * m_zoom},
                                   m_theme.accentHover, 1.5f);
        }
    }

    if (m_showMinimap) {
        const ui::Rect map = minimap;
        m_renderer.rect(map, ui::Color{18, 20, 25, 230});
        m_renderer.rectOutline(map, m_theme.border, 1.0f);
        const f32 scale = std::min(map.w / std::max(level->width * level->tileW, 1),
                                   map.h / std::max(level->height * level->tileH, 1));
        const f32 offsetX = map.x + (map.w - level->width * level->tileW * scale) * 0.5f;
        const f32 offsetY = map.y + (map.h - level->height * level->tileH * scale) * 0.5f;
        for (const TileLayer &layer : level->tileLayers) {
            if (!layer.visible) continue;
            for (int y = 0; y < level->height; y += 2) {
                for (int x = 0; x < level->width; x += 2) {
                    if (level->tileAt(layer, x, y) == 0) continue;
                    m_renderer.rect({offsetX + x * level->tileW * scale,
                                     offsetY + y * level->tileH * scale,
                                     std::max(level->tileW * scale * 2.0f, 1.0f),
                                     std::max(level->tileH * scale * 2.0f, 1.0f)},
                                    ui::Color{110, 130, 160, 180});
                }
            }
        }
        for (const ObjectLayer &layer : level->objectLayers) {
            if (!layer.visible) continue;
            for (const Entity &entity : layer.entities) {
                ui::Color color{120, 150, 190, 255};
                if (const EntityDef *def = FindEntityDef(m_project, entity.def)) color = def->color;
                m_renderer.rect({offsetX + entity.x * scale, offsetY + entity.y * scale,
                                 std::max(entity.w * scale, 2.0f), std::max(entity.h * scale, 2.0f)},
                                color);
            }
        }
        const vec2 viewTop = worldFromScreen({rect.x, rect.y});
        const vec2 viewBottom = worldFromScreen({rect.x + rect.w, rect.y + rect.h});
        m_renderer.rectOutline({offsetX + viewTop.x * scale, offsetY + viewTop.y * scale,
                                (viewBottom.x - viewTop.x) * scale, (viewBottom.y - viewTop.y) * scale},
                               m_theme.accentHover, 1.0f);
        if (m_input.pressed[0] && map.contains(m_input.mouse)) {
            m_cameraX = (m_input.mouse.x - offsetX) / scale;
            m_cameraY = (m_input.mouse.y - offsetY) / scale;
        }
    }

    m_renderer.popClip();
}

// ── Inspector ───────────────────────────────────────────────────────────────

void EditorApp::drawInspector(const ui::Rect &rect) {
    m_renderer.textRect({rect.x + 8.0f, rect.y + 4.0f, rect.w - 16.0f, 22.0f}, "Inspector",
                        m_theme.fontLarge, m_theme.text);
    const ui::Rect content = m_ui.beginScroll({rect.x + 4.0f, rect.y + 28.0f, rect.w - 8.0f,
                                               rect.h - 32.0f},
                                              1600.0f);
    f32 y = content.y + 4.0f;
    Level *level = activeLevel();
    if (level == nullptr) {
        m_renderer.textRect({content.x + 6.0f, y, content.w - 12.0f, 20.0f},
                            "Kein Level geöffnet.", m_theme.fontSmall, m_theme.textDim);
        m_ui.endScroll();
        return;
    }

    Entity *selected = nullptr;
    if (m_selectedEntityId >= 0) {
        for (ObjectLayer &layer : level->objectLayers) {
            for (Entity &entity : layer.entities) {
                if (entity.id == m_selectedEntityId) {
                    selected = &entity;
                    break;
                }
            }
            if (selected != nullptr) break;
        }
    }

    bool changed = false;
    if (selected != nullptr) {
        m_renderer.textRect({content.x + 6.0f, y, content.w - 12.0f, 20.0f},
                            "Entity #" + std::to_string(selected->id) + " · " + selected->def,
                            m_theme.fontNormal, m_theme.text);
        y += 24.0f;
        if (m_ui.dragFloat({content.x + 6.0f, y, (content.w - 18.0f) * 0.5f, 24.0f}, "X",
                           selected->x, 1.0f, -100000.0f, 100000.0f, 1)) changed = true;
        if (m_ui.dragFloat({content.x + 12.0f + (content.w - 18.0f) * 0.5f, y,
                            (content.w - 18.0f) * 0.5f, 24.0f}, "Y", selected->y, 1.0f,
                           -100000.0f, 100000.0f, 1)) changed = true;
        y += 28.0f;
        if (m_ui.dragFloat({content.x + 6.0f, y, (content.w - 18.0f) * 0.5f, 24.0f}, "Breite",
                           selected->w, 1.0f, 1.0f, 100000.0f, 1)) changed = true;
        if (m_ui.dragFloat({content.x + 12.0f + (content.w - 18.0f) * 0.5f, y,
                            (content.w - 18.0f) * 0.5f, 24.0f}, "Höhe", selected->h, 1.0f,
                           1.0f, 100000.0f, 1)) changed = true;
        y += 28.0f;
        if (m_ui.dragFloat({content.x + 6.0f, y, content.w - 12.0f, 24.0f}, "Rotation",
                           selected->rotation, 1.0f, -360.0f, 360.0f, 1)) changed = true;
        y += 34.0f;
        m_renderer.line({content.x + 6.0f, y}, {content.x + content.w - 6.0f, y}, 1.0f,
                        m_theme.border);
        y += 8.0f;
        m_renderer.textRect({content.x + 6.0f, y, content.w - 12.0f, 18.0f}, "Attribute",
                            m_theme.fontSmall, m_theme.textDim);
        y += 22.0f;
        const EntityDef *def = FindEntityDef(m_project, selected->def);
        if (def != nullptr) {
            for (const AttributeDef &attribute : def->attributes) {
                if (selected->attrs.count(attribute.name) == 0) {
                    selected->attrs[attribute.name] = attribute.defaultValue;
                }
                std::string &value = selected->attrs[attribute.name];
                m_renderer.textRect({content.x + 6.0f, y, content.w - 12.0f, 16.0f},
                                    attribute.name, m_theme.fontSmall, m_theme.textDim);
                y += 18.0f;
                const ui::Rect field{content.x + 6.0f, y, content.w - 12.0f, 24.0f};
                if (attribute.type == "bool") {
                    bool flag = value == "true";
                    if (m_ui.checkbox(field, "aktiv", flag)) {
                        value = flag ? "true" : "false";
                        changed = true;
                    }
                } else if (attribute.type == "int") {
                    int number = std::atoi(value.c_str());
                    if (m_ui.dragInt(field, attribute.name, number, -1000000, 1000000)) {
                        value = std::to_string(number);
                        changed = true;
                    }
                } else if (attribute.type == "float") {
                    f32 number = static_cast<f32>(std::atof(value.c_str()));
                    if (m_ui.dragFloat(field, attribute.name, number, 0.1f, -1000000.0f,
                                       1000000.0f, 3)) {
                        value = std::to_string(number);
                        changed = true;
                    }
                } else if (attribute.type == "enum" && !attribute.values.empty()) {
                    int index = 0;
                    for (std::size_t i = 0; i < attribute.values.size(); ++i) {
                        if (attribute.values[i] == value) index = static_cast<int>(i);
                    }
                    m_ui.combo(field, attribute.name, attribute.values, index);
                    if (index >= 0 && index < static_cast<int>(attribute.values.size()) &&
                        attribute.values[static_cast<std::size_t>(index)] != value) {
                        value = attribute.values[static_cast<std::size_t>(index)];
                        changed = true;
                    }
                } else {
                    if (m_ui.textField(field, attribute.name, value)) changed = true;
                }
                y += 30.0f;
            }
        } else {
            m_renderer.textRect({content.x + 6.0f, y, content.w - 12.0f, 20.0f},
                                "Keine Definition für '" + selected->def + "'.",
                                m_theme.fontSmall, m_theme.textFaint);
            y += 24.0f;
        }
    } else if (m_layerSelectionIsObject && !level->objectLayers.empty()) {
        m_activeObjectLayer = std::clamp(m_activeObjectLayer, 0,
                                         static_cast<int>(level->objectLayers.size()) - 1);
        ObjectLayer &layer = level->objectLayers[static_cast<std::size_t>(m_activeObjectLayer)];
        m_renderer.textRect({content.x + 6.0f, y, content.w - 12.0f, 20.0f},
                            "Objektebene", m_theme.fontNormal, m_theme.text);
        y += 24.0f;
        if (m_ui.textField({content.x + 6.0f, y, content.w - 12.0f, 24.0f}, "Name", layer.name))
            changed = true;
        y += 30.0f;
        if (m_ui.checkbox({content.x + 6.0f, y, content.w - 12.0f, 22.0f}, "sichtbar",
                          layer.visible)) changed = true;
        y += 26.0f;
        if (m_ui.checkbox({content.x + 6.0f, y, content.w - 12.0f, 22.0f}, "gesperrt",
                          layer.locked)) changed = true;
        y += 30.0f;
        m_renderer.textRect({content.x + 6.0f, y, content.w - 12.0f, 18.0f},
                            std::to_string(layer.entities.size()) + " Entities",
                            m_theme.fontSmall, m_theme.textDim);
        y += 26.0f;
    } else if (!level->tileLayers.empty()) {
        m_activeTileLayer = std::clamp(m_activeTileLayer, 0,
                                       static_cast<int>(level->tileLayers.size()) - 1);
        TileLayer &layer = level->tileLayers[static_cast<std::size_t>(m_activeTileLayer)];
        m_renderer.textRect({content.x + 6.0f, y, content.w - 12.0f, 20.0f},
                            "Kachelebene", m_theme.fontNormal, m_theme.text);
        y += 24.0f;
        if (m_ui.textField({content.x + 6.0f, y, content.w - 12.0f, 24.0f}, "Name", layer.name))
            changed = true;
        y += 30.0f;
        if (m_ui.checkbox({content.x + 6.0f, y, content.w - 12.0f, 22.0f}, "sichtbar",
                          layer.visible)) changed = true;
        y += 26.0f;
        if (m_ui.checkbox({content.x + 6.0f, y, content.w - 12.0f, 22.0f}, "gesperrt",
                          layer.locked)) changed = true;
        y += 26.0f;
        if (m_ui.slider({content.x + 6.0f, y, content.w - 12.0f, 34.0f}, "Deckkraft",
                         layer.opacity, 0.0f, 1.0f)) changed = true;
        y += 40.0f;
    } else {
        m_renderer.textRect({content.x + 6.0f, y, content.w - 12.0f, 20.0f}, "Level",
                            m_theme.fontNormal, m_theme.text);
        y += 24.0f;
        if (m_ui.textField({content.x + 6.0f, y, content.w - 12.0f, 24.0f}, "Name", level->name))
            changed = true;
        y += 30.0f;
        int width = level->width;
        int height = level->height;
        if (m_ui.dragInt({content.x + 6.0f, y, (content.w - 18.0f) * 0.5f, 24.0f}, "Breite", width,
                         1, 4096)) {
            ResizeLevel(*level, width, level->height);
            changed = true;
        }
        if (m_ui.dragInt({content.x + 12.0f + (content.w - 18.0f) * 0.5f, y,
                          (content.w - 18.0f) * 0.5f, 24.0f}, "Höhe", height, 1, 4096)) {
            ResizeLevel(*level, level->width, height);
            changed = true;
        }
        y += 30.0f;
        int tileW = level->tileW;
        int tileH = level->tileH;
        if (m_ui.dragInt({content.x + 6.0f, y, (content.w - 18.0f) * 0.5f, 24.0f}, "Kachel-B",
                          tileW, 1, 512)) {
            level->tileW = tileW;
            changed = true;
        }
        if (m_ui.dragInt({content.x + 12.0f + (content.w - 18.0f) * 0.5f, y,
                          (content.w - 18.0f) * 0.5f, 24.0f}, "Kachel-H", tileH, 1, 512)) {
            level->tileH = tileH;
            changed = true;
        }
        y += 34.0f;
    }

    m_renderer.line({content.x + 6.0f, y}, {content.x + content.w - 6.0f, y}, 1.0f,
                    m_theme.border);
    y += 8.0f;
    m_renderer.textRect({content.x + 6.0f, y, content.w - 12.0f, 18.0f}, "Ebenen",
                        m_theme.fontSmall, m_theme.textDim);
    y += 22.0f;
    const f32 half = (content.w - 18.0f) * 0.5f;
    if (m_ui.button({content.x + 6.0f, y, half, 24.0f}, "+ Kacheln")) addTileLayer();
    if (m_ui.button({content.x + 12.0f + half, y, half, 24.0f}, "+ Objekte")) addObjectLayer();
    y += 28.0f;
    if (m_ui.button({content.x + 6.0f, y, half, 24.0f}, "Duplizieren")) duplicateLayer();
    if (m_ui.button({content.x + 12.0f + half, y, half, 24.0f}, "Löschen", true, false)) deleteLayer();
    y += 28.0f;
    if (m_ui.button({content.x + 6.0f, y, half, 24.0f}, "Nach oben")) moveLayer(-1);
    if (m_ui.button({content.x + 12.0f + half, y, half, 24.0f}, "Nach unten")) moveLayer(1);
    y += 32.0f;

    m_ui.endScroll();

    if (changed) {
        markDirty();
        commitHistory();
    }
}

// ── Projekt und Dateien ─────────────────────────────────────────────────────

void EditorApp::newProject(const fs::path &root, const std::string &name) {
    if (name.empty()) return;
    std::error_code ec;
    const fs::path directory = root / PathFromUtf8(name);
    fs::create_directories(directory, ec);
    if (ec) {
        log("Projektverzeichnis konnte nicht angelegt werden: " + ec.message());
        return;
    }
    m_project = CreateProject(name, directory);
    m_documents.clear();
    m_active = -1;
    m_textures.clear();
    m_textureFailed.clear();

    Level level = CreateLevel(60, 34, 32, 32, "level01");
    level.tilesets = m_project.tilesets;
    Document doc;
    doc.level = std::move(level);
    doc.path = directory / "levels" / "level01.tmj";
    doc.title = "level01";
    doc.history.reset(doc.level);
    m_documents.push_back(std::move(doc));
    m_active = 0;
    m_project.levels = {"levels/level01.tmj"};
    m_cameraX = m_documents[0].level.width * m_documents[0].level.tileW * 0.5f;
    m_cameraY = m_documents[0].level.height * m_documents[0].level.tileH * 0.5f;
    m_zoom = 1.0f;
    if (!m_project.entityDefs.empty()) m_selectedEntityDef = m_project.entityDefs.front().name;
    if (!m_documents[0].level.tilesets.empty()) {
        m_selectedTile = static_cast<u32>(m_documents[0].level.tilesets.front().firstGid);
    }
    saveProject();
    log("Projekt angelegt: " + name);
}

bool EditorApp::saveProject() {
    if (m_project.root.empty()) return false;
    std::string error;
    for (Document &doc : m_documents) {
        if (doc.path.empty()) {
            doc.path = m_project.root / "levels" / (doc.title + ".tmj");
        }
        if (!ExportTiled(doc.path, doc.level, m_project, error)) {
            log("Speichern fehlgeschlagen: " + error);
            return false;
        }
        doc.dirty = false;
    }
    m_project.levels.clear();
    for (const Document &doc : m_documents) {
        m_project.levels.push_back(RelativeLevelPath(m_project, doc.path));
    }
    if (!SaveProject(m_project, m_project.root / "project.sglproj", error)) {
        log("Projektdatei: " + error);
        return false;
    }
    m_projectDirty = false;
    m_status = "Gespeichert: " + PathToUtf8(m_project.root);
    log("Projekt gespeichert.");
    return true;
}

bool EditorApp::saveProjectAs(const fs::path &file) {
    if (file.empty()) return false;
    m_project.root = file.parent_path();
    return saveProject();
}

bool EditorApp::saveActiveLevel() {
    Document *doc = activeDocument();
    if (doc == nullptr) return false;
    std::string error;
    if (!ExportTiled(doc->path, doc->level, m_project, error)) {
        log("Level speichern: " + error);
        return false;
    }
    doc->dirty = false;
    return true;
}

bool EditorApp::openProject(const fs::path &file) {
    Project project;
    std::string error;
    if (!LoadProject(file, project, error)) {
        log(error);
        return false;
    }
    m_project = std::move(project);
    m_project.root = file.parent_path();
    m_documents.clear();
    m_textures.clear();
    m_textureFailed.clear();
    for (const std::string &levelPath : m_project.levels) {
        const fs::path absolute = ResolveAsset(m_project, levelPath);
        Level level;
        if (!ImportTiled(absolute, level, m_project, error)) {
            log("Level '" + levelPath + "': " + error);
            continue;
        }
        Document doc;
        doc.level = std::move(level);
        doc.path = absolute;
        doc.title = FileStem(absolute);
        doc.history.reset(doc.level);
        m_documents.push_back(std::move(doc));
    }
    m_active = m_documents.empty() ? -1 : 0;
    if (m_active >= 0) {
        m_cameraX = m_documents[0].level.width * m_documents[0].level.tileW * 0.5f;
        m_cameraY = m_documents[0].level.height * m_documents[0].level.tileH * 0.5f;
    }
    m_zoom = 1.0f;
    if (!m_project.entityDefs.empty() && m_selectedEntityDef.empty()) {
        m_selectedEntityDef = m_project.entityDefs.front().name;
    }
    if (!m_documents.empty() && m_selectedTile == 0 && !m_documents[0].level.tilesets.empty()) {
        m_selectedTile = static_cast<u32>(m_documents[0].level.tilesets.front().firstGid);
    }
    log("Projekt geöffnet: " + m_project.name + " (" +
        std::to_string(m_documents.size()) + " Levels)");
    return true;
}

void EditorApp::newLevel() {
    if (m_project.root.empty()) {
        log("Bitte zuerst ein Projekt anlegen oder öffnen.");
        return;
    }
    int index = 1;
    std::string name;
    do {
        name = std::string("level") + (index < 10 ? "0" : "") + std::to_string(index);
        ++index;
    } while (std::any_of(m_documents.begin(), m_documents.end(),
                         [&](const Document &doc) { return doc.title == name; }));
    Level level = CreateLevel(60, 34, 32, 32, name);
    level.tilesets = m_project.tilesets;
    Document doc;
    doc.level = std::move(level);
    doc.path = m_project.root / "levels" / (name + ".tmj");
    doc.title = name;
    doc.history.reset(doc.level);
    m_documents.push_back(std::move(doc));
    m_active = static_cast<int>(m_documents.size()) - 1;
    m_cameraX = m_documents.back().level.width * m_documents.back().level.tileW * 0.5f;
    m_cameraY = m_documents.back().level.height * m_documents.back().level.tileH * 0.5f;
    saveProject();
    log("Neues Level: " + name);
}

void EditorApp::closeDocument(int index) {
    if (index < 0 || index >= static_cast<int>(m_documents.size())) return;
    m_documents.erase(m_documents.begin() + index);
    if (m_active >= static_cast<int>(m_documents.size())) m_active = static_cast<int>(m_documents.size()) - 1;
    m_selectedEntityId = -1;
}

bool EditorApp::openLevelFile(const fs::path &file, bool asImport) {
    Level level;
    std::string error;
    if (!ImportTiled(file, level, m_project, error)) {
        log(error);
        return false;
    }
    if (m_project.root.empty()) {
        log("Bitte zuerst ein Projekt öffnen, damit das Level gespeichert werden kann.");
        return false;
    }
    const std::string title = FileStem(file);
    fs::path target = file;
    if (asImport) {
        target = m_project.root / "levels" / (title + ".tmj");
        if (!ExportTiled(target, level, m_project, error)) {
            log("Import speichern: " + error);
            return false;
        }
    }
    Document doc;
    doc.level = std::move(level);
    doc.path = target;
    doc.title = title;
    doc.history.reset(doc.level);
    m_documents.push_back(std::move(doc));
    m_active = static_cast<int>(m_documents.size()) - 1;
    m_cameraX = m_documents.back().level.width * m_documents.back().level.tileW * 0.5f;
    m_cameraY = m_documents.back().level.height * m_documents.back().level.tileH * 0.5f;
    m_zoom = 1.0f;
    m_projectDirty = true;
    log((asImport ? "Level importiert: " : "Level geöffnet: ") + title);
    return true;
}

bool EditorApp::importTiled(const fs::path &file) {
    return openLevelFile(file, true);
}

bool EditorApp::exportTiled(const fs::path &file) {
    Document *doc = activeDocument();
    if (doc == nullptr) {
        log("Kein Level geöffnet.");
        return false;
    }
    std::string error;
    if (!ExportTiled(file, doc->level, m_project, error)) {
        log(error);
        return false;
    }
    log("Exportiert: " + PathToUtf8(file));
    return true;
}

void EditorApp::importTileset(const fs::path &file) {
    Document *doc = activeDocument();
    if (doc == nullptr) {
        log("Bitte zuerst ein Level öffnen.");
        return;
    }
    const auto image = sgl::ImageIO::TryLoad(file);
    if (!image) {
        log(std::string("Bild konnte nicht geladen werden: ") + image.error().what());
        return;
    }
    if (m_project.root.empty()) {
        m_project.root = fs::current_path();
    }
    TileSet tileset;
    tileset.name = FileStem(file);
    tileset.image = MakeProjectRelativeSafe(m_project, file);
    tileset.tileW = std::max(doc->level.tileW, 1);
    tileset.tileH = std::max(doc->level.tileH, 1);
    tileset.margin = 0;
    tileset.spacing = 0;
    tileset.columns = std::max((image->width() + tileset.spacing) /
                                   (tileset.tileW + tileset.spacing),
                               1);
    tileset.tileCount = tileset.columns *
                        std::max((image->height() + tileset.spacing) /
                                     (tileset.tileH + tileset.spacing),
                                 1);
    int nextGid = 1;
    for (const TileSet &existing : m_project.tilesets) {
        nextGid = std::max(nextGid, existing.firstGid + std::max(existing.tileCount, 1));
    }
    tileset.firstGid = nextGid;
    m_project.tilesets.push_back(tileset);
    doc->level.tilesets.push_back(tileset);
    m_selectedTile = static_cast<u32>(tileset.firstGid);
    m_activeTileset = static_cast<int>(doc->level.tilesets.size()) - 1;
    doc->dirty = true;
    m_projectDirty = true;
    commitHistory();
    log("Tileset importiert: " + tileset.name + " (" + std::to_string(tileset.tileCount) +
        " Kacheln)");
}

void EditorApp::ensureTileTexture(const TileSet &tileset) {
    if (tileset.image.empty()) return;
    if (m_textures.count(tileset.image) != 0 || m_textureFailed.count(tileset.image) != 0) return;
    const auto image = sgl::ImageIO::TryLoad(ResolveAsset(m_project, tileset.image));
    if (!image) {
        m_textureFailed[tileset.image] = true;
        log("Tileset-Bild fehlt: " + tileset.image);
        return;
    }
    m_renderer.flush();
    TileTexture entry;
    entry.texture = ui::Texture::fromImage(*image, true);
    entry.tileW = tileset.tileW;
    entry.tileH = tileset.tileH;
    entry.margin = tileset.margin;
    entry.spacing = tileset.spacing;
    entry.columns = tileset.columns > 0
                        ? tileset.columns
                        : std::max((image->width() + tileset.spacing) /
                                       (tileset.tileW + tileset.spacing),
                                   1);
    entry.tileCount = tileset.tileCount > 0
                          ? tileset.tileCount
                          : entry.columns * std::max((image->height() + tileset.spacing) /
                                                         (tileset.tileH + tileset.spacing),
                                                     1);
    m_textures.emplace(tileset.image, std::move(entry));
}


// ── Dialoge ─────────────────────────────────────────────────────────────────

void EditorApp::openDialog(DialogKind kind, std::string heading, std::string extension,
                           const fs::path &directory, std::string name) {
    m_dialog = {};
    m_dialog.kind = kind;
    m_dialog.heading = std::move(heading);
    m_dialog.extension = std::move(extension);
    m_dialog.directory = directory.empty() ? fs::current_path() : directory;
    if (!fs::is_directory(m_dialog.directory)) m_dialog.directory = fs::current_path();
    m_dialog.name = std::move(name);
    m_ui.clearFocus();
    refreshDialogEntries();
}

void EditorApp::refreshDialogEntries() {
    m_dialog.entries.clear();
    std::error_code ec;
    for (const auto &entry : fs::directory_iterator(m_dialog.directory, ec)) {
        if (ec) break;
        const std::string name = PathToUtf8(entry.path().filename());
        if (entry.is_directory(ec)) {
            m_dialog.entries.push_back(name + "/");
        } else if (entry.is_regular_file(ec) && HasExtension(name, m_dialog.extension)) {
            m_dialog.entries.push_back(name);
        }
    }
    std::sort(m_dialog.entries.begin(), m_dialog.entries.end(),
              [](const std::string &a, const std::string &b) {
                  const bool aDir = !a.empty() && a.back() == '/';
                  const bool bDir = !b.empty() && b.back() == '/';
                  if (aDir != bDir) return aDir;
                  return a < b;
              });
}

void EditorApp::cancelDialog() {
    m_dialog = {};
    m_ui.clearFocus();
}

void EditorApp::confirmDialog() {
    if (m_dialog.name.empty()) {
        log("Bitte einen Namen angeben.");
        return;
    }
    const DialogKind kind = m_dialog.kind;
    const fs::path directory = m_dialog.directory;
    std::string name = m_dialog.name;
    if (kind == DialogKind::OpenProject || kind == DialogKind::SaveProjectAs) {
        name = EnsureExtension(name, ".sglproj");
    } else if (kind == DialogKind::ExportTiled) {
        name = EnsureExtension(name, ".tmj");
    }
    const fs::path target = directory / PathFromUtf8(name);
    cancelDialog();
    switch (kind) {
    case DialogKind::NewProject:
        newProject(directory, name);
        break;
    case DialogKind::OpenProject:
        openProject(target);
        break;
    case DialogKind::SaveProjectAs:
        saveProjectAs(target);
        break;
    case DialogKind::ImportTiled:
        importTiled(target);
        break;
    case DialogKind::ExportTiled:
        exportTiled(target);
        break;
    case DialogKind::ImportTileset:
        importTileset(target);
        break;
    case DialogKind::None:
        break;
    }
}

void EditorApp::openProjectDialog() {
    openDialog(DialogKind::OpenProject, "Projekt öffnen", ".sglproj", fs::current_path(), "");
}

void EditorApp::saveProjectAsDialog() {
    const std::string name = m_project.name.empty() ? "project.sglproj" : m_project.name + ".sglproj";
    openDialog(DialogKind::SaveProjectAs, "Projekt speichern unter", ".sglproj",
               m_project.root.empty() ? fs::current_path() : m_project.root, name);
}

void EditorApp::importDialog() {
    openDialog(DialogKind::ImportTiled, "Level importieren", "", fs::current_path(), "");
}

void EditorApp::exportDialog() {
    const Document *doc = activeDocument();
    const std::string name = doc != nullptr ? doc->title + ".tmj" : "level.tmj";
    openDialog(DialogKind::ExportTiled, "Level exportieren", "",
               m_project.root.empty() ? fs::current_path() : m_project.root, name);
}

void EditorApp::importTilesetDialog() {
    openDialog(DialogKind::ImportTileset, "Tileset importieren", "", fs::current_path(), "");
}

void EditorApp::drawDialog() {
    if (m_dialog.kind == DialogKind::None) return;
    m_renderer.rect({0.0f, 0.0f, static_cast<f32>(m_windowSize.x),
                     static_cast<f32>(m_windowSize.y)},
                    ui::Color{0, 0, 0, 170});
    const f32 width = 640.0f;
    const f32 height = 470.0f;
    const ui::Rect panel{(m_windowSize.x - width) * 0.5f, (m_windowSize.y - height) * 0.5f,
                         width, height};
    m_renderer.rect(panel, m_theme.panel);
    m_renderer.rectOutline(panel, m_theme.border, 1.0f);
    m_renderer.rect(panel.top(30.0f), m_theme.header);
    m_renderer.textRect(panel.top(30.0f).inset(10.0f, 0.0f), m_dialog.heading,
                        m_theme.fontLarge, m_theme.text);

    const f32 left = panel.x + 12.0f;
    const f32 right = panel.x + panel.w - 12.0f;
    f32 y = panel.y + 38.0f;
    m_renderer.textRect({left, y, right - left - 120.0f, 20.0f},
                        PathToUtf8(m_dialog.directory), m_theme.fontSmall, m_theme.textDim);
    if (m_ui.button({right - 110.0f, y - 2.0f, 110.0f, 22.0f}, "Übergeordnet")) {
        const fs::path parent = m_dialog.directory.parent_path();
        if (!parent.empty() && parent != m_dialog.directory) {
            m_dialog.directory = parent;
            refreshDialogEntries();
        }
    }
    y += 26.0f;

    const ui::Rect list{left, y, right - left, 300.0f};
    m_renderer.rect(list, m_theme.window);
    m_renderer.rectOutline(list, m_theme.border, 1.0f);
    const ui::Rect content = m_ui.beginScroll({list.x + 2.0f, list.y + 2.0f, list.w - 4.0f,
                                               list.h - 4.0f},
                                              static_cast<f32>(m_dialog.entries.size()) * 22.0f + 4.0f);
    f32 rowY = content.y + 2.0f;
    for (const std::string &entry : m_dialog.entries) {
        m_ui.pushId(entry);
        const bool directory = !entry.empty() && entry.back() == '/';
        const std::string label = directory ? entry.substr(0, entry.size() - 1) : entry;
        const ui::Rect row{content.x + 2.0f, rowY, content.w - 8.0f, 22.0f};
        if (m_ui.selectable(row, (directory ? "[D] " : "    ") + label, entry == m_dialog.name)) {
            if (directory) {
                m_dialog.directory /= PathFromUtf8(label);
                m_ui.popId();
                refreshDialogEntries();
                break;
            }
            m_dialog.name = entry;
            if (m_input.doubleClick[0]) {
                m_ui.popId();
                confirmDialog();
                return;
            }
        }
        m_ui.popId();
        rowY += 22.0f;
    }
    m_ui.endScroll();

    y = panel.y + panel.h - 72.0f;
    m_renderer.textRect({left, y, 70.0f, 22.0f}, "Name", m_theme.fontSmall, m_theme.textDim);
    if (m_ui.textField({left + 64.0f, y, right - left - 64.0f, 24.0f}, "Dateiname",
                       m_dialog.name)) {
    }
    y += 32.0f;
    if (m_ui.button({right - 216.0f, y, 100.0f, 26.0f}, "Abbrechen")) cancelDialog();
    if (m_ui.button({right - 108.0f, y, 100.0f, 26.0f}, "OK", true, true)) confirmDialog();
    if (m_input.keyPressed(sgl::Key::Enter)) confirmDialog();
}

} // namespace editor
