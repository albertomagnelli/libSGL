#include "X11Internal.hpp"

#include <cctype>
#include <cstdio>
#include <string>
#include <sys/select.h>
#include <unistd.h>
#include <vector>

namespace sgl::detail::x11 {
namespace {

std::string DecodeFileUri(const std::string& uri) {
    if (uri.rfind("file://", 0) != 0) {
        return {};
    }
    std::string path = uri.substr(7);
    if (path.rfind("localhost/", 0) == 0) {
        path = path.substr(9);
    } else if (!path.empty() && path[0] != '/') {
        const auto slash = path.find('/');
        if (slash == std::string::npos) {
            return {};
        }
        path = path.substr(slash);
    }

    std::string decoded;
    decoded.reserve(path.size());
    for (std::size_t i = 0; i < path.size(); ++i) {
        if (path[i] == '%' && i + 2 < path.size() &&
            std::isxdigit(static_cast<unsigned char>(path[i + 1])) &&
            std::isxdigit(static_cast<unsigned char>(path[i + 2]))) {
            const auto hex = path.substr(i + 1, 2);
            decoded.push_back(static_cast<char>(std::stoi(hex, nullptr, 16)));
            i += 2;
        } else {
            decoded.push_back(path[i]);
        }
    }
    return decoded;
}

std::vector<std::string> ParseUriList(const std::string& data) {
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
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::string path = DecodeFileUri(line);
        if (!path.empty()) {
            paths.push_back(std::move(path));
        }
    }
    return paths;
}

std::string ReadProperty(::Window window, Atom property) {
    State& s = GetX11State();
    Atom actualType = 0;
    int actualFormat = 0;
    unsigned long count = 0;
    unsigned long remaining = 0;
    unsigned char* data = nullptr;
    std::string result;

    if (XGetWindowProperty(s.display, window, property, 0, 0x1FFFFFFF, True, AnyPropertyType,
                           &actualType, &actualFormat, &count, &remaining, &data) == Success &&
        data) {
        result.assign(reinterpret_cast<char*>(data), count);
        XFree(data);
    }
    return result;
}

} // namespace

