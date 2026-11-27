#include "X11Internal.hpp"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <sys/select.h>
#include <unistd.h>

#include <X11/extensions/Xinerama.h>

#include "backend/BackendFactory.hpp"
#include "common/LinuxJoystick.hpp"
#include "common/MonitorAccess.hpp"

namespace sgl::detail::x11 {
namespace {

State g_state;

Atom intern(Display* display, const char* name) {
    return XInternAtom(display, name, False);
}

int ignoreXError(Display*, XErrorEvent*) {
    return 0;
}

RROutput toOutput(const Monitor& monitor) {
    return static_cast<RROutput>(reinterpret_cast<uintptr_t>(MonitorAccess::native(monitor)));
}

Monitor makeMonitor(RROutput output) {
    return MonitorAccess::make(reinterpret_cast<void*>(static_cast<uintptr_t>(output)), Platform::X11);
}

struct XrandrMonitor {
    RROutput output = 0;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

std::vector<XrandrMonitor> queryXrandrMonitors(Display* display, ::Window root) {
    std::vector<XrandrMonitor> result;
    int count = 0;
    XRRMonitorInfo* monitors = XRRGetMonitors(display, root, True, &count);
    if (monitors) {
        for (int i = 0; i < count; ++i) {
            XrandrMonitor entry;
            entry.x = monitors[i].x;
            entry.y = monitors[i].y;
            entry.width = monitors[i].width;
            entry.height = monitors[i].height;
            if (monitors[i].noutput > 0) {
                entry.output = monitors[i].outputs[0];
            }
            if (entry.output != 0) {
                result.push_back(entry);
            }
        }
        XRRFreeMonitors(monitors);
    }
    return result;
}

} // namespace

State& state() {
    return g_state;
}

void rebuildMonitors(bool notify) {
    State& s = g_state;
    if (!s.display || !s.hasXrandr) {
        return;
    }

    std::vector<Monitor> newMonitors;
    std::vector<void*> newHandles;
    for (const auto& entry : queryXrandrMonitors(s.display, s.root)) {
        newMonitors.push_back(makeMonitor(entry.output));
        newHandles.push_back(reinterpret_cast<void*>(static_cast<uintptr_t>(entry.output)));
    }

    if (notify && s.monitorCallback) {
        for (auto* handle : s.monitorHandles) {
            if (std::find(newHandles.begin(), newHandles.end(), handle) == newHandles.end()) {
                s.monitorCallback(makeMonitor(static_cast<RROutput>(reinterpret_cast<uintptr_t>(handle))), false);
            }
        }
        for (auto* handle : newHandles) {
            if (std::find(s.monitorHandles.begin(), s.monitorHandles.end(), handle) == s.monitorHandles.end()) {
                s.monitorCallback(makeMonitor(static_cast<RROutput>(reinterpret_cast<uintptr_t>(handle))), true);
            }
        }
    }

    s.monitors = std::move(newMonitors);
    s.monitorHandles = std::move(newHandles);
}

void processPendingEvents();

class X11Backend final : public Backend {
public:
    bool init(const InitOptions&, Error& error) override;
    void terminate() override;
    [[nodiscard]] Platform getPlatform() const override { return Platform::X11; }

    std::unique_ptr<WindowBackend> createWindow(const WindowHints& hints, Error& error) override;
    std::unique_ptr<CursorBackend> createCursor(const Image& image, ivec2 hotspot, Error& error) override;
    std::unique_ptr<CursorBackend> createStandardCursor(StandardCursor shape, Error& error) override;

    [[nodiscard]] std::vector<Monitor> getMonitors() override { return state().monitors; }
    [[nodiscard]] Monitor getPrimaryMonitor() override {
        if (state().hasXrandr && state().display) {
            const RROutput primary = XRRGetOutputPrimary(state().display, state().root);
            if (primary != 0) {
                return makeMonitor(primary);
            }
        }
        if (!state().monitors.empty()) {
            return state().monitors.front();
        }
        return {};
    }

    void pollEvents() override;
    void waitEvents(f64 timeout) override;
    void postEmptyEvent() override;

