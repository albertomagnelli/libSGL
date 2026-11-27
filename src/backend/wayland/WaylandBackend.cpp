#include "WaylandInternal.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <memory>
#include <poll.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "backend/BackendFactory.hpp"
#include "common/LinuxJoystick.hpp"
#include "common/MonitorAccess.hpp"

namespace sgl::detail::wayland {
namespace {

WaylandState g_state;

extern const wl_pointer_listener pointerListener;
extern const wl_keyboard_listener keyboardListener;

Monitor makeMonitor(WaylandOutput* output) {
    return MonitorAccess::make(output, Platform::Wayland);
}

void outputGeometry(void*, wl_output* output, int32_t x, int32_t y, int32_t physicalWidth,
                    int32_t physicalHeight, int32_t, const char*, const char*, int32_t transform) {
    if (auto* entry = findOutput(output)) {
        entry->position = ivec2(x, y);
        entry->physicalSize = ivec2(physicalWidth, physicalHeight);
        entry->transform = transform;
    }
}

void outputMode(void*, wl_output* output, uint32_t flags, int32_t width, int32_t height,
                int32_t refresh) {
    if (auto* entry = findOutput(output)) {
        if ((flags & WL_OUTPUT_MODE_CURRENT) != 0 || !entry->hasMode) {
            entry->logicalSize = ivec2(width, height);
            entry->refreshRate = static_cast<i32>(refresh / 1000);
            entry->hasMode = true;
        }
    }
}

void outputDone(void*, wl_output*) {}

void outputScale(void*, wl_output* output, int32_t factor) {
    if (auto* entry = findOutput(output)) {
        entry->scale = std::max(factor, 1);
    }
}

void outputName(void*, wl_output* output, const char* name) {
    if (auto* entry = findOutput(output)) {
        entry->name = name ? name : "";
    }
}

void outputDescription(void*, wl_output* output, const char* description) {
    if (auto* entry = findOutput(output)) {
        entry->description = description ? description : "";
    }
}

const wl_output_listener outputListener = {
    outputGeometry, outputMode, outputDone, outputScale, outputName, outputDescription,
};

void xdgOutputLogicalPosition(void*, zxdg_output_v1* output, int32_t x, int32_t y) {
    for (auto& entry : g_state.outputs) {
        if (entry->xdgOutput == output) {
            entry->position = ivec2(x, y);
            break;
        }
    }
}

void xdgOutputLogicalSize(void*, zxdg_output_v1* output, int32_t width, int32_t height) {
    for (auto& entry : g_state.outputs) {
        if (entry->xdgOutput == output) {
            entry->logicalSize = ivec2(width, height);
            break;
        }
    }
}

void xdgOutputDone(void*, zxdg_output_v1*) {}

void xdgOutputName(void*, zxdg_output_v1* output, const char* name) {
    for (auto& entry : g_state.outputs) {
        if (entry->xdgOutput == output) {
            entry->name = name ? name : "";
            break;
        }
    }
}

void xdgOutputDescription(void*, zxdg_output_v1* output, const char* description) {
    for (auto& entry : g_state.outputs) {
        if (entry->xdgOutput == output) {
            entry->description = description ? description : "";
            break;
        }
    }
}

const zxdg_output_v1_listener xdgOutputListener = {
    xdgOutputLogicalPosition, xdgOutputLogicalSize, xdgOutputDone, xdgOutputName,
    xdgOutputDescription,
};

void seatCapabilities(void*, wl_seat* seat, uint32_t capabilities) {
    WaylandState& s = g_state;
    if ((capabilities & WL_SEAT_CAPABILITY_POINTER) && !s.pointer) {
        s.pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(s.pointer, &pointerListener, nullptr);
    }
    if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) && !s.keyboard) {
        s.keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(s.keyboard, &keyboardListener, nullptr);
    }
    if ((capabilities & WL_SEAT_CAPABILITY_TOUCH) && !s.touch) {
        s.touch = wl_seat_get_touch(seat);
    }
}

void seatName(void*, wl_seat*, const char*) {}

const wl_seat_listener seatListener = {seatCapabilities, seatName};

void wmBasePing(void*, xdg_wm_base* base, uint32_t serial) {
    xdg_wm_base_pong(base, serial);
}

const xdg_wm_base_listener wmBaseListener = {wmBasePing};

