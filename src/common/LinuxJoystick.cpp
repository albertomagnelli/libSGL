#include "LinuxJoystick.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

namespace sgl::detail {
namespace {

constexpr std::size_t bitCount(int maxValue) {
    return static_cast<std::size_t>(maxValue / 8 + 1);
}

bool testBit(const unsigned long* bits, int code) {
    const auto index = static_cast<std::size_t>(code);
    return (bits[index / (sizeof(unsigned long) * 8)] &
            (1UL << (index % (sizeof(unsigned long) * 8)))) != 0;
}

struct AbsAxis {
    int code = 0;
    int minimum = 0;
    int maximum = 0;
    int value = 0;
};

bool hasHat(JoystickHat value, JoystickHat flag) {
    return (static_cast<int>(value) & static_cast<int>(flag)) != 0;
}

struct LinuxDevice {
    std::string path;
    int fd = -1;
    std::string name;
    std::string guid;
    u16 bus = 0;
    u16 vendor = 0;
    u16 product = 0;
    u16 version = 0;

    std::vector<f32> axes;
    std::vector<AbsAxis> axisInfo;
    std::vector<int> axisCodes;
    std::unordered_map<int, int> axisIndex;

    std::vector<u8> buttons;
    std::vector<int> buttonCodes;
    std::unordered_map<int, int> buttonIndex;

    std::vector<JoystickHat> hats;
    std::vector<std::pair<int, int>> hatCodes;

    void* userPointer = nullptr;

    bool gamepad = false;
    GamepadState gamepadState{};

    int buttonFor(std::initializer_list<int> codes) const {
        for (int code : codes) {
            const auto it = buttonIndex.find(code);
            if (it != buttonIndex.end()) {
                return it->second;
            }
        }
        return -1;
    }

    int axisFor(std::initializer_list<int> codes) const {
        for (int code : codes) {
            const auto it = axisIndex.find(code);
            if (it != axisIndex.end()) {
                return it->second;
            }
        }
        return -1;
    }

    void computeGamepad() {
        const int a = buttonFor({BTN_A, BTN_SOUTH, BTN_1});
        const int b = buttonFor({BTN_B, BTN_EAST, BTN_2});
        const int x = buttonFor({BTN_X, BTN_NORTH, BTN_4});
        const int y = buttonFor({BTN_Y, BTN_WEST, BTN_3});
        const int leftX = axisFor({ABS_X});
        const int leftY = axisFor({ABS_Y});
        const int rightX = axisFor({ABS_RX});
        const int rightY = axisFor({ABS_RY});
        gamepad = a >= 0 && b >= 0 && x >= 0 && y >= 0 && leftX >= 0 && leftY >= 0 &&
                  rightX >= 0 && rightY >= 0;

        if (!gamepad) {
            return;
        }

        gamepadState = GamepadState{};
        const int lb = buttonFor({BTN_TL, BTN_LEFT});
        const int rb = buttonFor({BTN_TR, BTN_RIGHT});
        const int back = buttonFor({BTN_SELECT, BTN_BACK});
        const int start = buttonFor({BTN_START});
        const int guide = buttonFor({BTN_MODE, BTN_MODE});
        const int ls = buttonFor({BTN_THUMBL});
        const int rs = buttonFor({BTN_THUMBR});

        auto setButton = [this](GamepadButton button, int index) {
            if (index >= 0 && static_cast<std::size_t>(index) < buttons.size()) {
                gamepadState.buttons[static_cast<std::size_t>(button)] = buttons[static_cast<std::size_t>(index)];
            }
        };
        setButton(GamepadButton::A, a);
        setButton(GamepadButton::B, b);
        setButton(GamepadButton::X, x);
        setButton(GamepadButton::Y, y);
        setButton(GamepadButton::LeftBumper, lb);
        setButton(GamepadButton::RightBumper, rb);
        setButton(GamepadButton::Back, back);
        setButton(GamepadButton::Start, start);
        setButton(GamepadButton::Guide, guide);
        setButton(GamepadButton::LeftThumb, ls);
        setButton(GamepadButton::RightThumb, rs);

        auto setAxis = [this](GamepadAxis axis, int index) {
            if (index >= 0 && static_cast<std::size_t>(index) < axes.size()) {
                gamepadState.axes[static_cast<std::size_t>(axis)] =
                    axes[static_cast<std::size_t>(index)];
            }
        };
        setAxis(GamepadAxis::LeftX, leftX);
        setAxis(GamepadAxis::LeftY, leftY);
        setAxis(GamepadAxis::RightX, rightX);
        setAxis(GamepadAxis::RightY, rightY);
        setAxis(GamepadAxis::LeftTrigger, axisFor({ABS_Z, ABS_GAS, ABS_BRAKE}));
        setAxis(GamepadAxis::RightTrigger, axisFor({ABS_RZ, ABS_BRAKE, ABS_GAS}));

        if (!hats.empty()) {
            const auto hat = hats.front();
            gamepadState.buttons[static_cast<std::size_t>(GamepadButton::DPadUp)] =
                hasHat(hat, JoystickHat::Up);
            gamepadState.buttons[static_cast<std::size_t>(GamepadButton::DPadDown)] =
                hasHat(hat, JoystickHat::Down);
            gamepadState.buttons[static_cast<std::size_t>(GamepadButton::DPadLeft)] =
                hasHat(hat, JoystickHat::Left);
            gamepadState.buttons[static_cast<std::size_t>(GamepadButton::DPadRight)] =
                hasHat(hat, JoystickHat::Right);
        }
    }
};


int openDevice(const std::string& path, LinuxDevice& device) {
    const int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        return -1;
    }

