#include "WaylandInternal.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <linux/input-event-codes.h>
#include <poll.h>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>

#include <xkbcommon/xkbcommon-keysyms.h>

#include "common/MonitorAccess.hpp"

namespace sgl::detail::wayland {

WaylandCursorBackend::~WaylandCursorBackend() {
    if (m_surface) {
        wl_surface_destroy(m_surface);
        m_surface = nullptr;
    }
    if (m_ownsBuffer && m_buffer) {
        wl_buffer_destroy(m_buffer);
        m_buffer = nullptr;
    }
}

namespace {

Key TranslateKeysym(u32 keysym) {
    switch (keysym) {
    case XKB_KEY_space: return Key::Space;
    case XKB_KEY_apostrophe: return Key::Apostrophe;
    case XKB_KEY_comma: return Key::Comma;
    case XKB_KEY_minus: return Key::Minus;
    case XKB_KEY_period: return Key::Period;
    case XKB_KEY_slash: return Key::Slash;
    case XKB_KEY_0: return Key::Num0;
    case XKB_KEY_1: return Key::Num1;
    case XKB_KEY_2: return Key::Num2;
    case XKB_KEY_3: return Key::Num3;
    case XKB_KEY_4: return Key::Num4;
    case XKB_KEY_5: return Key::Num5;
    case XKB_KEY_6: return Key::Num6;
    case XKB_KEY_7: return Key::Num7;
    case XKB_KEY_8: return Key::Num8;
    case XKB_KEY_9: return Key::Num9;
    case XKB_KEY_semicolon: return Key::Semicolon;
    case XKB_KEY_equal: return Key::Equal;
    case XKB_KEY_a: case XKB_KEY_A: return Key::A;
    case XKB_KEY_b: case XKB_KEY_B: return Key::B;
    case XKB_KEY_c: case XKB_KEY_C: return Key::C;
    case XKB_KEY_d: case XKB_KEY_D: return Key::D;
    case XKB_KEY_e: case XKB_KEY_E: return Key::E;
    case XKB_KEY_f: case XKB_KEY_F: return Key::F;
    case XKB_KEY_g: case XKB_KEY_G: return Key::G;
    case XKB_KEY_h: case XKB_KEY_H: return Key::H;
    case XKB_KEY_i: case XKB_KEY_I: return Key::I;
    case XKB_KEY_j: case XKB_KEY_J: return Key::J;
    case XKB_KEY_k: case XKB_KEY_K: return Key::K;
    case XKB_KEY_l: case XKB_KEY_L: return Key::L;
    case XKB_KEY_m: case XKB_KEY_M: return Key::M;
    case XKB_KEY_n: case XKB_KEY_N: return Key::N;
    case XKB_KEY_o: case XKB_KEY_O: return Key::O;
    case XKB_KEY_p: case XKB_KEY_P: return Key::P;
    case XKB_KEY_q: case XKB_KEY_Q: return Key::Q;
    case XKB_KEY_r: case XKB_KEY_R: return Key::R;
    case XKB_KEY_s: case XKB_KEY_S: return Key::S;
    case XKB_KEY_t: case XKB_KEY_T: return Key::T;
    case XKB_KEY_u: case XKB_KEY_U: return Key::U;
    case XKB_KEY_v: case XKB_KEY_V: return Key::V;
    case XKB_KEY_w: case XKB_KEY_W: return Key::W;
    case XKB_KEY_x: case XKB_KEY_X: return Key::X;
    case XKB_KEY_y: case XKB_KEY_Y: return Key::Y;
    case XKB_KEY_z: case XKB_KEY_Z: return Key::Z;
    case XKB_KEY_bracketleft: return Key::LeftBracket;
    case XKB_KEY_backslash: return Key::Backslash;
    case XKB_KEY_bracketright: return Key::RightBracket;
    case XKB_KEY_grave: return Key::GraveAccent;
    case XKB_KEY_Escape: return Key::Escape;
    case XKB_KEY_Return: return Key::Enter;
    case XKB_KEY_Tab: case XKB_KEY_ISO_Left_Tab: return Key::Tab;
    case XKB_KEY_BackSpace: return Key::Backspace;
    case XKB_KEY_Insert: return Key::Insert;
    case XKB_KEY_Delete: return Key::Delete;
    case XKB_KEY_Right: return Key::Right;
    case XKB_KEY_Left: return Key::Left;
    case XKB_KEY_Down: return Key::Down;
    case XKB_KEY_Up: return Key::Up;
    case XKB_KEY_Page_Up: return Key::PageUp;
    case XKB_KEY_Page_Down: return Key::PageDown;
    case XKB_KEY_Home: return Key::Home;
    case XKB_KEY_End: return Key::End;
    case XKB_KEY_Caps_Lock: return Key::CapsLock;
    case XKB_KEY_Scroll_Lock: return Key::ScrollLock;
    case XKB_KEY_Num_Lock: return Key::NumLock;
    case XKB_KEY_Print: return Key::PrintScreen;
    case XKB_KEY_Pause: return Key::Pause;
    case XKB_KEY_F1: return Key::F1;
    case XKB_KEY_F2: return Key::F2;
    case XKB_KEY_F3: return Key::F3;
    case XKB_KEY_F4: return Key::F4;
    case XKB_KEY_F5: return Key::F5;
    case XKB_KEY_F6: return Key::F6;
    case XKB_KEY_F7: return Key::F7;
    case XKB_KEY_F8: return Key::F8;
    case XKB_KEY_F9: return Key::F9;
    case XKB_KEY_F10: return Key::F10;
    case XKB_KEY_F11: return Key::F11;
    case XKB_KEY_F12: return Key::F12;
    case XKB_KEY_F13: return Key::F13;
    case XKB_KEY_F14: return Key::F14;
    case XKB_KEY_F15: return Key::F15;
    case XKB_KEY_F16: return Key::F16;
    case XKB_KEY_F17: return Key::F17;
    case XKB_KEY_F18: return Key::F18;
    case XKB_KEY_F19: return Key::F19;
    case XKB_KEY_F20: return Key::F20;
    case XKB_KEY_F21: return Key::F21;
    case XKB_KEY_F22: return Key::F22;
    case XKB_KEY_F23: return Key::F23;
    case XKB_KEY_F24: return Key::F24;
    case XKB_KEY_F25: return Key::F25;
    case XKB_KEY_KP_0: return Key::KP0;
    case XKB_KEY_KP_1: return Key::KP1;
    case XKB_KEY_KP_2: return Key::KP2;
    case XKB_KEY_KP_3: return Key::KP3;
    case XKB_KEY_KP_4: return Key::KP4;
    case XKB_KEY_KP_5: return Key::KP5;
    case XKB_KEY_KP_6: return Key::KP6;
    case XKB_KEY_KP_7: return Key::KP7;
    case XKB_KEY_KP_8: return Key::KP8;
    case XKB_KEY_KP_9: return Key::KP9;
    case XKB_KEY_KP_Decimal: return Key::KPDecimal;
    case XKB_KEY_KP_Divide: return Key::KPDivide;
    case XKB_KEY_KP_Multiply: return Key::KPMultiply;
    case XKB_KEY_KP_Subtract: return Key::KPSubtract;
    case XKB_KEY_KP_Add: return Key::KPAdd;
    case XKB_KEY_KP_Enter: return Key::KPEnter;
    case XKB_KEY_KP_Equal: return Key::KPEqual;
    case XKB_KEY_Shift_L: return Key::LeftShift;
    case XKB_KEY_Control_L: return Key::LeftControl;
    case XKB_KEY_Alt_L: return Key::LeftAlt;
    case XKB_KEY_Super_L: return Key::LeftSuper;
    case XKB_KEY_Shift_R: return Key::RightShift;
    case XKB_KEY_Control_R: return Key::RightControl;
    case XKB_KEY_Alt_R: return Key::RightAlt;
    case XKB_KEY_Super_R: return Key::RightSuper;
    case XKB_KEY_Menu: return Key::Menu;
    case XKB_KEY_ISO_Level3_Shift: return Key::World1;
    case XKB_KEY_ISO_Level5_Shift: return Key::World2;
    default: return Key::Unknown;
    }
}

u32 KeysymForKey(Key key) {
    if (key >= Key::A && key <= Key::Z) {
        return XKB_KEY_a + (static_cast<u32>(key) - static_cast<u32>(Key::A));
    }
    if (key >= Key::Num0 && key <= Key::Num9) {
        return XKB_KEY_0 + (static_cast<u32>(key) - static_cast<u32>(Key::Num0));
    }
    if (key >= Key::F1 && key <= Key::F25) {
        return XKB_KEY_F1 + (static_cast<u32>(key) - static_cast<u32>(Key::F1));
    }
    if (key >= Key::KP0 && key <= Key::KP9) {
        return XKB_KEY_KP_0 + (static_cast<u32>(key) - static_cast<u32>(Key::KP0));
    }
    switch (key) {
    case Key::Space: return XKB_KEY_space;
    case Key::Escape: return XKB_KEY_Escape;
    case Key::Enter: return XKB_KEY_Return;
    case Key::Tab: return XKB_KEY_Tab;
    case Key::Backspace: return XKB_KEY_BackSpace;
    case Key::Delete: return XKB_KEY_Delete;
    case Key::Insert: return XKB_KEY_Insert;
    case Key::Home: return XKB_KEY_Home;
    case Key::End: return XKB_KEY_End;
    case Key::PageUp: return XKB_KEY_Page_Up;
    case Key::PageDown: return XKB_KEY_Page_Down;
    case Key::Left: return XKB_KEY_Left;
    case Key::Right: return XKB_KEY_Right;
    case Key::Up: return XKB_KEY_Up;
    case Key::Down: return XKB_KEY_Down;
    case Key::LeftShift: return XKB_KEY_Shift_L;
    case Key::LeftControl: return XKB_KEY_Control_L;
    case Key::LeftAlt: return XKB_KEY_Alt_L;
    case Key::LeftSuper: return XKB_KEY_Super_L;
    case Key::RightShift: return XKB_KEY_Shift_R;
    case Key::RightControl: return XKB_KEY_Control_R;
    case Key::RightAlt: return XKB_KEY_Alt_R;
    case Key::RightSuper: return XKB_KEY_Super_R;
    default: return 0;
    }
}

void XdgSurfaceConfigure(void* data, xdg_surface* surface, uint32_t serial) {
    auto* window = static_cast<WaylandWindowBackend*>(data);
    xdg_surface_ack_configure(surface, serial);
    window->handleConfigure(window->getSize());
}

void ToplevelConfigure(void* data, xdg_toplevel*, int32_t width, int32_t height,
                       wl_array* states) {
    auto* window = static_cast<WaylandWindowBackend*>(data);
    bool maximized = false;
    bool fullscreen = false;
    bool activated = false;
    if (states) {
        const auto* begin = static_cast<const uint32_t*>(states->data);
        const std::size_t count = states->size / sizeof(uint32_t);
        for (std::size_t i = 0; i < count; ++i) {
            if (begin[i] == XDG_TOPLEVEL_STATE_MAXIMIZED) maximized = true;
            else if (begin[i] == XDG_TOPLEVEL_STATE_FULLSCREEN) fullscreen = true;
            else if (begin[i] == XDG_TOPLEVEL_STATE_ACTIVATED) activated = true;
        }
    }
    window->handleToplevelConfigure(ivec2(width, height), maximized, fullscreen, activated);
}

void ToplevelClose(void* data, xdg_toplevel*) {
    static_cast<WaylandWindowBackend*>(data)->handleClose();
}

void ToplevelConfigureBounds(void*, xdg_toplevel*, int32_t, int32_t) {}
void ToplevelWmCapabilities(void*, xdg_toplevel*, wl_array*) {}

const xdg_toplevel_listener toplevelListener = {
    ToplevelConfigure, ToplevelClose, ToplevelConfigureBounds, ToplevelWmCapabilities,
};

const xdg_surface_listener xdgSurfaceListener = {XdgSurfaceConfigure};

void DecorationConfigure(void*, zxdg_toplevel_decoration_v1*, uint32_t) {}

const zxdg_toplevel_decoration_v1_listener decorationListener = {DecorationConfigure};

void SurfaceEnter(void* data, wl_surface*, wl_output* output) {
    static_cast<WaylandWindowBackend*>(data)->handleSurfaceEnter(output);
}

void SurfaceLeave(void* data, wl_surface*, wl_output* output) {
    static_cast<WaylandWindowBackend*>(data)->handleSurfaceLeave(output);
}

void SurfacePreferredBufferScale(void*, wl_surface*, int32_t) {}
void SurfacePreferredBufferTransform(void*, wl_surface*, uint32_t) {}

const wl_surface_listener surfaceListener = {
    SurfaceEnter, SurfaceLeave, SurfacePreferredBufferScale, SurfacePreferredBufferTransform,
};

} // namespace

WaylandWindowBackend::WaylandWindowBackend(const WindowHints& hints, Error& error)
    : m_size{hints.width, hints.height}, m_title(hints.title), m_hints(hints) {
    WaylandState& s = State();
    if (!s.display || !s.compositor || !s.wmBase) {
        error = MakeError(ErrorCode::PlatformUnavailable, "Wayland ist nicht verfuegbar.");
        return;
    }
    if (hints.contextApi == ContextApi::OSMesa) {
        error = MakeError(ErrorCode::FeatureUnavailable,
                          "OSMesa ist auf der Wayland-Plattform nicht verfuegbar.");
        return;
    }
    if (hints.clientApi != ClientApi::OpenGL) {
        error = MakeError(ErrorCode::ApiUnavailable,
                          "Das Wayland-Backend unterstuetzt nur OpenGL-Kontexte.");
        return;
    }

    m_resizable = hints.resizable;
    m_decorated = hints.decorated;
    m_minSize = ivec2(0, 0);
    m_maxSize = ivec2(0, 0);

    m_surface = wl_compositor_create_surface(s.compositor);
    if (!m_surface) {
        error = MakeError(ErrorCode::PlatformError, "Wayland-Oberflaeche konnte nicht erstellt werden.");
        return;
    }
    wl_surface_add_listener(m_surface, &surfaceListener, this);

    m_xdgSurface = xdg_wm_base_get_xdg_surface(s.wmBase, m_surface);
    xdg_surface_add_listener(m_xdgSurface, &xdgSurfaceListener, this);

    m_toplevel = xdg_surface_get_toplevel(m_xdgSurface);
    xdg_toplevel_add_listener(m_toplevel, &toplevelListener, this);
    xdg_toplevel_set_title(m_toplevel, m_title.c_str());
    xdg_toplevel_set_app_id(m_toplevel, m_appId.c_str());
    if (m_resizable) {
        xdg_toplevel_set_min_size(m_toplevel, 0, 0);
        xdg_toplevel_set_max_size(m_toplevel, 0, 0);
    } else {
        xdg_toplevel_set_min_size(m_toplevel, m_size.x, m_size.y);
        xdg_toplevel_set_max_size(m_toplevel, m_size.x, m_size.y);
    }

    if (s.decorationManager) {
        m_decoration = zxdg_decoration_manager_v1_get_toplevel_decoration(s.decorationManager,
                                                                         m_toplevel);
        zxdg_toplevel_decoration_v1_add_listener(m_decoration, &decorationListener, this);
        applyDecorations();
    }

    if (s.viewporter) {
        m_viewport = wp_viewporter_get_viewport(s.viewporter, m_surface);
        wp_viewport_set_destination(m_viewport, m_size.x, m_size.y);
    }

    m_eglWindow = wl_egl_window_create(m_surface, m_size.x, m_size.y);
    if (!m_eglWindow) {
        error = MakeError(ErrorCode::PlatformError, "EGL-Fenster konnte nicht erstellt werden.");
        return;
    }

    createEGL(error);
    if (error.isError()) {
        return;
    }

    m_xkbContext = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

    s.windows[m_surface] = this;
    wl_surface_commit(m_surface);

    const auto start = std::chrono::steady_clock::now();
    while (!m_configured && std::chrono::steady_clock::now() - start < std::chrono::seconds(2)) {
        DispatchEvents(true, 0.01);
    }
    if (!m_configured) {
        error = MakeError(ErrorCode::PlatformError,
                          "Wayland-Fenster wurde nicht konfiguriert.");
        return;
    }

    if (hints.visible) {
        show();
    }
    if (hints.maximized) {
        maximize();
    }
}

WaylandWindowBackend::~WaylandWindowBackend() {
    destroy();
}

void WaylandWindowBackend::createEGL(Error& error) {
    WaylandState& s = State();
    auto getPlatformDisplay = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(
        eglGetProcAddress("eglGetPlatformDisplayEXT"));
    if (getPlatformDisplay) {
        m_eglDisplay = getPlatformDisplay(EGL_PLATFORM_WAYLAND_KHR, s.display, nullptr);
    } else {
        m_eglDisplay = eglGetDisplay(reinterpret_cast<EGLNativeDisplayType>(s.display));
    }
    if (m_eglDisplay == EGL_NO_DISPLAY) {
        error = MakeError(ErrorCode::ApiUnavailable, "EGL-Display konnte nicht geoeffnet werden.");
        return;
    }

    EGLint major = 0;
    EGLint minor = 0;
    if (!eglInitialize(m_eglDisplay, &major, &minor)) {
        error = MakeError(ErrorCode::ApiUnavailable, "EGL konnte nicht initialisiert werden.");
        return;
    }
    eglBindAPI(EGL_OPENGL_API);

    EGLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_RED_SIZE, m_hints.redBits,
        EGL_GREEN_SIZE, m_hints.greenBits,
        EGL_BLUE_SIZE, m_hints.blueBits,
        EGL_ALPHA_SIZE, m_hints.alphaBits,
        EGL_DEPTH_SIZE, m_hints.depthBits,
        EGL_STENCIL_SIZE, m_hints.stencilBits,
        EGL_SAMPLE_BUFFERS, m_hints.samples > 0 ? 1 : 0,
        EGL_SAMPLES, m_hints.samples,
        EGL_NONE
    };