std::string X11WindowBackend::getClipboardString() const {
    State& s = GetX11State();
    if (s.clipboardOwned && XGetSelectionOwner(s.display, s.clipboard) == m_window) {
        return m_clipboardText;
    }

    s.selectionResult.clear();
    s.selectionReady = false;

    XConvertSelection(s.display, s.clipboard, s.utf8String, s.sglSelection, m_window, CurrentTime);
    XFlush(s.display);

    auto* self = const_cast<X11WindowBackend*>(this);
    for (int attempt = 0; attempt < 20 && !s.selectionReady; ++attempt) {
        if (XPending(s.display) > 0) {
            while (XPending(s.display) > 0) {
                XEvent event;
                XNextEvent(s.display, &event);
                if (event.type == SelectionNotify) {
                    self->handleSelectionNotify(event.xselection);
                } else {
                    self->handleEvent(event);
                }
            }
            continue;
        }

        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(ConnectionNumber(s.display), &readSet);
        timeval timeout{0, 100'000};
        const int result =
            select(ConnectionNumber(s.display) + 1, &readSet, nullptr, nullptr, &timeout);
        if (result <= 0) {
            break;
        }
    }

    if (!s.selectionReady) {
        ReportError(ErrorCode::PlatformError, "Zwischenablage konnte nicht gelesen werden.");
    }
    return s.selectionResult;
}

void X11WindowBackend::setClipboardString(std::string_view text) {
    State& s = GetX11State();
    m_clipboardText = std::string(text);
    s.clipboardString = m_clipboardText;
    s.clipboardOwned = true;
    XSetSelectionOwner(s.display, s.clipboard, m_window, CurrentTime);
    if (XGetSelectionOwner(s.display, s.clipboard) != m_window) {
        s.clipboardOwned = false;
        ReportError(ErrorCode::PlatformError, "Zwischenablage konnte nicht belegt werden.");
    }
}

void X11WindowBackend::handleSelectionRequest(XSelectionRequestEvent& event) {
    State& s = GetX11State();
    XSelectionEvent notify{};
    notify.type = SelectionNotify;
    notify.display = event.display;
    notify.requestor = event.requestor;
    notify.selection = event.selection;
    notify.target = event.target;
    notify.time = event.time;
    notify.property = X11None;

    Atom property = event.property != X11None ? event.property : event.target;

    if (event.selection == s.clipboard) {
        if (event.target == s.targets) {
            Atom targets[] = {s.targets, s.utf8String, XA_STRING};
            XChangeProperty(s.display, event.requestor, property, XA_ATOM, 32, PropModeReplace,
                            reinterpret_cast<const unsigned char*>(targets), 3);
            notify.property = property;
        } else if (event.target == s.utf8String || event.target == XA_STRING) {
            XChangeProperty(s.display, event.requestor, property, event.target, 8, PropModeReplace,
                            reinterpret_cast<const unsigned char*>(m_clipboardText.data()),
                            static_cast<int>(m_clipboardText.size()));
            notify.property = property;
        }
    }

    XSendEvent(s.display, event.requestor, False, NoEventMask,
               reinterpret_cast<XEvent*>(&notify));
    XFlush(s.display);
}

void X11WindowBackend::handleSelectionNotify(XSelectionEvent& event) {
    State& s = GetX11State();

    if (event.property == X11None) {
        s.selectionReady = true;
        return;
    }

    if (event.selection == s.xdndSelection) {
        const std::string data = ReadProperty(m_window, event.property);
        XDeleteProperty(s.display, m_window, event.property);

        const auto paths = ParseUriList(data);
        if (!paths.empty() && m_events) {
            m_events->onDrop(paths);
        }

        if (m_xdndSource != 0) {
            XEvent finished{};
            finished.type = ClientMessage;
            finished.xclient.window = m_xdndSource;
            finished.xclient.message_type = s.xdndFinished;
            finished.xclient.format = 32;
            finished.xclient.data.l[0] = static_cast<long>(m_window);
            finished.xclient.data.l[1] = 1;
            finished.xclient.data.l[2] = 1;
            XSendEvent(s.display, m_xdndSource, False, NoEventMask, &finished);
            m_xdndSource = 0;
        }
        return;
    }

    s.selectionResult = ReadProperty(m_window, event.property);
    XDeleteProperty(s.display, m_window, event.property);
    s.selectionReady = true;
}

void X11WindowBackend::handleSelectionClear(XSelectionClearEvent& event) {
    State& s = GetX11State();
    if (event.selection == s.clipboard) {
        s.clipboardOwned = false;
        m_clipboardText.clear();
    }
}

void X11WindowBackend::sendXdndStatus(XClientMessageEvent& event, bool accept) {
    State& s = GetX11State();
    XEvent status{};
    status.type = ClientMessage;
    status.xclient.window = static_cast<::Window>(event.data.l[0]);
    status.xclient.message_type = s.xdndStatus;
    status.xclient.format = 32;
    status.xclient.data.l[0] = static_cast<long>(m_window);
    status.xclient.data.l[1] = accept ? 1 : 0;
    status.xclient.data.l[2] = 0;
    status.xclient.data.l[3] = 0;
    status.xclient.data.l[4] = 1;
    XSendEvent(s.display, static_cast<::Window>(event.data.l[0]), False, NoEventMask, &status);
    XFlush(s.display);
}

void X11WindowBackend::handleXdnd(XClientMessageEvent& event) {
    State& s = GetX11State();
    if (event.message_type == s.xdndEnter) {
        m_xdndSource = static_cast<::Window>(event.data.l[0]);
    } else if (event.message_type == s.xdndPosition) {
        sendXdndStatus(event, true);
    } else if (event.message_type == s.xdndLeave) {
        m_xdndSource = 0;
    } else if (event.message_type == s.xdndDrop) {
        m_xdndSource = static_cast<::Window>(event.data.l[0]);
        XConvertSelection(s.display, s.xdndSelection, s.textUriList, s.sglSelection, m_window,
                          event.data.l[2] != 0 ? static_cast<Time>(event.data.l[2]) : CurrentTime);
        XFlush(s.display);
    }
}

} // namespace sgl::detail::x11