    std::array<unsigned long, bitCount(EV_MAX)> eventBits{};
    if (ioctl(fd, EVIOCGBIT(0, sizeof(eventBits)), eventBits.data()) < 0) {
        close(fd);
        return -1;
    }
    if (!testBit(eventBits.data(), EV_ABS) && !testBit(eventBits.data(), EV_KEY)) {
        close(fd);
        return -1;
    }

    std::array<char, 256> name{};
    if (ioctl(fd, EVIOCGNAME(sizeof(name) - 1), name.data()) < 0) {
        close(fd);
        return -1;
    }
    device.name = name.data();

    input_id id{};
    if (ioctl(fd, EVIOCGID, &id) == 0) {
        device.bus = id.bustype;
        device.vendor = id.vendor;
        device.product = id.product;
        device.version = id.version;
    }
    char guid[40];
    std::snprintf(guid, sizeof(guid), "%04x%04x%04x%04x0000000000000000",
                  static_cast<unsigned>(device.bus), static_cast<unsigned>(device.vendor),
                  static_cast<unsigned>(device.product), static_cast<unsigned>(device.version));
    device.guid = guid;

    std::array<unsigned long, bitCount(ABS_MAX)> absBits{};
    if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absBits)), absBits.data()) < 0) {
        std::memset(absBits.data(), 0, sizeof(absBits));
    }

    const int axisOrder[] = {ABS_X,      ABS_Y,     ABS_Z,       ABS_RX,    ABS_RY,
                             ABS_RZ,     ABS_THROTTLE, ABS_RUDDER, ABS_WHEEL, ABS_GAS,
                             ABS_BRAKE,  ABS_DISTANCE, ABS_MISC};
    for (int code : axisOrder) {
        if (!testBit(absBits.data(), code)) {
            continue;
        }
        input_absinfo info{};
        if (ioctl(fd, EVIOCGABS(code), &info) < 0) {
            continue;
        }
        AbsAxis axis;
        axis.code = code;
        axis.minimum = info.minimum;
        axis.maximum = info.maximum;
        axis.value = info.value;
        device.axisIndex[code] = static_cast<int>(device.axes.size());
        device.axisCodes.push_back(code);
        device.axisInfo.push_back(axis);
        device.axes.push_back(info.minimum == info.maximum
                                  ? 0.0f
                                  : 2.0f * (static_cast<f32>(info.value) -
                                            static_cast<f32>(info.minimum)) /
                                            static_cast<f32>(info.maximum - info.minimum) -
                                        1.0f);
    }

    std::array<unsigned long, bitCount(KEY_MAX)> keyBits{};
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keyBits)), keyBits.data()) < 0) {
        std::memset(keyBits.data(), 0, sizeof(keyBits));
    }
    for (int code = BTN_MISC; code <= 0x2ff; ++code) {
        if (static_cast<std::size_t>(code) >= bitCount(KEY_MAX) * sizeof(unsigned long) * 8) {
            break;
        }
        if (!testBit(keyBits.data(), code)) {
            continue;
        }
        device.buttonIndex[code] = static_cast<int>(device.buttons.size());
        device.buttonCodes.push_back(code);
        device.buttons.push_back(0);
    }

    for (int hat = 0; hat < 4; ++hat) {
        const int x = ABS_HAT0X + hat * 2;
        const int y = ABS_HAT0Y + hat * 2;
        if (testBit(absBits.data(), x) && testBit(absBits.data(), y)) {
            device.hatCodes.emplace_back(x, y);
            device.hats.push_back(JoystickHat::Centered);
        }
    }

    device.path = path;
    device.fd = fd;
    device.computeGamepad();
    return fd;
}