    EGLint configCount = 0;
    auto chooseConfig = [&]() {
        configCount = 0;
        return eglChooseConfig(m_eglDisplay, configAttribs, &m_eglConfig, 1,
                               &configCount) &&
               configCount > 0;
    };
    if (!chooseConfig() && m_hints.samples > 0) {
        // EGL_SAMPLE_BUFFERS und EGL_SAMPLES deaktivieren.
        configAttribs[17] = 0;
        configAttribs[19] = 0;
        chooseConfig();
    }
    if (configCount == 0 && m_hints.alphaBits > 0) {
        // EGL_ALPHA_SIZE deaktivieren.
        configAttribs[11] = 0;
        chooseConfig();
    }
    if (configCount == 0) {
        error = MakeError(ErrorCode::FormatUnavailable, "Keine passende EGL-Konfiguration.");
        return;
    }

    m_eglSurface = eglCreateWindowSurface(m_eglDisplay, m_eglConfig,
                                          reinterpret_cast<EGLNativeWindowType>(m_eglWindow),
                                          nullptr);
    if (m_eglSurface == EGL_NO_SURFACE) {
        error = MakeError(ErrorCode::PlatformError, "EGL-Surface konnte nicht erstellt werden.");
        return;
    }

    std::vector<EGLint> contextAttribs;
    contextAttribs.push_back(EGL_CONTEXT_MAJOR_VERSION);
    contextAttribs.push_back(m_hints.contextVersionMajor);
    contextAttribs.push_back(EGL_CONTEXT_MINOR_VERSION);
    contextAttribs.push_back(m_hints.contextVersionMinor);

