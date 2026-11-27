#include "EditorApp.hpp"

#include "doc/JsonWriter.hpp"

#include "common/Json.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace editor {
namespace {

constexpr int kDockLeft = 0;
constexpr int kDockRight = 1;
constexpr int kDockBottom = 2;

struct PanelNameEntry {
    EditorApp::Panel panel;
    const char *key;
    const char *title;
};

constexpr PanelNameEntry kPanelNames[] = {
    {EditorApp::Panel::Project, "project", "Projekt"},
    {EditorApp::Panel::Palette, "palette", "Kacheln"},
    {EditorApp::Panel::Entities, "entities", "Entities"},
    {EditorApp::Panel::Inspector, "inspector", "Inspector"},
    {EditorApp::Panel::Layers, "layers", "Ebenen"},
    {EditorApp::Panel::Console, "console", "Konsole"},
    {EditorApp::Panel::Assets, "assets", "Assets"},
};

int DefaultDock(EditorApp::Panel panel) {
    switch (panel) {
    case EditorApp::Panel::Project:
    case EditorApp::Panel::Palette:
    case EditorApp::Panel::Entities:
        return kDockLeft;
    case EditorApp::Panel::Inspector:
        return kDockRight;
    case EditorApp::Panel::Layers:
    case EditorApp::Panel::Console:
    case EditorApp::Panel::Assets:
        return kDockBottom;
    }
    return kDockLeft;
}

std::string ReadText(const fs::path &file) {
    std::ifstream input(file, std::ios::binary);
    if (!input) return {};
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

} // namespace

const char *EditorApp::PanelName(Panel panel) {
    for (const auto &entry : kPanelNames) {
        if (entry.panel == panel) return entry.title;
    }
    return "Panel";
}

std::vector<EditorApp::Panel> &EditorApp::dockPanels(int dock) {
    if (dock == kDockRight) return m_rightPanels;
    if (dock == kDockBottom) return m_bottomPanels;
    return m_leftPanels;
}

int &EditorApp::dockActive(int dock) {
    if (dock == kDockRight) return m_rightActive;
    if (dock == kDockBottom) return m_bottomActive;
    return m_leftActive;
}

void EditorApp::resetLayout() {
    m_leftPanels = {Panel::Project, Panel::Palette, Panel::Entities};
    m_rightPanels = {Panel::Inspector};
    m_bottomPanels = {Panel::Layers, Panel::Console, Panel::Assets};
    m_leftActive = 0;
    m_rightActive = 0;
    m_bottomActive = 0;
    m_dragPanel.reset();
    m_draggingPanel = false;
}

void EditorApp::saveLayout() {
    if (m_project.root.empty()) return;
    JsonWriter writer;
    writer.beginObject();
    auto writeList = [&](const char *key, const std::vector<Panel> &panels, int active) {
        writer.key(key);
        writer.beginArray();
        for (const Panel panel : panels) {
            for (const auto &entry : kPanelNames) {
                if (entry.panel == panel) {
                    writer.value(entry.key);
                    break;
                }
            }
        }
        writer.endArray();
        writer.key((std::string(key) + "Active").c_str());
        writer.value(active);
    };
    writeList("left", m_leftPanels, m_leftActive);
    writeList("right", m_rightPanels, m_rightActive);
    writeList("bottom", m_bottomPanels, m_bottomActive);
    writer.endObject();

    const fs::path file = m_project.root / ".sgl" / "layout.json";
    std::error_code ec;
    fs::create_directories(file.parent_path(), ec);
    std::ofstream output(file, std::ios::binary | std::ios::trunc);
    if (!output) return;
    const std::string text = writer.take();
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
}

void EditorApp::loadLayout() {
    resetLayout();
    if (m_project.root.empty()) return;
    const fs::path file = m_project.root / ".sgl" / "layout.json";
    const std::string text = ReadText(file);
    if (text.empty()) return;
    const sgl::detail::Json json = sgl::detail::Json::Parse(text);
    if (!json.isValid() || !json.isObject()) return;

    m_leftPanels.clear();
    m_rightPanels.clear();
    m_bottomPanels.clear();
    auto readList = [&](const char *key, std::vector<Panel> &panels) {
        const sgl::detail::Json *values = json.find(key);
        if (values == nullptr || !values->isArray()) return;
        for (int i = 0; i < values->size(); ++i) {
            const std::string &value = values->at(i).string();
            for (const auto &entry : kPanelNames) {
                if (value == entry.key) {
                    panels.push_back(entry.panel);
                    break;
                }
            }
        }
    };
    readList("left", m_leftPanels);
    readList("right", m_rightPanels);
    readList("bottom", m_bottomPanels);

    // Jedes Panel muss genau einmal vorkommen; fehlende landen im Standarddock.
    auto contains = [&](Panel panel) {
        for (const auto &entry : kPanelNames) {
            if (entry.panel != panel) continue;
            for (const Panel value : m_leftPanels) if (value == panel) return true;
            for (const Panel value : m_rightPanels) if (value == panel) return true;
            for (const Panel value : m_bottomPanels) if (value == panel) return true;
        }
        return false;
    };
    for (const auto &entry : kPanelNames) {
        if (!contains(entry.panel)) dockPanels(DefaultDock(entry.panel)).push_back(entry.panel);
    }
    auto readActive = [&](const char *key, int &active, std::size_t count) {
        const sgl::detail::Json *value = json.find(key);
        active = value != nullptr ? value->intValue(0) : 0;
        if (count == 0) active = 0;
        else active = std::clamp(active, 0, static_cast<int>(count) - 1);
    };
    readActive("leftActive", m_leftActive, m_leftPanels.size());
    readActive("rightActive", m_rightActive, m_rightPanels.size());
    readActive("bottomActive", m_bottomActive, m_bottomPanels.size());
}

void EditorApp::movePanel(Panel panel, int fromDock, int toDock) {
    if (fromDock == toDock) return;
    for (int d = 0; d < 3; ++d) {
        auto &panels = dockPanels(d);
        for (std::size_t i = 0; i < panels.size(); ++i) {
            if (panels[i] != panel) continue;
            if (d != fromDock) return;
            panels.erase(panels.begin() + static_cast<std::ptrdiff_t>(i));
            if (panels.empty()) dockActive(d) = 0;
            else dockActive(d) = std::min(dockActive(d), static_cast<int>(panels.size()) - 1);
            break;
        }
    }
    std::vector<Panel> &target = dockPanels(toDock);
    target.push_back(panel);
    dockActive(toDock) = static_cast<int>(target.size()) - 1;
    saveLayout();
}

void EditorApp::handleDockDrop() {
    if (!m_dragPanel.has_value()) return;
    if (m_input.down[0]) {
        if (!m_draggingPanel) {
            const f32 dx = m_input.mouse.x - m_dragStart.x;
            const f32 dy = m_input.mouse.y - m_dragStart.y;
            if (dx * dx + dy * dy > 25.0f) m_draggingPanel = true;
        }
        return;
    }
    if (m_draggingPanel) {
        for (int dock = 0; dock < 3; ++dock) {
            if (m_dockRects[dock].contains(m_input.mouse)) {
                movePanel(*m_dragPanel, m_dragFromDock, dock);
                break;
            }
        }
    }
    m_dragPanel.reset();
    m_draggingPanel = false;
    m_dragFromDock = -1;
}

void EditorApp::drawPanel(Panel panel, const ui::Rect &rect) {
    m_ui.pushId(PanelName(panel));
    switch (panel) {
    case Panel::Project: drawProjectPanel(rect); break;
    case Panel::Palette: drawPalettePanel(rect); break;
    case Panel::Entities: drawEntityPanel(rect); break;
    case Panel::Inspector: drawInspector(rect); break;
    case Panel::Layers: drawLayersPanel(rect); break;
    case Panel::Console: drawConsolePanel(rect); break;
    case Panel::Assets: drawAssetsPanel(rect); break;
    }
    m_ui.popId();
}

void EditorApp::drawDock(const ui::Rect &rect, std::vector<Panel> &panels, int &active,
                         int dockIndex) {
    m_dockRects[dockIndex] = rect;
    m_dockTabRects[dockIndex] = {rect.x, rect.y, rect.w, 26.0f};
    m_renderer.rect(rect, m_theme.panel);
    m_renderer.rectOutline(rect, m_theme.border, 1.0f);

    if (panels.empty()) {
        m_renderer.textRect(rect.inset(8.0f), "Panel hierher ziehen",
                            m_theme.fontSmall, m_theme.textFaint, 1);
        return;
    }
    active = std::clamp(active, 0, static_cast<int>(panels.size()) - 1);
    const f32 tabWidth =
        std::min(150.0f, (rect.w - 4.0f) / static_cast<f32>(panels.size()));
    f32 x = rect.x + 2.0f;
    for (std::size_t i = 0; i < panels.size(); ++i) {
        const ui::Rect tab{x, rect.y, tabWidth, 26.0f};
        if (m_ui.tab(tab, PanelName(panels[i]), active == static_cast<int>(i))) {
            active = static_cast<int>(i);
        }
        if (tab.contains(m_input.mouse) && m_input.pressed[0] && !m_ui.inputBlocked()) {
            m_dragPanel = panels[i];
            m_dragFromDock = dockIndex;
            m_dragStart = m_input.mouse;
            m_draggingPanel = false;
        }
        x += tabWidth;
        if (x > rect.x + rect.w - 20.0f) break;
    }
    drawPanel(panels[static_cast<std::size_t>(active)],
              {rect.x, rect.y + 26.0f, rect.w, std::max(rect.h - 26.0f, 20.0f)});
}

} // namespace editor
