#pragma once

// In-House-UI des Leveleditors: gebatchter 2D-Renderer (OpenGL 3.3),
// Theme, Immediate-Mode-Widgets und Texteditor.

#include <sgl/Color.hpp>
#include <sgl/Image.hpp>
#include <sgl/Input.hpp>
#include <sgl/MeshFont.hpp>
#include <sgl/Shader.hpp>
#include <sgl/Types.hpp>

#include <array>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace editor::ui {

using Color = sgl::Color;

struct Rect {
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 w = 0.0f;
    f32 h = 0.0f;

    [[nodiscard]] bool contains(vec2 p) const {
        return p.x >= x && p.y >= y && p.x < x + w && p.y < y + h;
    }
    [[nodiscard]] Rect inset(f32 amount) const { return {x + amount, y + amount, w - amount * 2.0f, h - amount * 2.0f}; }
    [[nodiscard]] Rect inset(f32 horizontal, f32 vertical) const {
        return {x + horizontal, y + vertical, w - horizontal * 2.0f, h - vertical * 2.0f};
    }
    [[nodiscard]] Rect left(f32 amount) const { return {x, y, amount, h}; }
    [[nodiscard]] Rect right(f32 amount) const { return {x + w - amount, y, amount, h}; }
    [[nodiscard]] Rect top(f32 amount) const { return {x, y, w, amount}; }
    [[nodiscard]] Rect bottom(f32 amount) const { return {x, y + h - amount, w, amount}; }
    [[nodiscard]] Rect shifted(f32 dx, f32 dy) const { return {x + dx, y + dy, w, h}; }
    [[nodiscard]] Rect grown(f32 amount) const { return {x - amount, y - amount, w + amount * 2.0f, h + amount * 2.0f}; }
    [[nodiscard]] Rect intersect(const Rect& other) const {
        const f32 x0 = x > other.x ? x : other.x;
        const f32 y0 = y > other.y ? y : other.y;
        const f32 x1 = (x + w) < (other.x + other.w) ? (x + w) : (other.x + other.w);
        const f32 y1 = (y + h) < (other.y + other.h) ? (y + h) : (other.y + other.h);
        return {x0, y0, x1 > x0 ? x1 - x0 : 0.0f, y1 > y0 ? y1 - y0 : 0.0f};
    }
};

struct Theme {
    Color window{24, 26, 32, 255};
    Color panel{31, 34, 42, 255};
    Color panelAlt{38, 42, 52, 255};
    Color header{44, 49, 61, 255};
    Color border{55, 61, 75, 255};
    Color text{224, 228, 236, 255};
    Color textDim{150, 157, 172, 255};
    Color textFaint{100, 107, 122, 255};
    Color accent{82, 145, 230, 255};
    Color accentHover{104, 164, 240, 255};
    Color accentDim{52, 94, 150, 255};
    Color selection{62, 110, 170, 255};
    Color danger{210, 90, 90, 255};
    Color success{90, 190, 120, 255};
    Color grid{58, 64, 78, 255};
    Color gridMajor{78, 86, 104, 255};
    Color viewport{20, 22, 27, 255};
    Color checkerA{46, 50, 60, 255};
    Color checkerB{40, 44, 53, 255};
    f32 radius = 4.0f;
    f32 fontSmall = 12.0f;
    f32 fontNormal = 14.0f;
    f32 fontLarge = 18.0f;
};

enum class Icon : u8 {
    None,
    Cursor,
    Brush,
    Rect,
    Line,
    Fill,
    Eraser,
    Picker,
    Entity,
    Pan,
    Undo,
    Redo,
    Save,
    Open,
    Folder,
    Eye,
    EyeOff,
    Lock,
    Unlock,
    Plus,
    Trash,
    Up,
    Down,
    Grid,
    ZoomIn,
    ZoomOut,
    Play,
    Settings,
    Close,
    Check,
    ChevronDown,
};