void readDevice(LinuxDevice& device) {
    input_event events[32];
    while (true) {
        const ssize_t count = read(device.fd, events, sizeof(events));
        if (count <= 0) {
            break;
        }
        const std::size_t eventCount = static_cast<std::size_t>(count) / sizeof(input_event);
        for (std::size_t i = 0; i < eventCount; ++i) {
            const input_event& event = events[i];
            if (event.type == EV_ABS) {
                const auto axisIt = device.axisIndex.find(event.code);
                if (axisIt != device.axisIndex.end()) {
                    const int index = axisIt->second;
                    if (static_cast<std::size_t>(index) < device.axisInfo.size()) {
                        const AbsAxis& info = device.axisInfo[static_cast<std::size_t>(index)];
                        if (info.maximum != info.minimum) {
                            device.axes[static_cast<std::size_t>(index)] =
                                2.0f * (static_cast<f32>(event.value) -
                                        static_cast<f32>(info.minimum)) /
                                    static_cast<f32>(info.maximum - info.minimum) -
                                1.0f;
                        }
                    }
                }
                for (std::size_t hat = 0; hat < device.hatCodes.size(); ++hat) {
                    if (event.code == device.hatCodes[hat].first) {
                        int value = static_cast<int>(device.hats[hat]);
                        value &= ~(static_cast<int>(JoystickHat::Left) |
                                   static_cast<int>(JoystickHat::Right));
                        if (event.value < 0) value |= static_cast<int>(JoystickHat::Left);
                        if (event.value > 0) value |= static_cast<int>(JoystickHat::Right);
                        device.hats[hat] = static_cast<JoystickHat>(value);
                    } else if (event.code == device.hatCodes[hat].second) {
                        int value = static_cast<int>(device.hats[hat]);
                        value &= ~(static_cast<int>(JoystickHat::Up) |
                                   static_cast<int>(JoystickHat::Down));
                        if (event.value < 0) value |= static_cast<int>(JoystickHat::Up);
                        if (event.value > 0) value |= static_cast<int>(JoystickHat::Down);
                        device.hats[hat] = static_cast<JoystickHat>(value);
                    }
                }
            } else if (event.type == EV_KEY) {
                const auto buttonIt = device.buttonIndex.find(event.code);
                if (buttonIt != device.buttonIndex.end()) {
                    device.buttons[static_cast<std::size_t>(buttonIt->second)] =
                        event.value != 0 ? 1 : 0;
                }
            }
        }
    }
    device.computeGamepad();
}

} // namespace

struct LinuxJoysticks::Impl {
    std::vector<LinuxDevice> devices;
    std::function<void(i32, bool)> callback;
    int inotifyFd = -1;
    int watchDescriptor = -1;
    bool scanned = false;

    ~Impl() {
        for (auto& device : devices) {
            if (device.fd >= 0) {
                close(device.fd);
            }
        }
        if (inotifyFd >= 0) {
            close(inotifyFd);
        }
    }

    void ensureInotify() {
        if (inotifyFd >= 0) {
            return;
        }
        inotifyFd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
        if (inotifyFd >= 0) {
            watchDescriptor = inotify_add_watch(inotifyFd, "/dev/input", IN_CREATE | IN_DELETE);
        }
    }

    bool pollInotify() {
        if (inotifyFd < 0) {
            return false;
        }
        char buffer[4096];
        bool changed = false;
        while (read(inotifyFd, buffer, sizeof(buffer)) > 0) {
            changed = true;
        }
        return changed;
    }