    if (m_hints.contextProfile == ContextProfile::Core) {
        contextAttribs.push_back(EGL_CONTEXT_OPENGL_PROFILE_MASK);
        contextAttribs.push_back(EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT);
    } else if (m_hints.contextProfile == ContextProfile::Compatibility) {
        contextAttribs.push_back(EGL_CONTEXT_OPENGL_PROFILE_MASK);
        contextAttribs.push_back(EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT);
    }
    if (m_hints.contextForwardCompat) {
        contextAttribs.push_back(EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE);
        contextAttribs.push_back(EGL_TRUE);
    }
    if (m_hints.contextDebug) {
        contextAttribs.push_back(EGL_CONTEXT_OPENGL_DEBUG);
        contextAttribs.push_back(EGL_TRUE);
    }
    if (m_hints.contextRobustness != ContextRobustness::None) {
        contextAttribs.push_back(EGL_CONTEXT_OPENGL_ROBUST_ACCESS);
        contextAttribs.push_back(EGL_TRUE);
    }
    contextAttribs.push_back(EGL_NONE);

    m_eglContext = eglCreateContext(m_eglDisplay, m_eglConfig, EGL_NO_CONTEXT,
                                    contextAttribs.data());
    if (m_eglContext == EGL_NO_CONTEXT) {
        error = MakeError(ErrorCode::VersionUnavailable,
                          "Der angeforderte OpenGL-Kontext konnte nicht erstellt werden.");
        return;
    }