    [[nodiscard]] bool isJoystickPresent(i32 jid) override {
        return linuxJoysticks().isPresent(jid);
    }
    [[nodiscard]] std::string getJoystickName(i32 jid) override {
        return linuxJoysticks().getName(jid);
    }
    [[nodiscard]] std::string getJoystickGUID(i32 jid) override {
        return linuxJoysticks().getGuid(jid);
    }
    [[nodiscard]] std::span<const f32> getJoystickAxes(i32 jid) override {
        return linuxJoysticks().getAxes(jid);
    }
    [[nodiscard]] std::span<const u8> getJoystickButtons(i32 jid) override {
        return linuxJoysticks().getButtons(jid);
    }
    [[nodiscard]] std::span<const JoystickHat> getJoystickHats(i32 jid) override {
        return linuxJoysticks().getHats(jid);
    }
    void setJoystickUserPointer(i32 jid, void* pointer) override {
        linuxJoysticks().setUserPointer(jid, pointer);
    }
    [[nodiscard]] void* getJoystickUserPointer(i32 jid) override {
        return linuxJoysticks().getUserPointer(jid);
    }
    [[nodiscard]] bool isGamepad(i32 jid) override { return linuxJoysticks().isGamepad(jid); }
    [[nodiscard]] std::string getGamepadName(i32 jid) override {
        return linuxJoysticks().getGamepadName(jid);
    }
    [[nodiscard]] std::optional<GamepadState> getGamepadState(i32 jid) override {
        return linuxJoysticks().getGamepadState(jid);
    }
    bool updateGamepadMappings(std::string_view mappings) override {
        return linuxJoysticks().updateGamepadMappings(mappings);
    }
    void setJoystickCallback(std::function<void(i32, bool)> callback) override {
        linuxJoysticks().setCallback(std::move(callback));
    }

    void setMonitorCallback(MonitorCallback callback) override {
        state().monitorCallback = std::move(callback);
    }

    [[nodiscard]] bool isMonitorConnected(const Monitor& monitor) override {
        auto* handle = MonitorAccess::native(monitor);
        return std::find(state().monitorHandles.begin(), state().monitorHandles.end(), handle) !=
               state().monitorHandles.end();
    }

    [[nodiscard]] ivec2 getMonitorPosition(const Monitor& monitor) override {
        const auto entry = findMonitor(monitor);
        return ivec2(entry.x, entry.y);
    }

    [[nodiscard]] Monitor::Workarea getMonitorWorkarea(const Monitor& monitor) override {
        Monitor::Workarea area;
        const auto entry = findMonitor(monitor);
        area.position = ivec2(entry.x, entry.y);
        area.size = ivec2(entry.width, entry.height);

        Atom actualType = 0;
        int actualFormat = 0;
        unsigned long count = 0;
        unsigned long remaining = 0;
        unsigned char* data = nullptr;
        if (XGetWindowProperty(state().display, state().root, state().netWorkarea, 0, 4, False,
                               XA_CARDINAL, &actualType, &actualFormat, &count, &remaining,
                               &data) == Success &&
            data && count >= 4) {
            const auto* values = reinterpret_cast<unsigned long*>(data);
            const ivec2 desktopPos(static_cast<i32>(values[0]), static_cast<i32>(values[1]));
            const ivec2 desktopSize(static_cast<i32>(values[2]), static_cast<i32>(values[3]));
            const ivec2 intersectMin = glm::max(area.position, desktopPos);
            const ivec2 intersectMax = glm::min(area.position + area.size, desktopPos + desktopSize);
            if (intersectMax.x > intersectMin.x && intersectMax.y > intersectMin.y) {
                area.position = intersectMin;
                area.size = intersectMax - intersectMin;
            }
        }
        if (data) {
            XFree(data);
        }
        return area;
    }