void registryGlobal(void*, wl_registry* registry, uint32_t name, const char* interface,
                    uint32_t version) {
    WaylandState& s = g_state;

    if (std::strcmp(interface, wl_compositor_interface.name) == 0) {
        s.compositor = static_cast<wl_compositor*>(wl_registry_bind(
            registry, name, &wl_compositor_interface, std::min<uint32_t>(version, 4)));
    } else if (std::strcmp(interface, wl_shm_interface.name) == 0) {
        s.shm = static_cast<wl_shm*>(
            wl_registry_bind(registry, name, &wl_shm_interface, std::min<uint32_t>(version, 1)));
    } else if (std::strcmp(interface, wl_seat_interface.name) == 0) {
        s.seat = static_cast<wl_seat*>(
            wl_registry_bind(registry, name, &wl_seat_interface, std::min<uint32_t>(version, 7)));
        wl_seat_add_listener(s.seat, &seatListener, nullptr);
    } else if (std::strcmp(interface, xdg_wm_base_interface.name) == 0) {
        s.wmBase = static_cast<xdg_wm_base*>(wl_registry_bind(
            registry, name, &xdg_wm_base_interface, std::min<uint32_t>(version, 6)));
        xdg_wm_base_add_listener(s.wmBase, &wmBaseListener, nullptr);
    } else if (std::strcmp(interface, zxdg_output_manager_v1_interface.name) == 0) {
        s.outputManager = static_cast<zxdg_output_manager_v1*>(wl_registry_bind(
            registry, name, &zxdg_output_manager_v1_interface, std::min<uint32_t>(version, 3)));
    } else if (std::strcmp(interface, zxdg_decoration_manager_v1_interface.name) == 0) {
        s.decorationManager = static_cast<zxdg_decoration_manager_v1*>(wl_registry_bind(
            registry, name, &zxdg_decoration_manager_v1_interface, 1));
    } else if (std::strcmp(interface, wp_viewporter_interface.name) == 0) {
        s.viewporter = static_cast<wp_viewporter*>(
            wl_registry_bind(registry, name, &wp_viewporter_interface, 1));
    } else if (std::strcmp(interface, wl_data_device_manager_interface.name) == 0) {
        s.dataDeviceManager = static_cast<wl_data_device_manager*>(wl_registry_bind(
            registry, name, &wl_data_device_manager_interface, std::min<uint32_t>(version, 3)));
    } else if (std::strcmp(interface, wl_output_interface.name) == 0) {
        auto entry = std::make_unique<WaylandOutput>();
        entry->output = static_cast<wl_output*>(
            wl_registry_bind(registry, name, &wl_output_interface, std::min<uint32_t>(version, 4)));
        wl_output_add_listener(entry->output, &outputListener, nullptr);
        if (s.outputManager) {
            entry->xdgOutput = zxdg_output_manager_v1_get_xdg_output(s.outputManager, entry->output);
            zxdg_output_v1_add_listener(entry->xdgOutput, &xdgOutputListener, nullptr);
        }
        s.outputs.push_back(std::move(entry));
        rebuildMonitors(true);
    }
}

void registryGlobalRemove(void*, wl_registry*, uint32_t name) {
    WaylandState& s = g_state;
    for (auto it = s.outputs.begin(); it != s.outputs.end(); ++it) {
        if ((*it)->output &&
            wl_proxy_get_id(reinterpret_cast<wl_proxy*>((*it)->output)) == name) {
            WaylandOutput* removed = it->get();
            if (s.monitorCallback) {
                s.monitorCallback(makeMonitor(removed), false);
            }
            if (removed->xdgOutput) {
                zxdg_output_v1_destroy(removed->xdgOutput);
            }
            if (removed->output) {
                wl_output_destroy(removed->output);
            }
            s.outputs.erase(it);
            return;
        }
    }
}

const wl_registry_listener registryListener = {registryGlobal, registryGlobalRemove};

void dataOfferOffer(void*, wl_data_offer*, const char*) {}
void dataOfferSourceActions(void*, wl_data_offer*, uint32_t) {}
void dataOfferAction(void*, wl_data_offer*, uint32_t) {}

const wl_data_offer_listener dataOfferListener = {
    dataOfferOffer, dataOfferSourceActions, dataOfferAction,
};

void dataSourceTarget(void*, wl_data_source*, const char*) {}