struct InputState {
    vec2 mouse{};
    vec2 mouseDelta{};
    bool down[3]{};
    bool pressed[3]{};
    bool released[3]{};
    bool doubleClick[3]{};
    f32 wheel = 0.0f;
    bool shift = false;
    bool control = false;
    bool alt = false;
    bool super = false;
    std::vector<u32> chars;
    std::vector<sgl::Key> keyPresses;

    void newFrame();
    [[nodiscard]] bool keyPressed(sgl::Key key) const;
};

struct Texture {
    unsigned int id = 0;
    int width = 0;
    int height = 0;

    Texture() = default;
    ~Texture();
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    void destroy();
    [[nodiscard]] bool isValid() const { return id != 0; }

    static Texture fromImage(const sgl::Image& image, bool pixelArt = true);
};

class Renderer2d {
public:
    Renderer2d() = default;
    ~Renderer2d();
    Renderer2d(const Renderer2d&) = delete;
    Renderer2d& operator=(const Renderer2d&) = delete;

    bool init(std::string& error);
    void shutdown();

    void beginFrame(int framebufferWidth, int framebufferHeight, f32 contentScale,
                    f32 logicalWidth, f32 logicalHeight);
    void endFrame();

    void clear(const Color& color);

    void rect(const Rect& rect, const Color& color);
    void rectOutline(const Rect& rect, const Color& color, f32 thickness = 1.0f);
    void line(vec2 a, vec2 b, f32 thickness, const Color& color);
    void triangle(vec2 a, vec2 b, vec2 c, const Color& color);
    void circle(vec2 center, f32 radius, const Color& color, int segments = 20);
    void image(const Rect& rect, const Texture& texture, const Color& tint = Color{},
               bool flipY = false);
    void imageSub(const Rect& rect, const Texture& texture, int sx, int sy, int sw, int sh,
                  const Color& tint = Color{});
    void imageQuad(vec2 p0, vec2 p1, vec2 p2, vec2 p3, const Color& tint = Color{});

    void text(vec2 position, std::string_view text, f32 size, const Color& color);
    void textRect(const Rect& rect, std::string_view text, f32 size, const Color& color,
                  int align = 0); // 0 links, 1 zentriert, 2 rechts
    [[nodiscard]] f32 textWidth(std::string_view text, f32 size) const;
    [[nodiscard]] f32 lineHeight(f32 size) const;
    void flush();

    void pushClip(const Rect& rect);
    void popClip();

    [[nodiscard]] const sgl::Font2d& font() const { return m_font; }
    [[nodiscard]] bool fontValid() const { return m_font.isValid(); }

private:
    struct Vertex {
        f32 x, y;
        f32 u, v;
        f32 r, g, b, a;
    };

    void addQuad(const Rect& rect, f32 u0, f32 v0, f32 u1, f32 v1, const Color& color);
    void setTexture(GLuint texture);
    void applyScissor();
    void drawIconGlyph(Icon icon, const Rect& rect, const Color& color);

public:
    void icon(Icon icon, const Rect& rect, const Color& color);

private:
    std::vector<Vertex> m_vertices;
    std::vector<Rect> m_clips;
    sgl::Shader m_shader;
    sgl::Font2d m_font;
    unsigned int m_vao = 0;
    unsigned int m_vbo = 0;
    unsigned int m_white = 0;
    unsigned int m_boundTexture = 0;
    std::unordered_map<u32, sgl::GlyphMesh> m_glyphs;
    int m_fbWidth = 1;
    int m_fbHeight = 1;
    f32 m_scale = 1.0f;
    f32 m_width = 1.0f;
    f32 m_height = 1.0f;
};

using UiId = u64;

class Ui {
public:
    Ui() = default;

    void beginFrame(InputState& input, Renderer2d& renderer, const Theme& theme, const Rect& screen);
    void endFrame();

    void setInputBlocked(bool blocked) { m_inputBlocked = blocked; }
    [[nodiscard]] bool inputBlocked() const { return m_inputBlocked; }