    [[nodiscard]] ivec2 getMonitorPhysicalSize(const Monitor& monitor) override {
        Display* display = state().display;
        XRRScreenResources* resources = screenResources();
        XRROutputInfo* info = resources ? XRRGetOutputInfo(display, resources, toOutput(monitor)) : nullptr;
        ivec2 size(0, 0);
        if (info) {
            size = ivec2(static_cast<i32>(info->mm_width), static_cast<i32>(info->mm_height));
            XRRFreeOutputInfo(info);
        }
        if (resources) {
            XRRFreeScreenResources(resources);
        }
        return size;
    }

    [[nodiscard]] vec2 getMonitorContentScale(const Monitor&) override {
        return {1.0f, 1.0f};
    }

    [[nodiscard]] std::string getMonitorName(const Monitor& monitor) override {
        Display* display = state().display;
        XRRScreenResources* resources = screenResources();
        XRROutputInfo* info = resources ? XRRGetOutputInfo(display, resources, toOutput(monitor)) : nullptr;
        std::string name;
        if (info) {
            if (info->name) {
                name.assign(info->name, static_cast<std::size_t>(info->nameLen));
            }
            XRRFreeOutputInfo(info);
        }
        if (resources) {
            XRRFreeScreenResources(resources);
        }
        return name;
    }

    [[nodiscard]] std::vector<VideoMode> getVideoModes(const Monitor&) override {
        std::vector<VideoMode> modes;
        XRRScreenResources* resources = screenResources();
        if (!resources) {
            return modes;
        }
        modes.reserve(static_cast<std::size_t>(resources->nmode));
        for (int i = 0; i < resources->nmode; ++i) {
            const XRRModeInfo& mode = resources->modes[i];
            if (mode.width == 0 || mode.height == 0) {
                continue;
            }
            i32 refresh = 0;
            const double total = static_cast<double>(mode.hTotal) * static_cast<double>(mode.vTotal);
            if (total > 0.0 && mode.dotClock > 0) {
                refresh = static_cast<i32>(std::lround(static_cast<double>(mode.dotClock) / total));
            }
            VideoMode videoMode;
            videoMode.size = ivec2(static_cast<i32>(mode.width), static_cast<i32>(mode.height));
            videoMode.refreshRate = refresh;
            modes.push_back(videoMode);
        }
        XRRFreeScreenResources(resources);
        return modes;
    }

    [[nodiscard]] VideoMode getVideoMode(const Monitor& monitor) override {
        VideoMode result;
        const auto entry = findMonitor(monitor);
        XRRScreenResources* resources = screenResources();
        if (!resources) {
            return result;
        }
        XRRCrtcInfo* crtc = nullptr;
        XRROutputInfo* output = XRRGetOutputInfo(state().display, resources, toOutput(monitor));
        if (output && output->crtc != 0) {
            crtc = XRRGetCrtcInfo(state().display, resources, output->crtc);
        }
        if (crtc) {
            result.size = ivec2(static_cast<i32>(crtc->width), static_cast<i32>(crtc->height));
            for (int i = 0; i < resources->nmode; ++i) {
                if (resources->modes[i].id == crtc->mode) {
                    const XRRModeInfo& mode = resources->modes[i];
                    const double total =
                        static_cast<double>(mode.hTotal) * static_cast<double>(mode.vTotal);
                    if (total > 0.0 && mode.dotClock > 0) {
                        result.refreshRate = static_cast<i32>(
                            std::lround(static_cast<double>(mode.dotClock) / total));
                    }
                    break;
                }
            }
            XRRFreeCrtcInfo(crtc);
        } else {
            result.size = ivec2(entry.width, entry.height);
        }
        if (output) {
            XRRFreeOutputInfo(output);
        }
        XRRFreeScreenResources(resources);
        if (result.size.x == 0 || result.size.y == 0) {
            result.size = ivec2(entry.width, entry.height);
        }
        return result;
    }