void dataSourceSend(void*, wl_data_source*, const char* mimeType, int32_t fd) {
    WaylandState& s = g_state;
    (void)mimeType;
    const std::string& text = s.clipboardText;
    std::size_t offset = 0;
    while (offset < text.size()) {
        const ssize_t written = ::write(fd, text.data() + offset, text.size() - offset);
        if (written <= 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        offset += static_cast<std::size_t>(written);
    }
    ::close(fd);
}

void dataSourceCancelled(void*, wl_data_source* source) {
    if (g_state.clipboardSource == source) {
        g_state.clipboardSource = nullptr;
    }
}

void dataSourceDndDropPerformed(void*, wl_data_source*) {}
void dataSourceDndFinished(void*, wl_data_source*) {}
void dataSourceAction(void*, wl_data_source*, uint32_t) {}

const wl_data_source_listener dataSourceListener = {
    dataSourceTarget, dataSourceSend, dataSourceCancelled, dataSourceDndDropPerformed,
    dataSourceDndFinished, dataSourceAction,
};

void dataDeviceDataOffer(void*, wl_data_device*, wl_data_offer* offer) {
    if (g_state.clipboardOffer && g_state.clipboardOffer != offer) {
        wl_data_offer_destroy(g_state.clipboardOffer);
    }
    g_state.clipboardOffer = offer;
    if (offer) {
        wl_data_offer_add_listener(offer, &dataOfferListener, nullptr);
    }
}

void dataDeviceEnter(void*, wl_data_device*, uint32_t, wl_surface*, wl_fixed_t, wl_fixed_t,
                     wl_data_offer* offer) {
    if (g_state.dropOffer) {
        wl_data_offer_destroy(g_state.dropOffer);
    }
    g_state.dropOffer = offer;
    if (offer) {
        wl_data_offer_add_listener(offer, &dataOfferListener, nullptr);
        wl_data_offer_accept(offer, 0, "text/uri-list");
    }
}

void dataDeviceLeave(void*, wl_data_device*) {
    if (g_state.dropOffer) {
        wl_data_offer_destroy(g_state.dropOffer);
        g_state.dropOffer = nullptr;
    }
}

void dataDeviceMotion(void*, wl_data_device*, uint32_t, wl_fixed_t, wl_fixed_t) {}

void dataDeviceDrop(void*, wl_data_device*) {
    WaylandState& s = g_state;
    if (!s.dropOffer || !s.pointerFocus) {
        return;
    }

    int fds[2];
    if (pipe(fds) != 0) {
        return;
    }
    wl_data_offer_receive(s.dropOffer, "text/uri-list", fds[1]);
    ::close(fds[1]);
    wl_display_flush(s.display);

    std::string data;
    char buffer[4096];
    while (true) {
        pollfd pfd{fds[0], POLLIN, 0};
        const int ready = poll(&pfd, 1, 1000);
        if (ready <= 0) {
            break;
        }
        const ssize_t count = ::read(fds[0], buffer, sizeof(buffer));
        if (count <= 0) {
            break;
        }
        data.append(buffer, static_cast<std::size_t>(count));
    }
    ::close(fds[0]);

    std::vector<std::string> paths;
    std::size_t start = 0;
    while (start < data.size()) {
        auto end = data.find('\n', start);
        if (end == std::string::npos) {
            end = data.size();
        }
        std::string line = data.substr(start, end - start);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        start = end + 1;
        if (line.rfind("file://", 0) == 0) {
            std::string path = line.substr(7);
            if (path.rfind("localhost/", 0) == 0) {
                path = path.substr(9);
            }
            if (!path.empty()) {
                paths.push_back(path);
            }
        }
    }

    if (!paths.empty() && s.pointerFocus) {
        s.pointerFocus->handleDrop(paths);
    }

    wl_data_offer_destroy(s.dropOffer);
    s.dropOffer = nullptr;
}

void dataDeviceSelection(void*, wl_data_device*, wl_data_offer* offer) {
    if (g_state.clipboardOffer && g_state.clipboardOffer != offer) {
        wl_data_offer_destroy(g_state.clipboardOffer);
    }
    g_state.clipboardOffer = offer;
    if (offer) {
        wl_data_offer_add_listener(offer, &dataOfferListener, nullptr);
    }
}

const wl_data_device_listener dataDeviceListener = {
    dataDeviceDataOffer, dataDeviceEnter, dataDeviceLeave, dataDeviceMotion, dataDeviceDrop,
    dataDeviceSelection,
};

void pointerEnter(void*, wl_pointer*, uint32_t serial, wl_surface* surface, wl_fixed_t x,
                  wl_fixed_t y) {
    WaylandState& s = g_state;
    s.pointerEnterSerial = serial;
    s.lastSerial = serial;
    s.pointerFocus = windowForSurface(surface);
    if (s.pointerFocus) {
        s.pointerFocus->handlePointerEnter(serial, surface,
                                           vec2(static_cast<f32>(wl_fixed_to_double(x)),
                                                static_cast<f32>(wl_fixed_to_double(y))));
        s.pointerFocus->updateCursor();
    }
}

void pointerLeave(void*, wl_pointer*, uint32_t serial, wl_surface*) {
    WaylandState& s = g_state;
    s.lastSerial = serial;
    if (s.pointerFocus) {
        s.pointerFocus->handlePointerLeave();
        s.pointerFocus = nullptr;
    }
}

void pointerMotion(void*, wl_pointer*, uint32_t, wl_fixed_t x, wl_fixed_t y) {
    if (g_state.pointerFocus) {
        g_state.pointerFocus->handlePointerMotion(
            vec2(static_cast<f32>(wl_fixed_to_double(x)),
                 static_cast<f32>(wl_fixed_to_double(y))));
    }
}

void pointerButton(void*, wl_pointer*, uint32_t serial, uint32_t, uint32_t button,
                   uint32_t state) {
    g_state.lastSerial = serial;
    if (g_state.pointerFocus) {
        g_state.pointerFocus->handlePointerButton(serial, button,
                                                  state == WL_POINTER_BUTTON_STATE_PRESSED);
    }
}

void pointerAxis(void*, wl_pointer*, uint32_t, uint32_t axis, wl_fixed_t value) {
    if (!g_state.pointerFocus) {
        return;
    }
    const auto amount = static_cast<f32>(wl_fixed_to_double(value));
    if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
        g_state.pointerFocus->handlePointerAxis(vec2(0.0f, -amount));
    } else if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL) {
        g_state.pointerFocus->handlePointerAxis(vec2(amount, 0.0f));
    }
}

