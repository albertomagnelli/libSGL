#include <algorithm>
#include <chrono>
#include <cstring>
#include <thread>
#include <vector>

#include "backend/Backend.hpp"
#include "backend/BackendFactory.hpp"

#if defined(SGL_HAS_OSMESA) && SGL_HAS_OSMESA
#include <GL/osmesa.h>
#endif

namespace sgl::detail {
namespace {

#if defined(SGL_HAS_OSMESA) && SGL_HAS_OSMESA

class OSMesaWindowBackend final : public WindowBackend {
public:
    OSMesaWindowBackend(const WindowHints& hints, Error& error)
        : m_size{hints.width, hints.height}, m_title(hints.title) {
        const int format = OSMESA_RGBA;
        const int profile = hints.contextProfile == ContextProfile::Core
                                ? OSMESA_CORE_PROFILE
                                : OSMESA_COMPAT_PROFILE;

        const int attribs[] = {
            OSMESA_FORMAT, format,
            OSMESA_PROFILE, profile,
            OSMESA_CONTEXT_MAJOR_VERSION, hints.contextVersionMajor,
            OSMESA_CONTEXT_MINOR_VERSION, hints.contextVersionMinor,
            0
        };

        m_context = OSMesaCreateContextAttribs(attribs, nullptr);
        if (!m_context) {
            error = makeError(ErrorCode::ApiUnavailable,
                              "OSMesa-Kontext konnte nicht erstellt werden.");
            return;
        }

        m_buffer.resize(static_cast<std::size_t>(std::max(hints.width, 1)) *
                        static_cast<std::size_t>(std::max(hints.height, 1)) * 4u);
        if (!OSMesaMakeCurrent(m_context, m_buffer.data(), GL_UNSIGNED_BYTE,
                               hints.width, hints.height)) {
            OSMesaDestroyContext(m_context);
            m_context = nullptr;
            error = makeError(ErrorCode::PlatformError,
                              "OSMesa-Kontext konnte nicht aktuell gemacht werden.");
        }
    }

    ~OSMesaWindowBackend() override {
        if (m_context) {
            OSMesaDestroyContext(m_context);
        }
    }

    void setEvents(WindowEvents* events) override { m_events = events; }
    void makeContextCurrent() override {
        if (m_context) {
            OSMesaMakeCurrent(m_context, m_buffer.data(), GL_UNSIGNED_BYTE,
                              m_size.x, m_size.y);
        }
    }
    void swapBuffers() override {}

    void* getProcAddress(const char* name) override {
        return reinterpret_cast<void*>(OSMesaGetProcAddress(name));
    }

    [[nodiscard]] NativeHandle getNativeHandle() const override {
        NativeHandle handle;
        handle.platform = NativePlatform::OSMesa;
        handle.context = m_context;
        handle.window = const_cast<u8*>(m_buffer.data());
        return handle;
    }

    [[nodiscard]] bool isCloseRequested() const override { return m_closeRequested; }
    void setCloseRequested(bool requested) override { m_closeRequested = requested; }
    [[nodiscard]] ivec2 getSize() const override { return m_size; }
    [[nodiscard]] ivec2 getFramebufferSize() const override { return m_size; }
    [[nodiscard]] std::string getTitle() const override { return m_title; }
    void setTitle(std::string_view title) override { m_title = title; }
    void setSize(ivec2 size) override {
        m_size = size;
        m_buffer.assign(static_cast<std::size_t>(std::max(size.x, 1)) *
                            static_cast<std::size_t>(std::max(size.y, 1)) * 4u, 0u);
        makeContextCurrent();
    }
    [[nodiscard]] i32 getAttrib(WindowAttrib attrib) const override {
        switch (attrib) {
        case WindowAttrib::Visible:
        case WindowAttrib::Resizable:
        case WindowAttrib::Decorated:
            return 1;
        default:
            return 0;
        }
    }
    [[nodiscard]] bool isKeyPressed(Key) const override { return false; }
    [[nodiscard]] bool isMouseButtonPressed(MouseButton) const override { return false; }
    [[nodiscard]] vec2 getCursorPos() const override { return {0.0f, 0.0f}; }
    [[nodiscard]] std::string getClipboardString() const override { return {}; }
    void setClipboardString(std::string_view) override {}

private:
    OSMesaContext m_context = nullptr;
    std::vector<u8> m_buffer;
    ivec2 m_size{0, 0};
    std::string m_title;
    WindowEvents* m_events = nullptr;
    bool m_closeRequested = false;
};

#endif

class NullBackend final : public Backend {
public:
    bool init(const InitOptions&, Error&) override { return true; }
    void terminate() override {}
    [[nodiscard]] Platform getPlatform() const override { return Platform::Null; }

    std::unique_ptr<WindowBackend> createWindow(const WindowHints& hints, Error& error) override {
        if (hints.contextApi == ContextApi::OSMesa) {
#if defined(SGL_HAS_OSMESA) && SGL_HAS_OSMESA
            auto window = std::make_unique<OSMesaWindowBackend>(hints, error);
            if (error.isError()) {
                return nullptr;
            }
            return window;
#else
            error = MakeError(ErrorCode::FeatureUnavailable,
                              "Dieser Build enthaelt keine OSMesa-Unterstuetzung.");
            return nullptr;
#endif
        }
        error = MakeError(ErrorCode::FeatureUnavailable,
                          "Die Null-Plattform kann keine Fenster erstellen.");
        return nullptr;
    }

    void pollEvents() override {}

    void waitEvents(f64 timeout) override {
        if (timeout < 0.0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        } else if (timeout > 0.0) {
            std::this_thread::sleep_for(std::chrono::duration<f64>(timeout));
        }
    }

    void postEmptyEvent() override {}
};

} // namespace

std::unique_ptr<Backend> CreateNullBackend(Error&) {
    return std::make_unique<NullBackend>();
}

} // namespace sgl::detail