    void setGamma(const Monitor& monitor, f32 gamma) override {
        if (gamma <= 0.0f) {
            reportError(ErrorCode::InvalidValue, "Gamma muss groesser als 0 sein.");
            return;
        }
        Display* display = state().display;
        RRCrtc crtcId = 0;
        XRRCrtcInfo* crtc = findCrtc(monitor, crtcId);
        if (!crtc) {
            return;
        }
        const int size = XRRGetCrtcGammaSize(display, crtcId);
        XRRCrtcGamma* ramp = XRRAllocGamma(size);
        if (ramp) {
            for (int i = 0; i < size; ++i) {
                const double value = std::pow(static_cast<double>(i) / (size - 1), 1.0 / gamma);
                ramp->red[i] = static_cast<unsigned short>(std::lround(value * 65535.0));
                ramp->green[i] = ramp->red[i];
                ramp->blue[i] = ramp->red[i];
            }
            XRRSetCrtcGamma(display, crtcId, ramp);
            XRRFreeGamma(ramp);
        }
        XRRFreeCrtcInfo(crtc);
    }

    [[nodiscard]] GammaRamp getGammaRamp(const Monitor& monitor) override {
        GammaRamp result;
        Display* display = state().display;
        RRCrtc crtcId = 0;
        XRRCrtcInfo* crtc = findCrtc(monitor, crtcId);
        if (!crtc) {
            return result;
        }
        XRRCrtcGamma* ramp = XRRGetCrtcGamma(display, crtcId);
        if (ramp) {
            const int size = ramp->size;
            for (int i = 0; i < 256; ++i) {
                const int index = size > 1 ? (i * (size - 1)) / 255 : 0;
                result.red[static_cast<std::size_t>(i)] = ramp->red[index];
                result.green[static_cast<std::size_t>(i)] = ramp->green[index];
                result.blue[static_cast<std::size_t>(i)] = ramp->blue[index];
            }
            XRRFreeGamma(ramp);
        }
        XRRFreeCrtcInfo(crtc);
        return result;
    }

    void setGammaRamp(const Monitor& monitor, const GammaRamp& ramp) override {
        Display* display = state().display;
        RRCrtc crtcId = 0;
        XRRCrtcInfo* crtc = findCrtc(monitor, crtcId);
        if (!crtc) {
            return;
        }
        const int size = XRRGetCrtcGammaSize(display, crtcId);
        XRRCrtcGamma* nativeRamp = XRRAllocGamma(size);
        if (nativeRamp) {
            for (int i = 0; i < size; ++i) {
                const int index = size > 1 ? (i * 255) / (size - 1) : 0;
                nativeRamp->red[i] = ramp.red[static_cast<std::size_t>(index)];
                nativeRamp->green[i] = ramp.green[static_cast<std::size_t>(index)];
                nativeRamp->blue[i] = ramp.blue[static_cast<std::size_t>(index)];
            }
            XRRSetCrtcGamma(display, crtcId, nativeRamp);
            XRRFreeGamma(nativeRamp);
        }
        XRRFreeCrtcInfo(crtc);
    }

    [[nodiscard]] NativeHandle getMonitorNativeHandle(const Monitor& monitor) override {
        NativeHandle handle;
        handle.platform = NativePlatform::X11;
        handle.display = state().display;
        handle.window = MonitorAccess::native(monitor);
        return handle;
    }

    void registerWindow(::Window window, X11WindowBackend* backend) {
        m_windows[window] = backend;
    }
    void unregisterWindow(::Window window) {
        m_windows.erase(window);
    }
    X11WindowBackend* findWindow(::Window window) const {
        const auto it = m_windows.find(window);
        return it == m_windows.end() ? nullptr : it->second;
    }
    X11WindowBackend* focusedWindow() const {
        for (auto& [window, backend] : m_windows) {
            (void)window;
            if (backend->getAttrib(WindowAttrib::Focused) != 0) {
                return backend;
            }
        }
        return m_windows.empty() ? nullptr : m_windows.begin()->second;
    }

private:
    XrandrMonitor findMonitor(const Monitor& monitor) const {
        const RROutput output = toOutput(monitor);
        if (state().display) {
            for (const auto& entry : queryXrandrMonitors(state().display, state().root)) {
                if (entry.output == output) {
                    return entry;
                }
            }
        }
        XrandrMonitor fallback;
        if (!state().monitors.empty()) {
            for (const auto& entry : queryXrandrMonitors(state().display, state().root)) {
                return entry;
            }
        }
        return fallback;
    }