void pointerFrame(void*, wl_pointer*) {}
void pointerAxisSource(void*, wl_pointer*, uint32_t) {}
void pointerAxisStop(void*, wl_pointer*, uint32_t, uint32_t) {}
void pointerAxisDiscrete(void*, wl_pointer*, uint32_t, int32_t) {}
void pointerAxisValue120(void*, wl_pointer*, uint32_t, int32_t) {}
void pointerAxisRelativeDirection(void*, wl_pointer*, uint32_t, uint32_t) {}

extern const wl_pointer_listener pointerListener = {
    pointerEnter, pointerLeave, pointerMotion, pointerButton, pointerAxis, pointerFrame,
    pointerAxisSource, pointerAxisStop, pointerAxisDiscrete, pointerAxisValue120,
    pointerAxisRelativeDirection,
};

void keyboardKeymap(void*, wl_keyboard*, uint32_t format, int32_t fd, uint32_t size) {
    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
        ::close(fd);
        return;
    }
    if (!g_state.keyboardFocus) {
        ::close(fd);
        return;
    }
    g_state.keyboardFocus->handleKeymap(format, fd, size);
    ::close(fd);
}

void keyboardEnter(void*, wl_keyboard*, uint32_t serial, wl_surface* surface, wl_array* keys) {
    WaylandState& s = g_state;
    s.lastSerial = serial;
    s.keyboardFocus = windowForSurface(surface);
    if (s.keyboardFocus) {
        s.keyboardFocus->handleKeyboardEnter(serial, keys);
    }
}

void keyboardLeave(void*, wl_keyboard*, uint32_t serial, wl_surface*) {
    WaylandState& s = g_state;
    s.lastSerial = serial;
    if (s.keyboardFocus) {
        s.keyboardFocus->handleKeyboardLeave();
        s.keyboardFocus = nullptr;
    }
}

void keyboardKey(void*, wl_keyboard*, uint32_t serial, uint32_t time, uint32_t key,
                 uint32_t state) {
    g_state.lastSerial = serial;
    if (g_state.keyboardFocus) {
        g_state.keyboardFocus->handleKey(serial, time, key, state);
    }
}

