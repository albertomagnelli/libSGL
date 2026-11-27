#include "X11Internal.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

#include <X11/extensions/shape.h>

#include "common/MonitorAccess.hpp"

namespace sgl::detail::x11 {
namespace {

using CreateContextAttribsProc = GLXContext (*)(Display*, GLXFBConfig, GLXContext, Bool, const int*);
using SwapIntervalExtProc = void (*)(Display*, GLXDrawable, int);
using SwapIntervalMesaProc = int (*)(unsigned int);
using SwapIntervalSgiProc = int (*)(int);

template <typename T>
T loadGLX(const char* name) {
    return reinterpret_cast<T>(glXGetProcAddressARB(reinterpret_cast<const GLubyte*>(name)));
}

void setWindowName(::Window window, const std::string& title) {
    State& s = state();
    XStoreName(s.display, window, title.c_str());
    XChangeProperty(s.display, window, s.netWmName, s.utf8String, 8, PropModeReplace,
                    reinterpret_cast<const unsigned char*>(title.data()),
                    static_cast<int>(title.size()));
}

} // namespace

X11WindowBackend::X11WindowBackend(const WindowHints& hints, Error& error)
    : m_size{hints.width, hints.height}, m_title(hints.title), m_hints(hints) {
    State& s = state();
    if (!s.display) {
        error = makeError(ErrorCode::PlatformUnavailable, "X11-Display ist nicht verfuegbar.");
        return;
    }

    m_resizable = hints.resizable;
    m_decorated = hints.decorated;
    m_floating = hints.floating;
    m_autoIconify = hints.autoIconify;
    m_focusOnShow = hints.focusOnShow;
    m_mousePassthrough = hints.mousePassthrough;
    m_doublebuffer = hints.doublebuffer;
    m_scaleToMonitor = hints.scaleToMonitor;
    m_visible = false;

    m_useEgl = hints.contextApi == ContextApi::EGL;
    if (hints.contextApi != ContextApi::Native && !m_useEgl) {
        error = makeError(ErrorCode::FeatureUnavailable,
                          "Dieses Backend unterstuetzt nur native GLX- oder EGL-Kontexte.");
        return;
    }
    if (hints.clientApi != ClientApi::OpenGL) {
        error = makeError(ErrorCode::ApiUnavailable,
                          "Das X11-Backend unterstuetzt nur OpenGL-Kontexte.");
        return;
    }

    if (!m_useEgl) {
        int glxMajor = 0;
        int glxMinor = 0;
        if (!glXQueryVersion(s.display, &glxMajor, &glxMinor) ||
            (glxMajor == 1 && glxMinor < 3) || glxMajor < 1) {
            error = makeError(ErrorCode::ApiUnavailable, "GLX 1.3 oder neuer wird benoetigt.");
            return;
        }
    }

    GLXFBConfig config = nullptr;
    XVisualInfo* visual = nullptr;

    if (m_useEgl) {
        auto getPlatformDisplay = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(
            eglGetProcAddress("eglGetPlatformDisplayEXT"));
        if (getPlatformDisplay) {
            m_eglDisplay = getPlatformDisplay(EGL_PLATFORM_X11_KHR, s.display, nullptr);
        } else {
            m_eglDisplay = eglGetDisplay(reinterpret_cast<EGLNativeDisplayType>(s.display));
        }
        if (m_eglDisplay == EGL_NO_DISPLAY) {
            error = makeError(ErrorCode::ApiUnavailable,
                              "EGL-Display konnte nicht geoeffnet werden.");
            return;
        }
        EGLint major = 0;
        EGLint minor = 0;
        if (!eglInitialize(m_eglDisplay, &major, &minor)) {
            error = makeError(ErrorCode::ApiUnavailable, "EGL konnte nicht initialisiert werden.");
            return;
        }
        eglBindAPI(EGL_OPENGL_API);

        const EGLint configAttribs[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
            EGL_RED_SIZE, hints.redBits,
            EGL_GREEN_SIZE, hints.greenBits,
            EGL_BLUE_SIZE, hints.blueBits,
            EGL_ALPHA_SIZE, hints.alphaBits,
            EGL_DEPTH_SIZE, hints.depthBits,
            EGL_STENCIL_SIZE, hints.stencilBits,
            EGL_SAMPLE_BUFFERS, hints.samples > 0 ? 1 : 0,
            EGL_SAMPLES, hints.samples,
            EGL_NONE
        };
        EGLint configCount = 0;
        if (!eglChooseConfig(m_eglDisplay, configAttribs, &m_eglConfig, 1, &configCount) ||
            configCount == 0) {
            error = makeError(ErrorCode::FormatUnavailable, "Keine passende EGL-Konfiguration.");
            return;
        }
        EGLint visualId = 0;
        eglGetConfigAttrib(m_eglDisplay, m_eglConfig, EGL_NATIVE_VISUAL_ID, &visualId);
        XVisualInfo templateInfo{};
        templateInfo.visualid = static_cast<VisualID>(visualId);
        int visualCount = 0;
        visual = XGetVisualInfo(s.display, VisualIDMask, &templateInfo, &visualCount);
    } else {
        int fbAttribs[] = {
            GLX_X_RENDERABLE, True,
            GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
            GLX_RENDER_TYPE, GLX_RGBA_BIT,
            GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
            GLX_RED_SIZE, hints.redBits,
            GLX_GREEN_SIZE, hints.greenBits,
            GLX_BLUE_SIZE, hints.blueBits,
            GLX_ALPHA_SIZE, hints.alphaBits,
            GLX_DEPTH_SIZE, hints.depthBits,
            GLX_STENCIL_SIZE, hints.stencilBits,
            GLX_DOUBLEBUFFER, hints.doublebuffer ? True : False,
            GLX_STEREO, hints.stereo ? True : False,
            GLX_SAMPLE_BUFFERS, hints.samples > 0 ? 1 : 0,
            GLX_SAMPLES, hints.samples,
            0
        };

        int count = 0;
        GLXFBConfig* configs = glXChooseFBConfig(s.display, s.screen, fbAttribs, &count);
        if ((!configs || count == 0) && hints.samples > 0) {
            fbAttribs[23] = 0;
            fbAttribs[25] = 0;
            configs = glXChooseFBConfig(s.display, s.screen, fbAttribs, &count);
        }
        if ((!configs || count == 0) && hints.alphaBits > 0) {
            fbAttribs[11] = 0;
            configs = glXChooseFBConfig(s.display, s.screen, fbAttribs, &count);
        }
        if (!configs || count == 0) {
            error = makeError(ErrorCode::FormatUnavailable,
                              "Kein passender GLX-Framebuffer gefunden.");
            if (configs) {
                XFree(configs);
            }
            return;
        }

        config = configs[0];
        visual = glXGetVisualFromFBConfig(s.display, config);
        XFree(configs);
    }

    if (!visual) {
        error = makeError(ErrorCode::FormatUnavailable, "Kein passendes X11-Visual gefunden.");
        return;
    }

    m_colormap = XCreateColormap(s.display, s.root, visual->visual, AllocNone);

    XSetWindowAttributes attributes{};
    attributes.colormap = m_colormap;
    attributes.border_pixel = 0;
    attributes.background_pixel = 0;
    attributes.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask |
                            ButtonReleaseMask | PointerMotionMask | StructureNotifyMask |
                            FocusChangeMask | EnterWindowMask | LeaveWindowMask |
                            PropertyChangeMask | VisibilityChangeMask;

    m_window = XCreateWindow(s.display, s.root, 0, 0,
                             static_cast<unsigned int>(std::max(hints.width, 1)),
                             static_cast<unsigned int>(std::max(hints.height, 1)), 0,
                             visual->depth, InputOutput, visual->visual,
                             CWColormap | CWBorderPixel | CWBackPixel | CWEventMask, &attributes);
    XFree(visual);

    if (m_window == 0) {
        error = makeError(ErrorCode::PlatformError, "X11-Fenster konnte nicht erstellt werden.");
        return;
    }

    setWindowName(m_window, m_title);

    XClassHint classHint{};
    classHint.res_name = const_cast<char*>(
        hints.x11InstanceName.empty() ? m_title.c_str() : hints.x11InstanceName.c_str());
    classHint.res_class = const_cast<char*>(
        hints.x11ClassName.empty() ? "sgl" : hints.x11ClassName.c_str());
    XSetClassHint(s.display, m_window, &classHint);

    Atom protocols[2] = {s.wmDeleteWindow, s.wmTakeFocus};
    XSetWMProtocols(s.display, m_window, protocols, 2);

    const long pid = static_cast<long>(getpid());
    XChangeProperty(s.display, m_window, s.netWmPid, XA_CARDINAL, 32, PropModeReplace,
                    reinterpret_cast<const unsigned char*>(&pid), 1);

    const long xdndVersion = 5;
    XChangeProperty(s.display, m_window, s.xdndAware, XA_ATOM, 32, PropModeReplace,
                    reinterpret_cast<const unsigned char*>(&xdndVersion), 1);

    if (!hints.resizable) {
        XSizeHints sizeHints{};
        sizeHints.flags = PMinSize | PMaxSize;
        sizeHints.min_width = sizeHints.max_width = hints.width;
        sizeHints.min_height = sizeHints.max_height = hints.height;
        XSetWMNormalHints(s.display, m_window, &sizeHints);
    }

    if (m_useEgl) {
        createEglContext(hints, error);
        if (error.isError()) {
            return;
        }
    } else {
        auto createContext = loadGLX<CreateContextAttribsProc>("glXCreateContextAttribsARB");
        if (!createContext) {
            error = makeError(ErrorCode::ApiUnavailable,
                              "glXCreateContextAttribsARB ist nicht verfuegbar.");
            return;
        }

    std::vector<int> contextAttribs;
    contextAttribs.push_back(GLX_CONTEXT_MAJOR_VERSION_ARB);
    contextAttribs.push_back(hints.contextVersionMajor);
    contextAttribs.push_back(GLX_CONTEXT_MINOR_VERSION_ARB);
    contextAttribs.push_back(hints.contextVersionMinor);

    int profileMask = 0;
    if (hints.contextProfile == ContextProfile::Core) {
        profileMask = GLX_CONTEXT_CORE_PROFILE_BIT_ARB;
    } else if (hints.contextProfile == ContextProfile::Compatibility) {
        profileMask = GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB;
    }
    if (profileMask != 0) {
        contextAttribs.push_back(GLX_CONTEXT_PROFILE_MASK_ARB);
        contextAttribs.push_back(profileMask);
    }

    int flags = 0;
    if (hints.contextDebug) {
        flags |= GLX_CONTEXT_DEBUG_BIT_ARB;
    }
    if (hints.contextForwardCompat) {
        flags |= GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB;
    }
    if (hints.contextRobustness != ContextRobustness::None) {
        flags |= GLX_CONTEXT_ROBUST_ACCESS_BIT_ARB;
    }
    if (flags != 0) {
        contextAttribs.push_back(GLX_CONTEXT_FLAGS_ARB);
        contextAttribs.push_back(flags);
    }
    if (hints.contextRobustness != ContextRobustness::None) {
        contextAttribs.push_back(GLX_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB);
        contextAttribs.push_back(hints.contextRobustness == ContextRobustness::LoseContextOnReset
                                     ? GLX_LOSE_CONTEXT_ON_RESET_ARB
                                     : GLX_NO_RESET_NOTIFICATION_ARB);
    }
    contextAttribs.push_back(X11None);

    m_glxContext = createContext(s.display, config, nullptr, True, contextAttribs.data());
    if (!m_glxContext) {
        error = makeError(ErrorCode::VersionUnavailable,
                          "Der angeforderte OpenGL-Kontext konnte nicht erstellt werden.");
        return;
    }
    m_glx = true;

    m_glxWindow = glXCreateWindow(s.display, config, m_window, nullptr);
    if (m_glxWindow == 0) {
        error = makeError(ErrorCode::PlatformError, "GLX-Fenster konnte nicht erstellt werden.");
        return;
    }
    }

    if (s.im) {
        m_ic = XCreateIC(s.im, XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
                         XNClientWindow, m_window, XNFocusWindow, m_window, nullptr);
    }

    registerWindow(this, m_window);

    if (hints.visible) {
        show();
    }
    if (hints.maximized) {
        maximize();
    }
    if (hints.focused) {
        focus();
    }
}

X11WindowBackend::~X11WindowBackend() {
    destroy();
}

void X11WindowBackend::createEglContext(const WindowHints& hints, Error& error) {
    m_eglSurface = eglCreateWindowSurface(m_eglDisplay, m_eglConfig,
                                          static_cast<EGLNativeWindowType>(m_window), nullptr);
    if (m_eglSurface == EGL_NO_SURFACE) {
        error = makeError(ErrorCode::PlatformError, "EGL-Surface konnte nicht erstellt werden.");
        return;
    }

    std::vector<EGLint> attribs;
    attribs.push_back(EGL_CONTEXT_MAJOR_VERSION);
    attribs.push_back(hints.contextVersionMajor);
    attribs.push_back(EGL_CONTEXT_MINOR_VERSION);
    attribs.push_back(hints.contextVersionMinor);
    if (hints.contextProfile == ContextProfile::Core) {
        attribs.push_back(EGL_CONTEXT_OPENGL_PROFILE_MASK);
        attribs.push_back(EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT);
    } else if (hints.contextProfile == ContextProfile::Compatibility) {
        attribs.push_back(EGL_CONTEXT_OPENGL_PROFILE_MASK);
        attribs.push_back(EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT);
    }
    if (hints.contextForwardCompat) {
        attribs.push_back(EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE);
        attribs.push_back(EGL_TRUE);
    }
    if (hints.contextDebug) {
        attribs.push_back(EGL_CONTEXT_OPENGL_DEBUG);
        attribs.push_back(EGL_TRUE);
    }
    if (hints.contextRobustness != ContextRobustness::None) {
        attribs.push_back(EGL_CONTEXT_OPENGL_ROBUST_ACCESS);
        attribs.push_back(EGL_TRUE);
    }
    attribs.push_back(EGL_NONE);

    m_eglContext = eglCreateContext(m_eglDisplay, m_eglConfig, EGL_NO_CONTEXT, attribs.data());
    if (m_eglContext == EGL_NO_CONTEXT) {
        error = makeError(ErrorCode::VersionUnavailable,
                          "Der angeforderte OpenGL-Kontext konnte nicht erstellt werden.");
        return;
    }
    m_glx = false;
}

void X11WindowBackend::destroy() {
    State& s = state();
    if (!s.display || m_window == 0) {
        return;
    }

    if (m_ic) {
        XDestroyIC(m_ic);
        m_ic = nullptr;
    }
    if (m_eglDisplay != EGL_NO_DISPLAY) {
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
    if (m_glxContext) {
        if (glXGetCurrentContext() == m_glxContext) {
            glXMakeCurrent(s.display, X11None, nullptr);
        }
        if (m_glxWindow) {
            glXDestroyWindow(s.display, m_glxWindow);
            m_glxWindow = 0;
        }
        glXDestroyContext(s.display, m_glxContext);
        m_glxContext = nullptr;
    }
    unregisterWindow(m_window);
    XDestroyWindow(s.display, m_window);
    m_window = 0;
    if (m_colormap) {
        XFreeColormap(s.display, m_colormap);
        m_colormap = 0;
    }
    XFlush(s.display);
}

void X11WindowBackend::makeContextCurrent() {
    if (m_useEgl) {
        eglMakeCurrent(m_eglDisplay, m_eglSurface, m_eglSurface, m_eglContext);
    } else {
        glXMakeCurrent(state().display, m_glxWindow, m_glxContext);
    }
}

void X11WindowBackend::swapBuffers() {
    if (m_useEgl) {
        eglSwapBuffers(m_eglDisplay, m_eglSurface);
    } else {
        glXSwapBuffers(state().display, m_glxWindow);
    }
}

void* X11WindowBackend::getProcAddress(const char* name) {
    if (m_useEgl) {
        return reinterpret_cast<void*>(eglGetProcAddress(name));
    }
    return reinterpret_cast<void*>(glXGetProcAddressARB(reinterpret_cast<const GLubyte*>(name)));
}

NativeHandle X11WindowBackend::getNativeHandle() const {
    NativeHandle handle;
    handle.platform = NativePlatform::X11;
    handle.display = state().display;
    handle.window = reinterpret_cast<void*>(static_cast<uintptr_t>(m_window));
    handle.context = m_useEgl ? static_cast<void*>(m_eglContext) : m_glxContext;
    handle.surface = m_useEgl ? static_cast<void*>(m_eglSurface)
                              : reinterpret_cast<void*>(static_cast<uintptr_t>(m_glxWindow));
    handle.eglDisplay = m_eglDisplay;
    return handle;
}

void X11WindowBackend::setSwapInterval(i32 interval) {
    if (m_useEgl) {
        if (m_eglDisplay != EGL_NO_DISPLAY) {
            eglSwapInterval(m_eglDisplay, interval);
        }
        m_swapInterval = interval;
        return;
    }
    if (auto swapIntervalExt = loadGLX<SwapIntervalExtProc>("glXSwapIntervalEXT")) {
        swapIntervalExt(state().display, m_glxWindow, interval);
        m_swapInterval = interval;
        return;
    }
    if (auto swapIntervalMesa = loadGLX<SwapIntervalMesaProc>("glXSwapIntervalMESA")) {
        swapIntervalMesa(static_cast<unsigned int>(interval));
        m_swapInterval = interval;
        return;
    }
    if (auto swapIntervalSgi = loadGLX<SwapIntervalSgiProc>("glXSwapIntervalSGI")) {
        swapIntervalSgi(interval);
        m_swapInterval = interval;
    }
}

bool X11WindowBackend::isExtensionSupported(const char* name) {
    Display* display = state().display;
    if (m_useEgl) {
        const char* eglExtensions = eglQueryString(m_eglDisplay, EGL_EXTENSIONS);
        if (eglExtensions && std::strstr(eglExtensions, name)) {
            return true;
        }
        using GetStringiProc = const unsigned char* (*)(unsigned int, unsigned int);
        using GetIntegervProc = void (*)(unsigned int, int*);
        auto getStringi = reinterpret_cast<GetStringiProc>(eglGetProcAddress("glGetStringi"));
        auto getIntegerv = reinterpret_cast<GetIntegervProc>(eglGetProcAddress("glGetIntegerv"));
        if (!getStringi || !getIntegerv) {
            return false;
        }
        int count = 0;
        getIntegerv(0x821D, &count);
        for (int i = 0; i < count; ++i) {
            const auto* extension = reinterpret_cast<const char*>(
                getStringi(0x1F03, static_cast<unsigned int>(i)));
            if (extension && std::strcmp(extension, name) == 0) {
                return true;
            }
        }
        return false;
    }

    const char* glxExtensions = glXQueryExtensionsString(display, state().screen);
    if (glxExtensions && std::strstr(glxExtensions, name)) {
        return true;
    }

    using GetStringProc = const unsigned char* (*)(unsigned int);
    using GetStringiProc = const unsigned char* (*)(unsigned int, unsigned int);
    using GetIntegervProc = void (*)(unsigned int, int*);
    auto getString = loadGLX<GetStringProc>("glGetString");
    auto getStringi = loadGLX<GetStringiProc>("glGetStringi");
    auto getIntegerv = loadGLX<GetIntegervProc>("glGetIntegerv");
    if (!getString || !getStringi || !getIntegerv) {
        return false;
    }

    constexpr unsigned int GL_EXTENSIONS_ENUM = 0x1F03;
    constexpr unsigned int GL_NUM_EXTENSIONS_ENUM = 0x821D;

    int count = 0;
    getIntegerv(GL_NUM_EXTENSIONS_ENUM, &count);
    for (int i = 0; i < count; ++i) {
        const auto* extension = reinterpret_cast<const char*>(getStringi(GL_EXTENSIONS_ENUM,
                                                                         static_cast<unsigned int>(i)));
        if (extension && std::strcmp(extension, name) == 0) {
            return true;
        }
    }
    (void)getString;
    return false;
}

void X11WindowBackend::setTitle(std::string_view title) {
    m_title = std::string(title);
    setWindowName(m_window, m_title);
}

void X11WindowBackend::setSize(ivec2 size) {
    XResizeWindow(state().display, m_window,
                  static_cast<unsigned int>(std::max(size.x, 1)),
                  static_cast<unsigned int>(std::max(size.y, 1)));
}

void X11WindowBackend::setPosition(ivec2 position) {
    XMoveWindow(state().display, m_window, position.x, position.y);
}

ivec2 X11WindowBackend::getPosition() const {
    XWindowAttributes attributes{};
    if (XGetWindowAttributes(state().display, m_window, &attributes)) {
        return ivec2(attributes.x, attributes.y);
    }
    return m_position;
}

ivec4 X11WindowBackend::getFrameSize() const {
    State& s = state();
    Atom actualType = 0;
    int actualFormat = 0;
    unsigned long count = 0;
    unsigned long remaining = 0;
    unsigned char* data = nullptr;
    if (XGetWindowProperty(s.display, m_window, s.netFrameExtents, 0, 4, False, XA_CARDINAL,
                           &actualType, &actualFormat, &count, &remaining, &data) == Success &&
        data && count >= 4) {
        const auto* values = reinterpret_cast<unsigned long*>(data);
        const ivec4 result(static_cast<i32>(values[0]), static_cast<i32>(values[2]),
                           static_cast<i32>(values[1]), static_cast<i32>(values[3]));
        XFree(data);
        return result;
    }
    if (data) {
        XFree(data);
    }
    return ivec4(0, 0, 0, 0);
}

vec2 X11WindowBackend::getContentScale() const {
    return {1.0f, 1.0f};
}

void X11WindowBackend::show() {
    if (m_visible) {
        return;
    }
    XMapRaised(state().display, m_window);
    m_visible = true;
}

void X11WindowBackend::hide() {
    if (!m_visible) {
        return;
    }
    XUnmapWindow(state().display, m_window);
    m_visible = false;
}

void X11WindowBackend::focus() {
    State& s = state();
    XRaiseWindow(s.display, m_window);

    XEvent event{};
    event.type = ClientMessage;
    event.xclient.window = m_window;
    event.xclient.message_type = XInternAtom(s.display, "_NET_ACTIVE_WINDOW", False);
    event.xclient.format = 32;
    event.xclient.data.l[0] = 1;
    event.xclient.data.l[1] = CurrentTime;
    XSendEvent(s.display, s.root, False,
               SubstructureNotifyMask | SubstructureRedirectMask, &event);

    if (m_mapped) {
        XSetInputFocus(s.display, m_window, RevertToParent, CurrentTime);
        m_focusPending = false;
    } else {
        m_focusPending = true;
    }
}

void X11WindowBackend::requestAttention() {
    State& s = state();
    XEvent event{};
    event.type = ClientMessage;
    event.xclient.window = m_window;
    event.xclient.message_type = s.netWmState;
    event.xclient.format = 32;
    event.xclient.data.l[0] = 1;
    event.xclient.data.l[1] = static_cast<long>(s.netWmStateDemandsAttention);
    event.xclient.data.l[2] = 0;
    event.xclient.data.l[3] = 1;
    XSendEvent(s.display, s.root, False,
               SubstructureNotifyMask | SubstructureRedirectMask, &event);
}

void X11WindowBackend::iconify() {
    XIconifyWindow(state().display, m_window, state().screen);
}

void X11WindowBackend::restore() {
    State& s = state();
    XMapWindow(s.display, m_window);
    if (m_maximized) {
        XEvent event{};
        event.type = ClientMessage;
        event.xclient.window = m_window;
        event.xclient.message_type = s.netWmState;
        event.xclient.format = 32;
        event.xclient.data.l[0] = 0;
        event.xclient.data.l[1] = static_cast<long>(s.netWmStateMaximizedVert);
        event.xclient.data.l[2] = static_cast<long>(s.netWmStateMaximizedHorz);
        event.xclient.data.l[3] = 1;
        XSendEvent(s.display, s.root, False,
                   SubstructureNotifyMask | SubstructureRedirectMask, &event);
        m_maximized = false;
    }
    m_iconified = false;
    m_visible = true;
}

void X11WindowBackend::maximize() {
    State& s = state();
    XEvent event{};
    event.type = ClientMessage;
    event.xclient.window = m_window;
    event.xclient.message_type = s.netWmState;
    event.xclient.format = 32;
    event.xclient.data.l[0] = 1;
    event.xclient.data.l[1] = static_cast<long>(s.netWmStateMaximizedVert);
    event.xclient.data.l[2] = static_cast<long>(s.netWmStateMaximizedHorz);
    event.xclient.data.l[3] = 1;
    XSendEvent(s.display, s.root, False,
               SubstructureNotifyMask | SubstructureRedirectMask, &event);
    m_maximized = true;
}

i32 X11WindowBackend::getAttrib(WindowAttrib attrib) const {
    switch (attrib) {
    case WindowAttrib::Focused: return m_focused ? 1 : 0;
    case WindowAttrib::Iconified: return m_iconified ? 1 : 0;
    case WindowAttrib::Resizable: return m_resizable ? 1 : 0;
    case WindowAttrib::Visible: return m_visible ? 1 : 0;
    case WindowAttrib::Decorated: return m_decorated ? 1 : 0;
    case WindowAttrib::AutoIconify: return m_autoIconify ? 1 : 0;
    case WindowAttrib::Floating: return m_floating ? 1 : 0;
    case WindowAttrib::Maximized: return m_maximized ? 1 : 0;
    case WindowAttrib::Hovered: return m_hovered ? 1 : 0;
    case WindowAttrib::FocusOnShow: return m_focusOnShow ? 1 : 0;
    case WindowAttrib::MousePassthrough: return m_mousePassthrough ? 1 : 0;
    case WindowAttrib::ScaleToMonitor: return m_scaleToMonitor ? 1 : 0;
    case WindowAttrib::Doublebuffer: return m_doublebuffer ? 1 : 0;
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

void X11WindowBackend::setAttrib(WindowAttrib attrib, bool value) {
    State& s = state();
    switch (attrib) {
    case WindowAttrib::Visible:
        value ? show() : hide();
        break;
    case WindowAttrib::Resizable: {
        m_resizable = value;
        XSizeHints hints{};
        if (value) {
            hints.flags = 0;
        } else {
            hints.flags = PMinSize | PMaxSize;
            hints.min_width = hints.max_width = m_size.x;
            hints.min_height = hints.max_height = m_size.y;
        }
        XSetWMNormalHints(s.display, m_window, &hints);
        break;
    }
    case WindowAttrib::Decorated: {
        m_decorated = value;
        struct MotifHints {
            unsigned long flags;
            unsigned long functions;
            unsigned long decorations;
            long inputMode;
            unsigned long status;
        };
        MotifHints hints{};
        hints.flags = 2;
        hints.decorations = value ? 1 : 0;
        XChangeProperty(s.display, m_window, s.motifWmHints, s.motifWmHints, 32,
                        PropModeReplace, reinterpret_cast<const unsigned char*>(&hints), 5);
        break;
    }
    case WindowAttrib::Floating:
        m_floating = value;
        updateNetWmState();
        break;
    case WindowAttrib::AutoIconify:
        m_autoIconify = value;
        break;
    case WindowAttrib::FocusOnShow:
        m_focusOnShow = value;
        break;
    case WindowAttrib::MousePassthrough:
        m_mousePassthrough = value;
        if (value) {
            XRectangle rectangle{0, 0, 0, 0};
            XShapeCombineRectangles(s.display, m_window, ShapeInput, 0, 0, &rectangle, 1,
                                    ShapeSet, Unsorted);
        } else {
            XShapeCombineMask(s.display, m_window, ShapeInput, 0, 0, X11None, ShapeSet);
        }
        break;
    case WindowAttrib::ScaleToMonitor:
        m_scaleToMonitor = value;
        break;
    case WindowAttrib::Maximized:
        value ? maximize() : restore();
        break;
    default:
        break;
    }
}

void X11WindowBackend::updateNetWmState() {
    State& s = state();
    XEvent event{};
    event.type = ClientMessage;
    event.xclient.window = m_window;
    event.xclient.message_type = s.netWmState;
    event.xclient.format = 32;
    event.xclient.data.l[0] = m_floating ? 1 : 0;
    event.xclient.data.l[1] = static_cast<long>(s.netWmStateAbove);
    event.xclient.data.l[2] = 0;
    event.xclient.data.l[3] = 1;
    XSendEvent(s.display, s.root, False,
               SubstructureNotifyMask | SubstructureRedirectMask, &event);
}

void X11WindowBackend::setOpacity(f32 opacity) {
    m_opacity = std::clamp(opacity, 0.0f, 1.0f);
    const unsigned long value =
        static_cast<unsigned long>(m_opacity * static_cast<f32>(0xFFFFFFFFu));
    XChangeProperty(state().display, m_window, state().netWmWindowOpacity, XA_CARDINAL, 32,
                    PropModeReplace, reinterpret_cast<const unsigned char*>(&value), 1);
}

void X11WindowBackend::setSizeLimits(ivec2 minimum, ivec2 maximum) {
    m_sizeLimitsMin = minimum;
    m_sizeLimitsMax = maximum;
    XSizeHints hints{};
    hints.flags = 0;
    if (minimum.x > 0 || minimum.y > 0) {
        hints.flags |= PMinSize;
        hints.min_width = minimum.x;
        hints.min_height = minimum.y;
    }
    if (maximum.x > 0 || maximum.y > 0) {
        hints.flags |= PMaxSize;
        hints.max_width = maximum.x;
        hints.max_height = maximum.y;
    }
    XSetWMNormalHints(state().display, m_window, &hints);
}

void X11WindowBackend::setAspectRatio(i32 numerator, i32 denominator) {
    m_aspectNumerator = numerator;
    m_aspectDenominator = denominator;
    XSizeHints hints{};
    hints.flags = 0;
    if (numerator > 0 && denominator > 0) {
        hints.flags |= PAspect;
        hints.min_aspect.x = numerator;
        hints.min_aspect.y = denominator;
        hints.max_aspect.x = numerator;
        hints.max_aspect.y = denominator;
    }
    XSetWMNormalHints(state().display, m_window, &hints);
}

void X11WindowBackend::setIcon(std::span<const Image> images) {
    State& s = state();
    m_icon.assign(images.begin(), images.end());
    m_iconStorage.clear();

    std::vector<unsigned long> data;
    for (const auto& image : images) {
        if (image.size.x <= 0 || image.size.y <= 0) {
            continue;
        }
        const auto pixels = static_cast<std::size_t>(image.size.x) *
                            static_cast<std::size_t>(image.size.y);
        if (image.rgba8.size() < pixels * 4) {
            continue;
        }
        data.push_back(static_cast<unsigned long>(image.size.x));
        data.push_back(static_cast<unsigned long>(image.size.y));
        for (std::size_t i = 0; i < pixels; ++i) {
            const u8 r = image.rgba8[i * 4 + 0];
            const u8 g = image.rgba8[i * 4 + 1];
            const u8 b = image.rgba8[i * 4 + 2];
            const u8 a = image.rgba8[i * 4 + 3];
            data.push_back((static_cast<unsigned long>(a) << 24) |
                           (static_cast<unsigned long>(r) << 16) |
                           (static_cast<unsigned long>(g) << 8) |
                           static_cast<unsigned long>(b));
        }
    }

    if (data.empty()) {
        XDeleteProperty(s.display, m_window, s.netWmIcon);
    } else {
        XChangeProperty(s.display, m_window, s.netWmIcon, XA_CARDINAL, 32, PropModeReplace,
                        reinterpret_cast<const unsigned char*>(data.data()),
                        static_cast<int>(data.size()));
    }
}

void X11WindowBackend::setMonitor(const Monitor& monitor, ivec2 position, ivec2 size,
                                  i32 refreshRate) {
    (void)refreshRate;
    State& s = state();

    if (!monitor.isValid() || size.x <= 0 || size.y <= 0) {
        if (m_fullscreen) {
            XEvent event{};
            event.type = ClientMessage;
            event.xclient.window = m_window;
            event.xclient.message_type = s.netWmState;
            event.xclient.format = 32;
            event.xclient.data.l[0] = 0;
            event.xclient.data.l[1] = static_cast<long>(s.netWmStateFullscreen);
            event.xclient.data.l[2] = 0;
            event.xclient.data.l[3] = 1;
            XSendEvent(s.display, s.root, False,
                       SubstructureNotifyMask | SubstructureRedirectMask, &event);
            XMoveResizeWindow(s.display, m_window, m_windowedPosition.x, m_windowedPosition.y,
                              static_cast<unsigned int>(std::max(m_windowedSize.x, 1)),
                              static_cast<unsigned int>(std::max(m_windowedSize.y, 1)));
            m_restoreWindowedGeometry = true;
            m_fullscreen = false;
            m_fullscreenMonitor = nullptr;
        }
        return;
    }

    if (!m_fullscreen) {
        m_windowedPosition = getPosition();
        m_windowedSize = m_size;
    }
    m_restoreWindowedGeometry = false;
    m_fullscreen = true;
    m_fullscreenMonitor = MonitorAccess::native(monitor);

    XEvent event{};
    event.type = ClientMessage;
    event.xclient.window = m_window;
    event.xclient.message_type = s.netWmState;
    event.xclient.format = 32;
    event.xclient.data.l[0] = 1;
    event.xclient.data.l[1] = static_cast<long>(s.netWmStateFullscreen);
    event.xclient.data.l[2] = 0;
    event.xclient.data.l[3] = 1;
    XSendEvent(s.display, s.root, False,
               SubstructureNotifyMask | SubstructureRedirectMask, &event);

    XMoveResizeWindow(s.display, m_window, position.x, position.y,
                      static_cast<unsigned int>(std::max(size.x, 1)),
                      static_cast<unsigned int>(std::max(size.y, 1)));
}

Monitor X11WindowBackend::getMonitor() const {
    State& s = state();
    if (m_fullscreen && m_fullscreenMonitor) {
        for (const auto& monitor : s.monitors) {
            if (MonitorAccess::native(monitor) == m_fullscreenMonitor) {
                return monitor;
            }
        }
    }

    const ivec2 position = getPosition();
    const ivec2 center = position + m_size / 2;
    for (const auto& monitor : s.monitors) {
        const ivec2 monitorPos = monitor.getPosition();
        const Monitor::Workarea workarea = monitor.getWorkarea();
        (void)workarea;
        const VideoMode mode = monitor.getVideoMode();
        if (center.x >= monitorPos.x && center.y >= monitorPos.y &&
            center.x < monitorPos.x + mode.size.x && center.y < monitorPos.y + mode.size.y) {
            return monitor;
        }
    }
    return {};
}

} // namespace sgl::detail::x11