    void pushId(std::string_view text);
    void popId();
    [[nodiscard]] UiId makeId(std::string_view text) const;
    [[nodiscard]] UiId currentId(std::string_view text) const { return makeId(text); }

    void panel(const Rect& rect, std::string_view title);
    void separator(const Rect& rect);
    void label(const Rect& rect, std::string_view text, const Color& color, int align = 0);

    bool button(const Rect& rect, std::string_view label, bool enabled = true, bool accent = false);
    bool iconButton(const Rect& rect, Icon icon, std::string_view tooltip, bool active = false,
                    bool enabled = true);
    bool checkbox(const Rect& rect, std::string_view label, bool& value);
    bool radio(const Rect& rect, std::string_view label, int& value, int option);
    bool slider(const Rect& rect, std::string_view label, f32& value, f32 minimum, f32 maximum);
    bool dragFloat(const Rect& rect, std::string_view label, f32& value, f32 step, f32 minimum,
                   f32 maximum, int decimals = 2);
    bool dragInt(const Rect& rect, std::string_view label, int& value, int minimum, int maximum);
    bool textField(const Rect& rect, std::string_view label, std::string& text, bool enabled = true);
    bool combo(const Rect& rect, std::string_view label, const std::vector<std::string>& items,
               int& selected);
    bool selectable(const Rect& rect, std::string_view label, bool selected, int depth = 0);
    bool tab(const Rect& rect, std::string_view label, bool active);
    bool colorSwatch(const Rect& rect, const Color& color, bool* open = nullptr);

    void tooltip(std::string_view text);

    Rect beginScroll(const Rect& rect, float contentHeight);
    void endScroll();

    void setClipboard(std::function<std::string()> getter, std::function<void(std::string_view)> setter) {
        m_getClipboard = std::move(getter);
        m_setClipboard = std::move(setter);
    }

    [[nodiscard]] bool isFocused(UiId id) const { return m_focus == id; }
    [[nodiscard]] bool isAnyTextFocused() const { return m_focus != 0; }
    void clearFocus() { m_focus = 0; }
    [[nodiscard]] Rect contentRect() const { return m_screen; }

private:
    struct TextState {
        std::size_t caret = 0;
        f32 scroll = 0.0f;
    };
    struct ComboState {
        UiId id = 0;
        Rect anchor{};
        std::vector<std::string> items;
        int selected = 0;
        bool opened = false;
        bool pending = false;
    };
    struct ScrollState {
        f32 offset = 0.0f;
        f32 content = 0.0f;
    };

    [[nodiscard]] bool hovered(const Rect& rect) const;
    void drawFrame(const Rect& rect, const Color& fill, const Color& border, bool hot, bool active);
    void insertText(std::string& text, std::size_t& caret, u32 codepoint);
    void eraseBefore(std::string& text, std::size_t& caret);
    void eraseAfter(std::string& text, std::size_t& caret);
    void drawComboPopup();

    InputState* m_input = nullptr;
    Renderer2d* m_renderer = nullptr;
    Theme m_theme{};
    Rect m_screen{};
    UiId m_idStack[32]{};
    int m_idDepth = 0;
    UiId m_hot = 0;
    UiId m_active = 0;
    UiId m_focus = 0;
    Rect m_focusRect{};
    bool m_inputBlocked = false;
    bool m_insideCombo = false;
    std::unordered_map<UiId, TextState> m_text;
    std::unordered_map<UiId, ScrollState> m_scroll;
    ComboState m_combo;
    bool m_scrollActive = false;
    Rect m_scrollRect{};
    UiId m_scrollId = 0;
    f32 m_scrollStart = 0.0f;
    std::string m_tooltip;
    vec2 m_tooltipPos{};
    std::function<std::string()> m_getClipboard;
    std::function<void(std::string_view)> m_setClipboard;
};

} // namespace editor::ui