    if (!eglMakeCurrent(m_eglDisplay, m_eglSurface, m_eglSurface, m_eglContext)) {
        error = MakeError(ErrorCode::PlatformError, "EGL-Kontext konnte nicht aktuell gemacht werden.");
        return;
    }
}

void WaylandWindowBackend::destroyEGL() {
    if (m_eglDisplay == EGL_NO_DISPLAY) {
        return;
    }
    eglMakeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (m_eglContext != EGL_NO_CONTEXT) {
        eglDestroyContext(m_eglDisplay, m_eglContext);
        m_eglContext = EGL_NO_CONTEXT;
    }
    if (m_eglSurface != EGL_NO_SURFACE) {
        eglDestroySurface(m_eglDisplay, m_eglSurface);
        m_eglSurface = EGL_NO_SURFACE;
    }
    eglTerminate(m_eglDisplay);
    m_eglDisplay = EGL_NO_DISPLAY;
}

void WaylandWindowBackend::applyDecorations() {
    if (!m_decoration) {
        return;
    }
    zxdg_toplevel_decoration_v1_set_mode(
        m_decoration, m_decorated ? ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE
                                  : ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
}

void WaylandWindowBackend::destroy() {
    WaylandState& s = State();
    if (!m_surface) {
        return;
    }

    if (s.windows.contains(m_surface)) {
        s.windows.erase(m_surface);
    }
    if (s.keyboardFocus == this) {
        s.keyboardFocus = nullptr;
    }
    if (s.pointerFocus == this) {
        s.pointerFocus = nullptr;
    }

    destroyEGL();

    if (m_xkbState) {
        xkb_state_unref(m_xkbState);
        m_xkbState = nullptr;
    }
    if (m_xkbKeymap) {
        xkb_keymap_unref(m_xkbKeymap);
        m_xkbKeymap = nullptr;
    }
    if (m_xkbContext) {
        xkb_context_unref(m_xkbContext);
        m_xkbContext = nullptr;
    }
    if (m_inputRegion) {
        wl_region_destroy(m_inputRegion);
        m_inputRegion = nullptr;
    }
    if (m_eglWindow) {
        wl_egl_window_destroy(m_eglWindow);
        m_eglWindow = nullptr;
    }
    if (m_viewport) {
        wp_viewport_destroy(m_viewport);
        m_viewport = nullptr;
    }
    if (m_decoration) {
        zxdg_toplevel_decoration_v1_destroy(m_decoration);
        m_decoration = nullptr;
    }
    if (m_toplevel) {
        xdg_toplevel_destroy(m_toplevel);
        m_toplevel = nullptr;
    }
    if (m_xdgSurface) {
        xdg_surface_destroy(m_xdgSurface);
        m_xdgSurface = nullptr;
    }
    if (m_surface) {
        wl_surface_destroy(m_surface);
        m_surface = nullptr;
    }
    m_mapped = false;
    m_visible = false;
}

void WaylandWindowBackend::makeContextCurrent() {
    if (m_eglDisplay != EGL_NO_DISPLAY) {
        eglMakeCurrent(m_eglDisplay, m_eglSurface, m_eglSurface, m_eglContext);
    }
}

void WaylandWindowBackend::swapBuffers() {
    if (m_eglDisplay != EGL_NO_DISPLAY) {
        eglSwapBuffers(m_eglDisplay, m_eglSurface);
        m_mapped = true;
    }
}

void* WaylandWindowBackend::getProcAddress(const char* name) {
    return reinterpret_cast<void*>(eglGetProcAddress(name));
}

NativeHandle WaylandWindowBackend::getNativeHandle() const {
    NativeHandle handle;
    handle.platform = NativePlatform::Wayland;
    handle.display = State().display;
    handle.window = m_surface;
    handle.context = m_eglContext;
    handle.surface = m_eglSurface;
    handle.eglDisplay = m_eglDisplay;
    return handle;
}

ivec2 WaylandWindowBackend::getFramebufferSize() const {
    const auto scale = static_cast<i32>(m_scale);
    return ivec2(m_size.x * scale, m_size.y * scale);
}

void WaylandWindowBackend::setSwapInterval(i32 interval) {
    m_swapInterval = interval;
    if (m_eglDisplay != EGL_NO_DISPLAY) {
        eglSwapInterval(m_eglDisplay, interval);
    }
}

bool WaylandWindowBackend::isExtensionSupported(const char* name) {
    const char* extensions = eglQueryString(m_eglDisplay, EGL_EXTENSIONS);
    if (extensions && std::strstr(extensions, name)) {
        return true;
    }

    using GetStringiProc = const unsigned char* (*)(unsigned int, unsigned int);
    using GetIntegervProc = void (*)(unsigned int, int*);
    auto getStringi = reinterpret_cast<GetStringiProc>(eglGetProcAddress("glGetStringi"));
    auto getIntegerv = reinterpret_cast<GetIntegervProc>(eglGetProcAddress("glGetIntegerv"));
    if (!getStringi || !getIntegerv) {
        return false;
    }
    constexpr unsigned int GL_EXTENSIONS_ENUM = 0x1F03;
    constexpr unsigned int GL_NUM_EXTENSIONS_ENUM = 0x821D;
    int count = 0;
    getIntegerv(GL_NUM_EXTENSIONS_ENUM, &count);
    for (int i = 0; i < count; ++i) {
        const auto* extension = reinterpret_cast<const char*>(
            getStringi(GL_EXTENSIONS_ENUM, static_cast<unsigned int>(i)));
        if (extension && std::strcmp(extension, name) == 0) {
            return true;
        }
    }
    return false;
}

void WaylandWindowBackend::setTitle(std::string_view title) {
    m_title = std::string(title);
    if (m_toplevel) {
        xdg_toplevel_set_title(m_toplevel, m_title.c_str());
    }
}

void WaylandWindowBackend::setSize(ivec2 size) {
    m_size = ivec2(std::max(size.x, 1), std::max(size.y, 1));
    if (m_eglWindow) {
        wl_egl_window_resize(m_eglWindow, m_size.x * static_cast<i32>(m_scale),
                             m_size.y * static_cast<i32>(m_scale), 0, 0);
    }
    if (m_viewport) {
        wp_viewport_set_destination(m_viewport, m_size.x, m_size.y);
    }
    if (m_visible) {
        swapBuffers();
    }
    if (m_events) {
        m_events->onSize(m_size);
        m_events->onFramebufferSize(getFramebufferSize());
    }
}

void WaylandWindowBackend::setPosition(ivec2) {
    ReportError(ErrorCode::FeatureUnavailable,
                "Wayland erlaubt kein Setzen der Fensterposition.");
}

vec2 WaylandWindowBackend::getContentScale() const {
    return {m_scale, m_scale};
}

void WaylandWindowBackend::show() {
    if (!m_configured) {
        m_visible = true;
        return;
    }
    m_visible = true;
    swapBuffers();
}

void WaylandWindowBackend::hide() {
    m_visible = false;
    m_mapped = false;
    if (m_surface) {
        wl_surface_attach(m_surface, nullptr, 0, 0);
        wl_surface_commit(m_surface);
    }
}

void WaylandWindowBackend::requestAttention() {
}

void WaylandWindowBackend::iconify() {
    if (m_toplevel) {
        xdg_toplevel_set_minimized(m_toplevel);
    }
}

void WaylandWindowBackend::restore() {
    if (m_toplevel) {
        xdg_toplevel_unset_maximized(m_toplevel);
        xdg_toplevel_unset_fullscreen(m_toplevel);
    }
    m_maximized = false;
    m_fullscreen = false;
    show();
}

void WaylandWindowBackend::maximize() {
    if (m_toplevel) {
        xdg_toplevel_set_maximized(m_toplevel);
    }
    m_maximized = true;
}

i32 WaylandWindowBackend::getAttrib(WindowAttrib attrib) const {
    switch (attrib) {
    case WindowAttrib::Focused: return m_focused ? 1 : 0;
    case WindowAttrib::Iconified: return m_minimized ? 1 : 0;
    case WindowAttrib::Resizable: return m_resizable ? 1 : 0;
    case WindowAttrib::Visible: return m_visible ? 1 : 0;
    case WindowAttrib::Decorated: return m_decorated ? 1 : 0;
    case WindowAttrib::AutoIconify: return 1;
    case WindowAttrib::Floating: return 0;
    case WindowAttrib::Maximized: return m_maximized ? 1 : 0;
    case WindowAttrib::Hovered: return m_hovered ? 1 : 0;
    case WindowAttrib::FocusOnShow: return m_hints.focusOnShow ? 1 : 0;
    case WindowAttrib::MousePassthrough: return m_mousePassthrough ? 1 : 0;
    case WindowAttrib::ScaleToMonitor: return m_scaleToMonitor ? 1 : 0;
    case WindowAttrib::Doublebuffer: return m_hints.doublebuffer ? 1 : 0;
    case WindowAttrib::ClientApi: return static_cast<i32>(m_hints.clientApi);
    case WindowAttrib::ContextCreationApi: return static_cast<i32>(m_hints.contextApi);
    case WindowAttrib::ContextVersionMajor: return m_hints.contextVersionMajor;
    case WindowAttrib::ContextVersionMinor: return m_hints.contextVersionMinor;
    case WindowAttrib::ContextRevision: return 0;
    case WindowAttrib::ContextRobustness: return static_cast<i32>(m_hints.contextRobustness);
    case WindowAttrib::ContextReleaseBehavior: return static_cast<i32>(m_hints.contextReleaseBehavior);
    case WindowAttrib::ContextDebug: return m_hints.contextDebug ? 1 : 0;
    case WindowAttrib::ContextNoError: return m_hints.contextNoError ? 1 : 0;
    case WindowAttrib::ContextForwardCompat: return m_hints.contextForwardCompat ? 1 : 0;
    case WindowAttrib::OpenGLProfile: return static_cast<i32>(m_hints.contextProfile);
    case WindowAttrib::Samples: return m_hints.samples;
    case WindowAttrib::RedBits: return m_hints.redBits;
    case WindowAttrib::GreenBits: return m_hints.greenBits;
    case WindowAttrib::BlueBits: return m_hints.blueBits;
    case WindowAttrib::AlphaBits: return m_hints.alphaBits;
    case WindowAttrib::DepthBits: return m_hints.depthBits;
    case WindowAttrib::StencilBits: return m_hints.stencilBits;
    case WindowAttrib::AccumRedBits: return m_hints.accumRedBits;
    case WindowAttrib::AccumGreenBits: return m_hints.accumGreenBits;
    case WindowAttrib::AccumBlueBits: return m_hints.accumBlueBits;
    case WindowAttrib::AccumAlphaBits: return m_hints.accumAlphaBits;
    case WindowAttrib::AuxBuffers: return m_hints.auxBuffers;
    case WindowAttrib::Stereo: return m_hints.stereo ? 1 : 0;
    case WindowAttrib::SRGBCapable: return m_hints.srgbCapable ? 1 : 0;
    case WindowAttrib::RefreshRate: return m_hints.refreshRate;
    case WindowAttrib::TransparentFramebuffer: return m_hints.transparentFramebuffer ? 1 : 0;
    case WindowAttrib::CenterCursor: return 0;
    }
    return 0;
}

void WaylandWindowBackend::setAttrib(WindowAttrib attrib, bool value) {
    switch (attrib) {
    case WindowAttrib::Visible:
        value ? show() : hide();
        break;
    case WindowAttrib::Resizable:
        m_resizable = value;
        if (m_toplevel) {
            if (value) {
                xdg_toplevel_set_min_size(m_toplevel, m_minSize.x, m_minSize.y);
                xdg_toplevel_set_max_size(m_toplevel, m_maxSize.x, m_maxSize.y);
            } else {
                xdg_toplevel_set_min_size(m_toplevel, m_size.x, m_size.y);
                xdg_toplevel_set_max_size(m_toplevel, m_size.x, m_size.y);
            }
        }
        break;
    case WindowAttrib::Decorated:
        m_decorated = value;
        applyDecorations();
        break;
    case WindowAttrib::Maximized:
        value ? maximize() : restore();
        break;
    case WindowAttrib::MousePassthrough:
        m_mousePassthrough = value;
        updateInputRegion();
        break;
    case WindowAttrib::ScaleToMonitor:
        m_scaleToMonitor = value;
        break;
    default:
        break;
    }
}

void WaylandWindowBackend::setSizeLimits(ivec2 minimum, ivec2 maximum) {
    m_minSize = minimum;
    m_maxSize = maximum;
    if (m_toplevel) {
        xdg_toplevel_set_min_size(m_toplevel, std::max(minimum.x, 0), std::max(minimum.y, 0));
        xdg_toplevel_set_max_size(m_toplevel, std::max(maximum.x, 0), std::max(maximum.y, 0));
    }
}

void WaylandWindowBackend::setMonitor(const Monitor& monitor, ivec2, ivec2 size, i32) {
    if (monitor.isValid() && size.x > 0 && size.y > 0) {
        auto* output = static_cast<WaylandOutput*>(MonitorAccess::Native(monitor));
        if (m_toplevel) {
            xdg_toplevel_set_fullscreen(m_toplevel, output ? output->output : nullptr);
        }
        m_fullscreen = true;
        m_fullscreenMonitor = monitor;
    } else if (m_toplevel) {
        xdg_toplevel_unset_fullscreen(m_toplevel);
        m_fullscreen = false;
        m_fullscreenMonitor = Monitor{};
    }
}

Monitor WaylandWindowBackend::getMonitor() const {
    if (m_currentOutput) {
        return MonitorAccess::Make(m_currentOutput, Platform::Wayland);
    }
    if (m_fullscreenMonitor.isValid()) {
        return m_fullscreenMonitor;
    }
    if (!State().outputs.empty()) {
        return MonitorAccess::Make(State().outputs.front().get(), Platform::Wayland);
    }
    return {};
}

void WaylandWindowBackend::setInputMode(InputMode mode, i32 value) {
    switch (mode) {
    case InputMode::Cursor:
        m_cursorMode = static_cast<CursorMode>(value);
        updateCursor();
        break;
    case InputMode::StickyKeys:
        m_stickyKeys = value != 0;
        break;
    case InputMode::StickyMouseButtons:
        m_stickyMouseButtons = value != 0;
        break;
    case InputMode::LockKeyMods:
        m_lockKeyMods = value != 0;
        break;
    case InputMode::RawMouseMotion:
        m_rawMouseMotion = value != 0;
        break;
    }
}

i32 WaylandWindowBackend::getInputMode(InputMode mode) const {
    switch (mode) {
    case InputMode::Cursor: return static_cast<i32>(m_cursorMode);
    case InputMode::StickyKeys: return m_stickyKeys ? 1 : 0;
    case InputMode::StickyMouseButtons: return m_stickyMouseButtons ? 1 : 0;
    case InputMode::LockKeyMods: return m_lockKeyMods ? 1 : 0;
    case InputMode::RawMouseMotion: return m_rawMouseMotion ? 1 : 0;
    }
    return 0;
}

std::string WaylandWindowBackend::getKeyName(Key key, i32) const {
    char buffer[64] = {};
    const u32 keysym = KeysymForKey(key);
    if (keysym == 0) {
        return {};
    }
    const int length = xkb_keysym_to_utf8(keysym, buffer, sizeof(buffer));
    if (length > 1) {
        return std::string(buffer, static_cast<std::size_t>(length - 1));
    }
    char name[64] = {};
    if (xkb_keysym_get_name(keysym, name, sizeof(name)) > 0) {
        return name;
    }
    return {};
}

i32 WaylandWindowBackend::getKeyScancode(Key key) const {
    if (!m_xkbState || !m_xkbKeymap) {
        return -1;
    }
    const u32 target = KeysymForKey(key);
    if (target == 0) {
        return -1;
    }
    for (u32 keycode = 8; keycode < 256; ++keycode) {
        if (xkb_state_key_get_one_sym(m_xkbState, keycode) == target) {
            return static_cast<i32>(keycode);
        }
    }
    return -1;
}

bool WaylandWindowBackend::isKeyPressed(Key key) const {
    const i32 scancode = getKeyScancode(key);
    return scancode >= 0 && m_pressedKeys.contains(static_cast<u32>(scancode));
}

bool WaylandWindowBackend::isMouseButtonPressed(MouseButton button) const {
    const auto index = static_cast<std::size_t>(button);
    return index < m_pressedButtons.size() && m_pressedButtons[index];
}

void WaylandWindowBackend::setCursorPos(vec2) {}

void WaylandWindowBackend::setCursor(CursorBackend* cursor) {
    m_cursor = dynamic_cast<WaylandCursorBackend*>(cursor);
    updateCursor();
}

void WaylandWindowBackend::updateCursor() {
    WaylandState& s = State();
    if (!s.pointer || s.pointerFocus != this) {
        return;
    }
    const bool showCursor = m_cursorMode == CursorMode::Normal && m_cursor;
    if (showCursor) {
        wl_pointer_set_cursor(s.pointer, m_lastSerial, m_cursor->getSurface(),
                              m_cursor->getHotspot().x, m_cursor->getHotspot().y);
        wl_surface_attach(m_cursor->getSurface(), m_cursor->getBuffer(), 0, 0);
        wl_surface_damage(m_cursor->getSurface(), 0, 0, m_cursor->getSize().x,
                          m_cursor->getSize().y);
        wl_surface_commit(m_cursor->getSurface());
    } else {
        wl_pointer_set_cursor(s.pointer, m_lastSerial, nullptr, 0, 0);
    }
}

void WaylandWindowBackend::updateInputRegion() {
    if (!m_surface) {
        return;
    }
    if (m_inputRegion) {
        wl_region_destroy(m_inputRegion);
        m_inputRegion = nullptr;
    }
    m_inputRegion = wl_compositor_create_region(State().compositor);
    if (!m_mousePassthrough) {
        wl_region_add(m_inputRegion, 0, 0, m_size.x, m_size.y);
    }
    wl_surface_set_input_region(m_surface, m_inputRegion);
    wl_surface_commit(m_surface);
}

std::string WaylandWindowBackend::getClipboardString() const {
    WaylandState& s = State();
    if (!s.clipboardOffer) {
        return {};
    }

    auto receive = [&s](const char* mimeType) {
        int fds[2];
        if (pipe(fds) != 0) {
            return std::string{};
        }
        wl_data_offer_receive(s.clipboardOffer, mimeType, fds[1]);
        ::close(fds[1]);
        wl_display_flush(s.display);

        std::string result;
        char buffer[4096];
        while (true) {
            pollfd pfd{fds[0], POLLIN, 0};
            if (poll(&pfd, 1, 500) <= 0) {
                break;
            }
            const ssize_t count = ::read(fds[0], buffer, sizeof(buffer));
            if (count <= 0) {
                break;
            }
            result.append(buffer, static_cast<std::size_t>(count));
        }
        ::close(fds[0]);
        return result;
    };

    std::string result = receive("text/plain;charset=utf-8");
    if (result.empty()) {
        result = receive("UTF8_STRING");
    }
    if (result.empty()) {
        result = receive("text/plain");
    }
    return result;
}

void WaylandWindowBackend::setClipboardString(std::string_view text) {
    WaylandState& s = State();
    if (!s.dataDeviceManager || !s.dataDevice) {
        ReportError(ErrorCode::FeatureUnavailable,
                    "Wayland-Zwischenablage ist nicht verfuegbar.");
        return;
    }
    if (s.clipboardSource) {
        wl_data_source_destroy(s.clipboardSource);
    }
    s.clipboardText = std::string(text);
    m_clipboardText = s.clipboardText;
    s.clipboardSource = wl_data_device_manager_create_data_source(s.dataDeviceManager);
    wl_data_source_add_listener(s.clipboardSource, DataSourceListenerPtr(), nullptr);
    wl_data_source_offer(s.clipboardSource, "text/plain;charset=utf-8");
    wl_data_source_offer(s.clipboardSource, "text/plain");
    wl_data_source_offer(s.clipboardSource, "UTF8_STRING");
    wl_data_device_set_selection(s.dataDevice, s.clipboardSource, s.lastSerial);
    wl_display_flush(s.display);
}

void WaylandWindowBackend::handleConfigure(ivec2 size) {
    if (size.x > 0 && size.y > 0) {
        m_size = size;
    }
    m_configured = true;
    m_mapped = true;
    if (m_eglWindow) {
        wl_egl_window_resize(m_eglWindow, m_size.x * static_cast<i32>(m_scale),
                             m_size.y * static_cast<i32>(m_scale), 0, 0);
    }
    if (m_viewport) {
        wp_viewport_set_destination(m_viewport, m_size.x, m_size.y);
    }
    if (m_events) {
        m_events->onSize(m_size);
        m_events->onFramebufferSize(getFramebufferSize());
    }
}

void WaylandWindowBackend::handleToplevelConfigure(ivec2 size, bool maximized, bool fullscreen,
                                                   bool activated) {
    if (size.x > 0 && size.y > 0) {
        m_size = size;
    }
    if (maximized != m_maximized) {
        m_maximized = maximized;
        if (m_events) {
            m_events->onMaximize(maximized);
        }
    }
    m_fullscreen = fullscreen;
    m_activated = activated;
}

void WaylandWindowBackend::handleClose() {
    m_closeRequested = true;
    if (m_events) {
        m_events->onClose();
    }
}

void WaylandWindowBackend::handleSurfaceEnter(wl_output* output) {
    if (auto* entry = FindOutput(output)) {
        m_currentOutput = entry;
        m_scale = static_cast<f32>(entry->scale);
        if (m_surface) {
            wl_surface_set_buffer_scale(m_surface, entry->scale);
        }
        if (m_eglWindow) {
            wl_egl_window_resize(m_eglWindow, m_size.x * entry->scale, m_size.y * entry->scale, 0,
                                 0);
        }
        if (m_events) {
            m_events->onContentScale(vec2(m_scale, m_scale));
        }
    }
}

void WaylandWindowBackend::handleSurfaceLeave(wl_output* output) {
    if (m_currentOutput == FindOutput(output)) {
        m_currentOutput = nullptr;
    }
}

void WaylandWindowBackend::handleKeymap(uint32_t, int fd, uint32_t size) {
    if (!m_xkbContext) {
        return;
    }
    auto* data = static_cast<char*>(mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0));
    if (data == MAP_FAILED) {
        return;
    }
    if (m_xkbState) {
        xkb_state_unref(m_xkbState);
        m_xkbState = nullptr;
    }
    if (m_xkbKeymap) {
        xkb_keymap_unref(m_xkbKeymap);
        m_xkbKeymap = nullptr;
    }
    m_xkbKeymap = xkb_keymap_new_from_string(m_xkbContext, data, XKB_KEYMAP_FORMAT_TEXT_V1,
                                             XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (m_xkbKeymap) {
        m_xkbState = xkb_state_new(m_xkbKeymap);
    }
    munmap(data, size);
}

void WaylandWindowBackend::handleKeyboardEnter(uint32_t serial, wl_array* keys) {
    m_lastSerial = serial;
    m_focused = true;
    m_pressedKeys.clear();
    if (keys) {
        const auto* begin = static_cast<const uint32_t*>(keys->data);
        const std::size_t count = keys->size / sizeof(uint32_t);
        m_pressedKeys.insert(begin, begin + count);
    }
    if (m_events) {
        m_events->onFocus(true);
    }
}

void WaylandWindowBackend::handleKeyboardLeave() {
    m_focused = false;
    m_pressedKeys.clear();
    if (m_events) {
        m_events->onFocus(false);
    }
}

void WaylandWindowBackend::handleKey(uint32_t, uint32_t, uint32_t key, uint32_t keyState) {
    const bool pressed = keyState == WL_KEYBOARD_KEY_STATE_PRESSED;
    KeyAction action = pressed ? KeyAction::Press : KeyAction::Release;
    if (pressed) {
        if (m_pressedKeys.contains(key)) {
            action = KeyAction::Repeat;
        } else {
            m_pressedKeys.insert(key);
        }
    } else {
        m_pressedKeys.erase(key);
    }

    Key keyEnum = Key::Unknown;
    Modifiers mods = Modifiers::None;
    if (m_xkbState) {
        keyEnum = TranslateKeysym(xkb_state_key_get_one_sym(m_xkbState, key));
        const xkb_mod_mask_t mask = xkb_state_serialize_mods(m_xkbState, XKB_STATE_MODS_EFFECTIVE);
        if (mask & (1u << 0)) mods |= Modifiers::Shift;
        if (mask & (1u << 2)) mods |= Modifiers::Control;
        if (mask & (1u << 3)) mods |= Modifiers::Alt;
        if (mask & (1u << 6)) mods |= Modifiers::Super;
        if (mask & (1u << 1)) mods |= Modifiers::CapsLock;
        if (mask & (1u << 4)) mods |= Modifiers::NumLock;
        if (!m_lockKeyMods) {
            mods = static_cast<Modifiers>(static_cast<int>(mods) &
                                          ~static_cast<int>(Modifiers::CapsLock | Modifiers::NumLock));
        }
    }

    if (m_events) {
        m_events->onKey(keyEnum, static_cast<i32>(key), action, mods);
    }

    if (pressed && action != KeyAction::Repeat && m_xkbState) {
        char buffer[64] = {};
        const int length = xkb_state_key_get_utf8(m_xkbState, key, buffer, sizeof(buffer));
        if (length > 0) {
            const auto* bytes = reinterpret_cast<const unsigned char*>(buffer);
            int offset = 0;
            while (offset < length) {
                u32 codepoint = 0;
                int consumed = 0;
                if (bytes[offset] < 0x80) {
                    codepoint = bytes[offset];
                    consumed = 1;
                } else if ((bytes[offset] & 0xE0) == 0xC0 && offset + 1 < length) {
                    codepoint = ((bytes[offset] & 0x1F) << 6) | (bytes[offset + 1] & 0x3F);
                    consumed = 2;
                } else if ((bytes[offset] & 0xF0) == 0xE0 && offset + 2 < length) {
                    codepoint = ((bytes[offset] & 0x0F) << 12) |
                                ((bytes[offset + 1] & 0x3F) << 6) | (bytes[offset + 2] & 0x3F);
                    consumed = 3;
                } else if ((bytes[offset] & 0xF8) == 0xF0 && offset + 3 < length) {
                    codepoint = ((bytes[offset] & 0x07) << 18) |
                                ((bytes[offset + 1] & 0x3F) << 12) |
                                ((bytes[offset + 2] & 0x3F) << 6) | (bytes[offset + 3] & 0x3F);
                    consumed = 4;
                } else {
                    break;
                }
                offset += consumed;
                if (codepoint >= 32 || codepoint == 9 || codepoint == 10 || codepoint == 13) {
                    if (m_events) {
                        m_events->onChar(codepoint);
                        m_events->onCharMods(codepoint, mods);
                    }
                }
            }
        }
    }
}

void WaylandWindowBackend::handleModifiers(uint32_t, uint32_t depressed, uint32_t latched,
                                           uint32_t locked, uint32_t group) {
    if (m_xkbState) {
        xkb_state_update_mask(m_xkbState, depressed, latched, locked, 0, 0, group);
    }
}

void WaylandWindowBackend::handlePointerEnter(uint32_t serial, wl_surface*, vec2 position) {
    m_lastSerial = serial;
    m_hovered = true;
    m_cursorPos = position;
    if (m_events) {
        m_events->onCursorEnter(true);
        m_events->onCursorPos(position);
    }
    updateCursor();
}

void WaylandWindowBackend::handlePointerLeave() {
    m_hovered = false;
    if (m_events) {
        m_events->onCursorEnter(false);
    }
}

void WaylandWindowBackend::handlePointerMotion(vec2 position) {
    m_cursorPos = position;
    if (m_events) {
        m_events->onCursorPos(position);
    }
}

void WaylandWindowBackend::handlePointerButton(uint32_t serial, uint32_t button, bool pressed) {
    m_lastSerial = serial;
    MouseButton mouseButton = MouseButton::Left;
    std::size_t index = 0;
    switch (button) {
    case BTN_LEFT: mouseButton = MouseButton::Left; index = 0; break;
    case BTN_RIGHT: mouseButton = MouseButton::Right; index = 1; break;
    case BTN_MIDDLE: mouseButton = MouseButton::Middle; index = 2; break;
    case BTN_SIDE: mouseButton = MouseButton::Button4; index = 3; break;
    case BTN_EXTRA: mouseButton = MouseButton::Button5; index = 4; break;
    case BTN_FORWARD: mouseButton = MouseButton::Button6; index = 5; break;
    case BTN_BACK: mouseButton = MouseButton::Button7; index = 6; break;
    case BTN_TASK: mouseButton = MouseButton::Button8; index = 7; break;
    default: return;
    }
    m_pressedButtons[index] = pressed;
    if (m_events) {
        m_events->onMouseButton(mouseButton, pressed ? KeyAction::Press : KeyAction::Release,
                                Modifiers::None);
    }
}

void WaylandWindowBackend::handlePointerAxis(vec2 offset) {
    if (m_events) {
        m_events->onScroll(offset);
    }
}

void WaylandWindowBackend::handleDrop(std::vector<std::string> paths) {
    if (m_events) {
        m_events->onDrop(std::move(paths));
    }
}

} // namespace sgl::detail::wayland