void keyboardModifiers(void*, wl_keyboard*, uint32_t serial, uint32_t depressed,
                       uint32_t latched, uint32_t locked, uint32_t group) {
    g_state.lastSerial = serial;
    if (g_state.keyboardFocus) {
        g_state.keyboardFocus->handleModifiers(serial, depressed, latched, locked, group);
    }
}

void keyboardRepeatInfo(void*, wl_keyboard*, int32_t, int32_t) {}

extern const wl_keyboard_listener keyboardListener = {
    keyboardKeymap, keyboardEnter, keyboardLeave, keyboardKey, keyboardModifiers,
    keyboardRepeatInfo,
};

void decorationConfigure(void*, zxdg_toplevel_decoration_v1*, uint32_t) {}

const zxdg_toplevel_decoration_v1_listener decorationListener = {decorationConfigure};

} // namespace

WaylandState& state() {
    return g_state;
}

WaylandOutput* findOutput(wl_output* output) {
    for (auto& candidate : g_state.outputs) {
        if (candidate->output == output) {
            return candidate.get();
        }
    }
    return nullptr;
}

WaylandWindowBackend* windowForSurface(wl_surface* surface) {
    const auto it = g_state.windows.find(surface);
    return it == g_state.windows.end() ? nullptr : it->second;
}

const wl_data_source_listener* dataSourceListenerPtr() {
    return &dataSourceListener;
}

void rebuildMonitors(bool notify) {
    WaylandState& s = g_state;
    if (notify && s.monitorCallback) {
        for (const auto& output : s.outputs) {
            s.monitorCallback(makeMonitor(output.get()), true);
        }
    }
}

void dispatchEvents(bool block, f64 timeout) {
    WaylandState& s = g_state;
    if (!s.display) {
        return;
    }

    wl_display_dispatch_pending(s.display);
    wl_display_flush(s.display);

    if (wl_display_prepare_read(s.display) != 0) {
        wl_display_dispatch_pending(s.display);
        return;
    }
    wl_display_flush(s.display);

    pollfd fds[2] = {{wl_display_get_fd(s.display), POLLIN, 0},
                     {s.pipeFds[0], POLLIN, 0}};
    const int count = s.pipeFds[0] >= 0 ? 2 : 1;
    int milliseconds = 0;
    if (block) {
        milliseconds = timeout < 0.0 ? -1 : static_cast<int>(timeout * 1000.0);
    }

    const int ready = poll(fds, static_cast<nfds_t>(count), milliseconds);
    if (ready > 0 && (fds[0].revents & POLLIN) != 0) {
        if (wl_display_read_events(s.display) < 0) {
            return;
        }
    } else {
        wl_display_cancel_read(s.display);
    }

    if (ready > 0 && count == 2 && (fds[1].revents & POLLIN) != 0) {
        char buffer[64];
        while (::read(s.pipeFds[0], buffer, sizeof(buffer)) > 0) {
        }
    }

    wl_display_dispatch_pending(s.display);
}

namespace {

class WaylandBackendImpl final : public Backend {
public:
    bool init(const InitOptions&, Error& error) override;
    void terminate() override;
    [[nodiscard]] Platform getPlatform() const override { return Platform::Wayland; }

    std::unique_ptr<WindowBackend> createWindow(const WindowHints& hints, Error& error) override;
    std::unique_ptr<CursorBackend> createCursor(const Image& image, ivec2 hotspot,
                                                Error& error) override;
    std::unique_ptr<CursorBackend> createStandardCursor(StandardCursor shape, Error& error) override;

    [[nodiscard]] std::vector<Monitor> getMonitors() override {
        std::vector<Monitor> monitors;
        monitors.reserve(state().outputs.size());
        for (const auto& output : state().outputs) {
            monitors.push_back(makeMonitor(output.get()));
        }
        return monitors;
    }

    [[nodiscard]] Monitor getPrimaryMonitor() override {
        if (!state().outputs.empty()) {
            return makeMonitor(state().outputs.front().get());
        }
        return {};
    }

    void pollEvents() override {
        dispatchEvents(false, 0.0);
        linuxJoysticks().poll();
    }
    void waitEvents(f64 timeout) override {
        dispatchEvents(true, timeout);
        linuxJoysticks().poll();
    }
    void postEmptyEvent() override {
        if (state().pipeFds[1] >= 0) {
            const char byte = 1;
            (void)!::write(state().pipeFds[1], &byte, 1);
        }
        wl_display_flush(state().display);
    }