    XRRScreenResources* screenResources() {
        return XRRGetScreenResourcesCurrent(state().display, state().root);
    }

    XRRCrtcInfo* findCrtc(const Monitor& monitor, RRCrtc& crtcId) {
        XRRScreenResources* resources = screenResources();
        if (!resources) {
            return nullptr;
        }
        XRROutputInfo* output = XRRGetOutputInfo(state().display, resources, toOutput(monitor));
        XRRCrtcInfo* crtc = nullptr;
        if (output && output->crtc != 0) {
            crtcId = output->crtc;
            crtc = XRRGetCrtcInfo(state().display, resources, output->crtc);
        }
        if (output) {
            XRRFreeOutputInfo(output);
        }
        XRRFreeScreenResources(resources);
        return crtc;
    }

    std::unordered_map<::Window, X11WindowBackend*> m_windows;
};

X11Backend* g_backendImpl = nullptr;

bool X11Backend::init(const InitOptions&, Error& error) {
    State& s = g_state;
    if (s.initialized) {
        return true;
    }

    s.display = XOpenDisplay(nullptr);
    if (!s.display) {
        error = makeError(ErrorCode::PlatformUnavailable,
                          "X11-Display konnte nicht geoeffnet werden.");
        return false;
    }

    s.screen = DefaultScreen(s.display);
    s.root = RootWindow(s.display, s.screen);
    XSetErrorHandler(ignoreXError);

    int eventBase = 0;
    int errorBase = 0;
    s.hasXrandr = XRRQueryExtension(s.display, &eventBase, &errorBase);
    s.rrEventBase = eventBase;
    s.rrErrorBase = errorBase;

    int xineramaEvent = 0;
    int xineramaError = 0;
    s.hasXinerama = XineramaQueryExtension(s.display, &xineramaEvent, &xineramaError);

    s.hasXcursor = XcursorSupportsARGB(s.display);

    int xiMajor = 2;
    int xiMinor = 2;
    s.hasXi2 = XIQueryVersion(s.display, &xiMajor, &xiMinor) == Success;
    if (s.hasXi2) {
        int xiEvent = 0;
        int xiError = 0;
        if (XQueryExtension(s.display, "XInputExtension", &s.xiOpcode, &xiEvent, &xiError)) {
            unsigned char mask[XIMaskLen(XI_LASTEVENT)] = {};
            XIEventMask eventMask;
            eventMask.deviceid = XIAllMasterDevices;
            eventMask.mask_len = sizeof(mask);
            eventMask.mask = mask;
            XISetMask(mask, XI_RawMotion);
            XISelectEvents(s.display, s.root, &eventMask, 1);
        } else {
            s.hasXi2 = false;
        }
    }

    s.wmProtocols = intern(s.display, "WM_PROTOCOLS");
    s.wmDeleteWindow = intern(s.display, "WM_DELETE_WINDOW");
    s.wmTakeFocus = intern(s.display, "WM_TAKE_FOCUS");
    s.netWmName = intern(s.display, "_NET_WM_NAME");
    s.netWmIcon = intern(s.display, "_NET_WM_ICON");
    s.netWmPid = intern(s.display, "_NET_WM_PID");
    s.netWmState = intern(s.display, "_NET_WM_STATE");
    s.netWmStateFullscreen = intern(s.display, "_NET_WM_STATE_FULLSCREEN");
    s.netWmStateMaximizedVert = intern(s.display, "_NET_WM_STATE_MAXIMIZED_VERT");
    s.netWmStateMaximizedHorz = intern(s.display, "_NET_WM_STATE_MAXIMIZED_HORZ");
    s.netWmStateHidden = intern(s.display, "_NET_WM_STATE_HIDDEN");
    s.netWmStateFocused = intern(s.display, "_NET_WM_STATE_FOCUSED");
    s.netWmStateAbove = intern(s.display, "_NET_WM_STATE_ABOVE");
    s.netWmStateDemandsAttention = intern(s.display, "_NET_WM_STATE_DEMANDS_ATTENTION");
    s.netWmWindowOpacity = intern(s.display, "_NET_WM_WINDOW_OPACITY");
    s.netFrameExtents = intern(s.display, "_NET_FRAME_EXTENTS");
    s.netWorkarea = intern(s.display, "_NET_WORKAREA");
    s.motifWmHints = intern(s.display, "_MOTIF_WM_HINTS");
    s.utf8String = intern(s.display, "UTF8_STRING");
    s.clipboard = intern(s.display, "CLIPBOARD");
    s.targets = intern(s.display, "TARGETS");
    s.incr = intern(s.display, "INCR");
    s.atomPair = intern(s.display, "ATOM_PAIR");
    s.deleteAtom = intern(s.display, "DELETE");
    s.sglSelection = intern(s.display, "SGL_SELECTION");
    s.textUriList = intern(s.display, "text/uri-list");
    s.xdndAware = intern(s.display, "XdndAware");
    s.xdndEnter = intern(s.display, "XdndEnter");
    s.xdndPosition = intern(s.display, "XdndPosition");
    s.xdndStatus = intern(s.display, "XdndStatus");
    s.xdndLeave = intern(s.display, "XdndLeave");
    s.xdndDrop = intern(s.display, "XdndDrop");
    s.xdndFinished = intern(s.display, "XdndFinished");
    s.xdndSelection = intern(s.display, "XdndSelection");
    s.xdndTypeList = intern(s.display, "XdndTypeList");

    Bool supported = False;
    XkbSetDetectableAutoRepeat(s.display, True, &supported);
    s.detectableRepeat = supported != False;

    s.im = XOpenIM(s.display, nullptr, nullptr, nullptr);

    if (s.hasXcursor) {
        XcursorImage* image = XcursorImageCreate(1, 1);
        if (image) {
            image->xhot = 0;
            image->yhot = 0;
            image->pixels[0] = 0;
            s.invisibleCursor = XcursorImageLoadCursor(s.display, image);
            XcursorImageDestroy(image);
        }
    }
    if (s.invisibleCursor == 0) {
        Pixmap pixmap = XCreatePixmap(s.display, s.root, 1, 1, 1);
        XColor color{};
        s.invisibleCursor = XCreatePixmapCursor(s.display, pixmap, pixmap, &color, &color, 0, 0);
        XFreePixmap(s.display, pixmap);
    }

    if (pipe(s.pipeFds) == 0) {
        for (int fd : s.pipeFds) {
            fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
            fcntl(fd, F_SETFD, FD_CLOEXEC);
        }
    }

    XSelectInput(s.display, s.root, SubstructureNotifyMask | StructureNotifyMask);
    if (s.hasXrandr) {
        XRRSelectInput(s.display, s.root, RRScreenChangeNotifyMask | RROutputChangeNotifyMask |
                                              RRCrtcChangeNotifyMask);
    }

    s.initialized = true;
    rebuildMonitors(false);
    return true;
}

void X11Backend::terminate() {
    State& s = g_state;
    if (!s.display) {
        return;
    }

    std::vector<X11WindowBackend*> windows;
    windows.reserve(m_windows.size());
    for (auto& [window, backend] : m_windows) {
        (void)window;
        windows.push_back(backend);
    }
    for (auto* window : windows) {
        window->destroy();
    }
    m_windows.clear();

    if (s.im) {
        XCloseIM(s.im);
        s.im = nullptr;
    }
    if (s.invisibleCursor != 0) {
        XFreeCursor(s.display, s.invisibleCursor);
        s.invisibleCursor = 0;
    }
    for (int fd : s.pipeFds) {
        if (fd >= 0) {
            close(fd);
        }
    }
    s.pipeFds[0] = s.pipeFds[1] = -1;

    XCloseDisplay(s.display);
    s.display = nullptr;
    s.initialized = false;
    s.monitors.clear();
    s.monitorHandles.clear();
    s.monitorCallback = nullptr;
}

std::unique_ptr<WindowBackend> X11Backend::createWindow(const WindowHints& hints, Error& error) {
    auto window = std::make_unique<X11WindowBackend>(hints, error);
    if (error.isError()) {
        return nullptr;
    }
    return window;
}

std::unique_ptr<CursorBackend> X11Backend::createCursor(const Image& image, ivec2 hotspot,
                                                            Error& error) {
    if (image.size.x <= 0 || image.size.y <= 0) {
        error = makeError(ErrorCode::InvalidValue, "Cursor-Bild hat eine ungueltige Groesse.");
        return nullptr;
    }
    const auto pixelCount = static_cast<std::size_t>(image.size.x) *
                            static_cast<std::size_t>(image.size.y);
    if (image.rgba8.size() < pixelCount * 4) {
        error = makeError(ErrorCode::InvalidValue, "Cursor-Bild enthaelt zu wenige Pixel.");
        return nullptr;
    }
    if (!state().hasXcursor) {
        error = makeError(ErrorCode::FeatureUnavailable,
                          "Xcursor wird von diesem X-Server nicht unterstuetzt.");
        return nullptr;
    }

    XcursorImage* cursorImage =
        XcursorImageCreate(static_cast<int>(image.size.x), static_cast<int>(image.size.y));
    if (!cursorImage) {
        error = makeError(ErrorCode::OutOfMemory, "Cursor-Bild konnte nicht angelegt werden.");
        return nullptr;
    }
    cursorImage->xhot = static_cast<XcursorDim>(hotspot.x);
    cursorImage->yhot = static_cast<XcursorDim>(hotspot.y);

    for (std::size_t i = 0; i < pixelCount; ++i) {
        const u8 r = image.rgba8[i * 4 + 0];
        const u8 g = image.rgba8[i * 4 + 1];
        const u8 b = image.rgba8[i * 4 + 2];
        const u8 a = image.rgba8[i * 4 + 3];
        cursorImage->pixels[i] = (static_cast<XcursorPixel>(a) << 24) |
                                 (static_cast<XcursorPixel>(r) << 16) |
                                 (static_cast<XcursorPixel>(g) << 8) |
                                 static_cast<XcursorPixel>(b);
    }

    const ::Cursor cursor = XcursorImageLoadCursor(state().display, cursorImage);
    XcursorImageDestroy(cursorImage);
    if (cursor == 0) {
        error = makeError(ErrorCode::PlatformError, "Cursor konnte nicht geladen werden.");
        return nullptr;
    }
    return std::make_unique<X11CursorBackend>(cursor, true);
}

std::unique_ptr<CursorBackend> X11Backend::createStandardCursor(StandardCursor shape,
                                                                    Error& error) {
    unsigned int glyph = XC_left_ptr;
    switch (shape) {
    case StandardCursor::Arrow: glyph = XC_left_ptr; break;
    case StandardCursor::IBeam: glyph = XC_xterm; break;
    case StandardCursor::Crosshair: glyph = XC_crosshair; break;
    case StandardCursor::Hand: glyph = XC_hand2; break;
    case StandardCursor::HResize: glyph = XC_sb_h_double_arrow; break;
    case StandardCursor::VResize: glyph = XC_sb_v_double_arrow; break;
    }
    const ::Cursor cursor = XCreateFontCursor(state().display, glyph);
    if (cursor == 0) {
        error = makeError(ErrorCode::PlatformError, "Standard-Cursor konnte nicht erstellt werden.");
        return nullptr;
    }
    return std::make_unique<X11CursorBackend>(cursor, true);
}

void X11Backend::pollEvents() {
    processPendingEvents();
    linuxJoysticks().poll();
}

void X11Backend::waitEvents(f64 timeout) {
    State& s = g_state;
    processPendingEvents();
    XFlush(s.display);

    if (timeout == 0.0) {
        return;
    }

    const int x11Fd = ConnectionNumber(s.display);
    const int maxFd = std::max(x11Fd, s.pipeFds[0]);

    while (true) {
        if (XPending(s.display) > 0) {
            break;
        }

        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(x11Fd, &readSet);
        if (s.pipeFds[0] >= 0) {
            FD_SET(s.pipeFds[0], &readSet);
        }

        int result = 0;
        if (timeout < 0.0) {
            result = select(maxFd + 1, &readSet, nullptr, nullptr, nullptr);
        } else {
            timeval tv;
            tv.tv_sec = static_cast<long>(timeout);
            tv.tv_usec = static_cast<long>((timeout - static_cast<f64>(tv.tv_sec)) * 1'000'000.0);
            result = select(maxFd + 1, &readSet, nullptr, nullptr, &tv);
        }

        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (result == 0) {
            break;
        }
        if (s.pipeFds[0] >= 0 && FD_ISSET(s.pipeFds[0], &readSet)) {
            char buffer[64];
            while (read(s.pipeFds[0], buffer, sizeof(buffer)) > 0) {
            }
        }
        if (FD_ISSET(x11Fd, &readSet)) {
            break;
        }
        if (timeout >= 0.0) {
            break;
        }
    }

    processPendingEvents();
    linuxJoysticks().poll();
}

void X11Backend::postEmptyEvent() {
    State& s = g_state;
    if (s.pipeFds[1] >= 0) {
        const char byte = 1;
        (void)!write(s.pipeFds[1], &byte, 1);
    }
    XFlush(s.display);
}

void processPendingEvents() {
    State& s = g_state;
    if (!s.display) {
        return;
    }
    while (XPending(s.display) > 0) {
        XEvent event;
        XNextEvent(s.display, &event);

        if (event.type == GenericEvent && s.hasXi2 && event.xcookie.extension == s.xiOpcode) {
            XGetEventData(s.display, &event.xcookie);
            if (event.xcookie.evtype == XI_RawMotion && g_backendImpl) {
                if (auto* window = g_backendImpl->focusedWindow()) {
                    window->handleRawMotion(*static_cast<XIRawEvent*>(event.xcookie.data));
                }
            }
            XFreeEventData(s.display, &event.xcookie);
            continue;
        }

        if (s.hasXrandr &&
            (event.type == s.rrEventBase + RRScreenChangeNotify ||
             event.type == s.rrEventBase + RRNotify)) {
            XRRUpdateConfiguration(&event);
            rebuildMonitors(true);
            continue;
        }

        if (event.type == SelectionRequest) {
            if (g_backendImpl) {
                if (auto* window = g_backendImpl->findWindow(event.xselectionrequest.owner)) {
                    window->handleSelectionRequest(event.xselectionrequest);
                }
            }
            continue;
        }
        if (event.type == SelectionClear) {
            if (g_backendImpl) {
                if (auto* window = g_backendImpl->findWindow(event.xselectionclear.window)) {
                    window->handleSelectionClear(event.xselectionclear);
                }
            }
            continue;
        }

        if (g_backendImpl) {
            if (auto* window = g_backendImpl->findWindow(event.xany.window)) {
                window->handleEvent(event);
            }
        }
    }
}

void registerWindow(X11WindowBackend* window, ::Window handle) {
    if (g_backendImpl) {
        g_backendImpl->registerWindow(handle, window);
    }
}

void unregisterWindow(::Window handle) {
    if (g_backendImpl) {
        g_backendImpl->unregisterWindow(handle);
    }
}

X11WindowBackend* findWindow(::Window handle) {
    return g_backendImpl ? g_backendImpl->findWindow(handle) : nullptr;
}

X11WindowBackend* focusedWindow() {
    return g_backendImpl ? g_backendImpl->focusedWindow() : nullptr;
}

std::unique_ptr<Backend> createX11Backend(Error& error) {
    auto backend = std::make_unique<X11Backend>();
    g_backendImpl = backend.get();
    (void)error;
    return backend;
}

} // namespace sgl::detail::x11

namespace sgl::detail {

std::unique_ptr<Backend> createX11Backend(Error& error) {
    return x11::createX11Backend(error);
}

} // namespace sgl::detail