    void scan() {
        std::vector<LinuxDevice> found;
        std::vector<std::string> previousNames;
        previousNames.reserve(devices.size());

        // Keep currently open devices that still exist.
        for (auto& device : devices) {
            previousNames.push_back(device.name);
            if (access(device.path.c_str(), F_OK) != 0) {
                close(device.fd);
                continue;
            }
            readDevice(device);
            found.push_back(std::move(device));
        }
        devices.clear();

        for (int index = 0; index < 32; ++index) {
            char path[64];
            std::snprintf(path, sizeof(path), "/dev/input/event%d", index);
            if (access(path, F_OK) != 0) {
                continue;
            }
            const bool alreadyOpen =
                std::any_of(found.begin(), found.end(),
                            [&path](const LinuxDevice& device) { return device.path == path; });
            if (alreadyOpen) {
                continue;
            }
            LinuxDevice device;
            if (openDevice(path, device) >= 0) {
                found.push_back(std::move(device));
            }
        }

        std::sort(found.begin(), found.end(),
                  [](const LinuxDevice& a, const LinuxDevice& b) { return a.path < b.path; });

        if (callback) {
            const std::size_t common = std::min(found.size(), previousNames.size());
            for (std::size_t i = 0; i < common; ++i) {
                if (found[i].name != previousNames[i]) {
                    callback(static_cast<i32>(i), false);
                    callback(static_cast<i32>(i), true);
                }
            }
            for (std::size_t i = previousNames.size(); i < found.size(); ++i) {
                callback(static_cast<i32>(i), true);
            }
            for (std::size_t i = found.size(); i < previousNames.size(); ++i) {
                callback(static_cast<i32>(i), false);
            }
        }

        devices = std::move(found);
    }

    void poll() {
        ensureInotify();
        if (!scanned || pollInotify()) {
            scan();
            scanned = true;
            return;
        }
        for (auto& device : devices) {
            readDevice(device);
        }
    }
};

LinuxJoysticks& linuxJoysticks() {
    static LinuxJoysticks manager;
    return manager;
}

LinuxJoysticks::Impl& LinuxJoysticks::impl() {
    static Impl instance;
    return instance;
}

const LinuxJoysticks::Impl& LinuxJoysticks::impl() const {
    return const_cast<LinuxJoysticks*>(this)->impl();
}

bool LinuxJoysticks::isPresent(i32 jid) const {
    const auto& instance = impl();
    return jid >= 0 && static_cast<std::size_t>(jid) < instance.devices.size();
}

std::string LinuxJoysticks::getName(i32 jid) const {
    return isPresent(jid) ? impl().devices[static_cast<std::size_t>(jid)].name : std::string{};
}

std::string LinuxJoysticks::getGuid(i32 jid) const {
    return isPresent(jid) ? impl().devices[static_cast<std::size_t>(jid)].guid : std::string{};
}

std::span<const f32> LinuxJoysticks::getAxes(i32 jid) const {
    return isPresent(jid) ? std::span<const f32>(
                                impl().devices[static_cast<std::size_t>(jid)].axes)
                          : std::span<const f32>{};
}

std::span<const u8> LinuxJoysticks::getButtons(i32 jid) const {
    return isPresent(jid) ? std::span<const u8>(
                                impl().devices[static_cast<std::size_t>(jid)].buttons)
                          : std::span<const u8>{};
}

std::span<const JoystickHat> LinuxJoysticks::getHats(i32 jid) const {
    return isPresent(jid) ? std::span<const JoystickHat>(
                                impl().devices[static_cast<std::size_t>(jid)].hats)
                          : std::span<const JoystickHat>{};
}

void LinuxJoysticks::setUserPointer(i32 jid, void* pointer) {
    if (isPresent(jid)) {
        impl().devices[static_cast<std::size_t>(jid)].userPointer = pointer;
    }
}

void* LinuxJoysticks::getUserPointer(i32 jid) const {
    return isPresent(jid) ? impl().devices[static_cast<std::size_t>(jid)].userPointer : nullptr;
}

bool LinuxJoysticks::isGamepad(i32 jid) const {
    return isPresent(jid) && impl().devices[static_cast<std::size_t>(jid)].gamepad;
}

std::string LinuxJoysticks::getGamepadName(i32 jid) const {
    return isGamepad(jid) ? getName(jid) : std::string{};
}

std::optional<GamepadState> LinuxJoysticks::getGamepadState(i32 jid) const {
    if (!isGamepad(jid)) {
        return std::nullopt;
    }
    return impl().devices[static_cast<std::size_t>(jid)].gamepadState;
}

void LinuxJoysticks::setCallback(std::function<void(i32, bool)> callback) {
    impl().callback = std::move(callback);
}

void LinuxJoysticks::poll() {
    impl().poll();
}

} // namespace sgl::detail