    void setMonitorCallback(MonitorCallback callback) override {
        state().monitorCallback = std::move(callback);
    }

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
    bool updateGamepadMappings(std::string_view) override { return true; }
    void setJoystickCallback(std::function<void(i32, bool)> callback) override {
        linuxJoysticks().setCallback(std::move(callback));
    }

    [[nodiscard]] bool isMonitorConnected(const Monitor& monitor) override {
        auto* handle = MonitorAccess::native(monitor);
        for (const auto& output : state().outputs) {
            if (output.get() == handle) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] ivec2 getMonitorPosition(const Monitor& monitor) override {
        auto* output = static_cast<WaylandOutput*>(MonitorAccess::native(monitor));
        return output ? output->position : ivec2(0, 0);
    }

    [[nodiscard]] Monitor::Workarea getMonitorWorkarea(const Monitor& monitor) override {
        auto* output = static_cast<WaylandOutput*>(MonitorAccess::native(monitor));
        Monitor::Workarea area;
        if (output) {
            area.position = output->position;
            area.size = output->logicalSize;
        }
        return area;
    }

    [[nodiscard]] ivec2 getMonitorPhysicalSize(const Monitor& monitor) override {
        auto* output = static_cast<WaylandOutput*>(MonitorAccess::native(monitor));
        return output ? output->physicalSize : ivec2(0, 0);
    }

    [[nodiscard]] vec2 getMonitorContentScale(const Monitor& monitor) override {
        auto* output = static_cast<WaylandOutput*>(MonitorAccess::native(monitor));
        const float scale = output ? static_cast<float>(output->scale) : 1.0f;
        return {scale, scale};
    }

    [[nodiscard]] std::string getMonitorName(const Monitor& monitor) override {
        auto* output = static_cast<WaylandOutput*>(MonitorAccess::native(monitor));
        return output ? output->name : std::string{};
    }

    [[nodiscard]] std::vector<VideoMode> getVideoModes(const Monitor& monitor) override {
        auto* output = static_cast<WaylandOutput*>(MonitorAccess::native(monitor));
        std::vector<VideoMode> modes;
        if (output && output->hasMode) {
            VideoMode mode;
            mode.size = output->logicalSize;
            mode.refreshRate = output->refreshRate;
            modes.push_back(mode);
        }
        return modes;
    }

    [[nodiscard]] VideoMode getVideoMode(const Monitor& monitor) override {
        auto* output = static_cast<WaylandOutput*>(MonitorAccess::native(monitor));
        VideoMode mode;
        if (output && output->hasMode) {
            mode.size = output->logicalSize;
            mode.refreshRate = output->refreshRate;
        }
        return mode;
    }

    [[nodiscard]] NativeHandle getMonitorNativeHandle(const Monitor& monitor) override {
        NativeHandle handle;
        handle.platform = NativePlatform::Wayland;
        handle.display = state().display;
        handle.window = MonitorAccess::native(monitor);
        return handle;
    }
};


} // namespace

bool WaylandBackendImpl::init(const InitOptions&, Error& error) {
    WaylandState& s = g_state;
    if (s.initialized) {
        return true;
    }

    s.display = wl_display_connect(nullptr);
    if (!s.display) {
        error = makeError(ErrorCode::PlatformUnavailable,
                          "Wayland-Display konnte nicht verbunden werden.");
        return false;
    }

    s.registry = wl_display_get_registry(s.display);
    wl_registry_add_listener(s.registry, &registryListener, nullptr);

    if (wl_display_roundtrip(s.display) < 0) {
        error = makeError(ErrorCode::PlatformError, "Wayland-Registry konnte nicht gelesen werden.");
        return false;
    }
    if (wl_display_roundtrip(s.display) < 0) {
        error = makeError(ErrorCode::PlatformError, "Wayland-Objekte konnten nicht gelesen werden.");
        return false;
    }
    if (!s.compositor || !s.shm || !s.wmBase) {
        error = makeError(ErrorCode::PlatformUnavailable,
                          "Der Wayland-Compositor bietet die benoetigten Protokolle nicht an.");
        return false;
    }

    if (s.dataDeviceManager && s.seat) {
        s.dataDevice = wl_data_device_manager_get_data_device(s.dataDeviceManager, s.seat);
        wl_data_device_add_listener(s.dataDevice, &dataDeviceListener, nullptr);
    }

    s.cursorTheme = wl_cursor_theme_load(nullptr, 24, s.shm);

    if (pipe(s.pipeFds) == 0) {
        for (int fd : s.pipeFds) {
            fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
            fcntl(fd, F_SETFD, FD_CLOEXEC);
        }
    }

    s.initialized = true;
    return true;
}

void WaylandBackendImpl::terminate() {
    WaylandState& s = g_state;
    if (!s.display) {
        return;
    }

    std::vector<WaylandWindowBackend*> windows;
    for (auto& [surface, window] : s.windows) {
        (void)surface;
        windows.push_back(window);
    }
    for (auto* window : windows) {
        window->destroy();
    }
    s.windows.clear();

    if (s.clipboardOffer) {
        wl_data_offer_destroy(s.clipboardOffer);
        s.clipboardOffer = nullptr;
    }
    if (s.dropOffer) {
        wl_data_offer_destroy(s.dropOffer);
        s.dropOffer = nullptr;
    }
    if (s.clipboardSource) {
        wl_data_source_destroy(s.clipboardSource);
        s.clipboardSource = nullptr;
    }
    if (s.cursorTheme) {
        wl_cursor_theme_destroy(s.cursorTheme);
        s.cursorTheme = nullptr;
    }
    for (auto& output : s.outputs) {
        if (output->xdgOutput) {
            zxdg_output_v1_destroy(output->xdgOutput);
        }
        if (output->output) {
            wl_output_destroy(output->output);
        }
    }
    s.outputs.clear();

    if (s.dataDevice) {
        wl_data_device_destroy(s.dataDevice);
        s.dataDevice = nullptr;
    }
    if (s.pointer) {
        wl_pointer_destroy(s.pointer);
        s.pointer = nullptr;
    }
    if (s.keyboard) {
        wl_keyboard_destroy(s.keyboard);
        s.keyboard = nullptr;
    }
    if (s.touch) {
        wl_touch_destroy(s.touch);
        s.touch = nullptr;
    }
    if (s.seat) {
        wl_seat_destroy(s.seat);
        s.seat = nullptr;
    }
    if (s.dataDeviceManager) {
        wl_data_device_manager_destroy(s.dataDeviceManager);
        s.dataDeviceManager = nullptr;
    }
    if (s.viewporter) {
        wp_viewporter_destroy(s.viewporter);
        s.viewporter = nullptr;
    }
    if (s.decorationManager) {
        zxdg_decoration_manager_v1_destroy(s.decorationManager);
        s.decorationManager = nullptr;
    }
    if (s.outputManager) {
        zxdg_output_manager_v1_destroy(s.outputManager);
        s.outputManager = nullptr;
    }
    if (s.wmBase) {
        xdg_wm_base_destroy(s.wmBase);
        s.wmBase = nullptr;
    }
    if (s.shm) {
        wl_shm_destroy(s.shm);
        s.shm = nullptr;
    }
    if (s.compositor) {
        wl_compositor_destroy(s.compositor);
        s.compositor = nullptr;
    }
    if (s.registry) {
        wl_registry_destroy(s.registry);
        s.registry = nullptr;
    }
    for (int fd : s.pipeFds) {
        if (fd >= 0) {
            ::close(fd);
        }
    }
    s.pipeFds[0] = s.pipeFds[1] = -1;

    wl_display_disconnect(s.display);
    s.display = nullptr;
    s.initialized = false;
    s.monitorCallback = nullptr;
}

std::unique_ptr<WindowBackend> WaylandBackendImpl::createWindow(const WindowHints& hints,
                                                                Error& error) {
    auto window = std::make_unique<WaylandWindowBackend>(hints, error);
    if (error.isError()) {
        return nullptr;
    }
    return window;
}

namespace {

wl_buffer* createShmBuffer(ivec2 size, const u8* rgba, ivec2 hotspot, int* fdOut,
                           std::size_t* sizeOut) {
    (void)hotspot;
    const int stride = size.x * 4;
    const std::size_t bytes = static_cast<std::size_t>(stride) * static_cast<std::size_t>(size.y);

    int fd = memfd_create("sgl-cursor", MFD_CLOEXEC);
    if (fd < 0) {
        return nullptr;
    }
    if (ftruncate(fd, static_cast<off_t>(bytes)) != 0) {
        ::close(fd);
        return nullptr;
    }
    auto* data = static_cast<u8*>(mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
    if (data == MAP_FAILED) {
        ::close(fd);
        return nullptr;
    }

    for (std::size_t i = 0; i < static_cast<std::size_t>(size.x) * static_cast<std::size_t>(size.y);
         ++i) {
        data[i * 4 + 0] = rgba[i * 4 + 2];
        data[i * 4 + 1] = rgba[i * 4 + 1];
        data[i * 4 + 2] = rgba[i * 4 + 0];
        data[i * 4 + 3] = rgba[i * 4 + 3];
    }

    wl_shm_pool* pool = wl_shm_create_pool(state().shm, fd, static_cast<int>(bytes));
    wl_buffer* buffer = wl_shm_pool_create_buffer(pool, 0, size.x, size.y, stride,
                                                  WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    munmap(data, bytes);

    *fdOut = fd;
    *sizeOut = bytes;
    return buffer;
}

} // namespace

std::unique_ptr<CursorBackend> WaylandBackendImpl::createCursor(const Image& image, ivec2 hotspot,
                                                                Error& error) {
    WaylandState& s = state();
    if (image.size.x <= 0 || image.size.y <= 0) {
        error = makeError(ErrorCode::InvalidValue, "Cursor-Bild hat eine ungueltige Groesse.");
        return nullptr;
    }
    const auto pixels = static_cast<std::size_t>(image.size.x) *
                        static_cast<std::size_t>(image.size.y);
    if (image.rgba8.size() < pixels * 4) {
        error = makeError(ErrorCode::InvalidValue, "Cursor-Bild enthaelt zu wenige Pixel.");
        return nullptr;
    }
    if (!s.compositor || !s.shm) {
        error = makeError(ErrorCode::FeatureUnavailable,
                          "Wayland-Compositor unterstuetzt keine Cursor.");
        return nullptr;
    }

    int fd = -1;
    std::size_t bytes = 0;
    wl_buffer* buffer = createShmBuffer(image.size, image.rgba8.data(), hotspot, &fd, &bytes);
    if (!buffer) {
        error = makeError(ErrorCode::OutOfMemory, "Cursor-Puffer konnte nicht erstellt werden.");
        return nullptr;
    }
    if (fd >= 0) {
        ::close(fd);
    }
    (void)bytes;

    wl_surface* surface = wl_compositor_create_surface(s.compositor);
    auto cursor = std::make_unique<WaylandCursorBackend>(surface, buffer, image.size, hotspot);
    return cursor;
}

std::unique_ptr<CursorBackend> WaylandBackendImpl::createStandardCursor(StandardCursor shape,
                                                                        Error& error) {
    WaylandState& s = state();
    if (!s.cursorTheme) {
        error = makeError(ErrorCode::FeatureUnavailable,
                          "Kein Wayland-Cursor-Theme verfuegbar.");
        return nullptr;
    }
    const char* name = "left_ptr";
    switch (shape) {
    case StandardCursor::Arrow: name = "left_ptr"; break;
    case StandardCursor::IBeam: name = "xterm"; break;
    case StandardCursor::Crosshair: name = "crosshair"; break;
    case StandardCursor::Hand: name = "hand2"; break;
    case StandardCursor::HResize: name = "sb_h_double_arrow"; break;
    case StandardCursor::VResize: name = "sb_v_double_arrow"; break;
    }

    wl_cursor* cursor = wl_cursor_theme_get_cursor(s.cursorTheme, name);
    if (!cursor || cursor->image_count == 0) {
        error = makeError(ErrorCode::PlatformError, "Standard-Cursor konnte nicht geladen werden.");
        return nullptr;
    }
    wl_cursor_image* image = cursor->images[0];
    wl_surface* surface = wl_compositor_create_surface(s.compositor);
    return std::make_unique<WaylandCursorBackend>(
        surface, wl_cursor_image_get_buffer(image), ivec2(static_cast<i32>(image->width),
                                                          static_cast<i32>(image->height)),
        ivec2(static_cast<i32>(image->hotspot_x), static_cast<i32>(image->hotspot_y)), false);
}

} // namespace sgl::detail::wayland

namespace sgl::detail {

std::unique_ptr<Backend> createWaylandBackend(Error& error) {
    auto backend = std::make_unique<wayland::WaylandBackendImpl>();

    (void)error;
    return backend;
}

} // namespace sgl::detail
