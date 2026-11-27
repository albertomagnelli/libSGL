#include <catch_amalgamated.hpp>

#include <array>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include <sgl/sgl.hpp>

namespace {

struct InitGuard {
    InitGuard() = default;
    ~InitGuard() { sgl::Terminate(); }
    InitGuard(const InitGuard&) = delete;
    InitGuard& operator=(const InitGuard&) = delete;
};

} // namespace

TEST_CASE("Version information is available", "[core]") {
    const sgl::Version version = sgl::GetVersion();
    REQUIRE(version.major >= 0);
    REQUIRE(version.minor >= 0);
    REQUIRE_FALSE(sgl::GetVersionString().empty());
}

TEST_CASE("Errors can be reported and consumed", "[core][error]") {
    sgl::Error captured;
    sgl::SetErrorCallback([&captured](const sgl::Error& error) { captured = error; });

    sgl::ReportError(sgl::ErrorCode::InvalidValue, "kaputt");
    const sgl::Error error = sgl::GetError();

    REQUIRE(error.code == sgl::ErrorCode::InvalidValue);
    REQUIRE(error.message == "kaputt");
    REQUIRE(captured.code == sgl::ErrorCode::InvalidValue);
    REQUIRE_FALSE(error.isError() == false);

    REQUIRE_FALSE(sgl::GetError().isError());
    sgl::SetErrorCallback(nullptr);
}

TEST_CASE("Modifier helpers combine flags", "[core][input]") {
    const sgl::Modifiers modifiers =
        sgl::Modifiers::Shift | sgl::Modifiers::Control | sgl::Modifiers::Super;
    REQUIRE(sgl::HasModifier(modifiers, sgl::Modifiers::Shift));
    REQUIRE(sgl::HasModifier(modifiers, sgl::Modifiers::Control));
    REQUIRE(sgl::HasModifier(modifiers, sgl::Modifiers::Super));
    REQUIRE_FALSE(sgl::HasModifier(modifiers, sgl::Modifiers::Alt));

    sgl::Modifiers mutableModifiers = sgl::Modifiers::None;
    mutableModifiers |= sgl::Modifiers::Alt;
    REQUIRE(sgl::HasModifier(mutableModifiers, sgl::Modifiers::Alt));
}

TEST_CASE("Time is monotonic", "[core][time]") {
    const f64 first = sgl::GetTime();
    const f64 second = sgl::GetTime();
    REQUIRE(second >= first);
    REQUIRE(sgl::GetTimerFrequency() > 0);
}

TEST_CASE("The null platform initialises and rejects windows", "[null]") {
    InitGuard guard;
    sgl::SetInitHint(sgl::InitHint::Platform, static_cast<i64>(sgl::Platform::Null));

    auto result = sgl::Init();
    REQUIRE(result.has_value());
    REQUIRE(sgl::GetPlatform() == sgl::Platform::Null);

    auto window = sgl::Window::Create({});
    REQUIRE_FALSE(window.has_value());
    REQUIRE(window.error().code == sgl::ErrorCode::FeatureUnavailable);

    REQUIRE_FALSE(sgl::IsJoystickPresent(0));
    REQUIRE(sgl::GetMonitors().empty());
    REQUIRE_FALSE(sgl::GetPrimaryMonitor().isValid());
}

TEST_CASE("Functions without a backend report NotInitialized", "[core]") {
    sgl::Terminate();

    REQUIRE(sgl::GetProcAddress("glGetString") == nullptr);
    REQUIRE_FALSE(sgl::IsExtensionSupported("GL_ARB_framebuffer_object"));

    auto cursor = sgl::Cursor::CreateStandard(sgl::StandardCursor::Arrow);
    REQUIRE_FALSE(cursor.has_value());
    REQUIRE(cursor.error().code == sgl::ErrorCode::NotInitialized);

    REQUIRE(sgl::GetCurrentWindow() == nullptr);
}

TEST_CASE("Window hints have GLFW-compatible defaults", "[core][window]") {
    const sgl::WindowHints hints;
    REQUIRE(hints.width == 800);
    REQUIRE(hints.height == 600);
    REQUIRE(hints.resizable);
    REQUIRE(hints.visible);
    REQUIRE(hints.decorated);
    REQUIRE(hints.doublebuffer);
    REQUIRE(hints.contextVersionMajor == 1);
    REQUIRE(hints.contextVersionMinor == 0);
    REQUIRE(hints.clientApi == sgl::ClientApi::OpenGL);
    REQUIRE(hints.contextApi == sgl::ContextApi::Native);
}

TEST_CASE("A default Monitor is invalid", "[core][monitor]") {
    const sgl::Monitor monitor;
    REQUIRE_FALSE(monitor.isValid());
    REQUIRE_FALSE(monitor.isConnected());
}

TEST_CASE("X11 windows can be created and controlled", "[x11][window]") {
    if (std::getenv("DISPLAY") == nullptr) {
        SKIP("DISPLAY ist nicht gesetzt.");
    }

    InitGuard guard;
    sgl::SetInitHint(sgl::InitHint::Platform, static_cast<i64>(sgl::Platform::X11));

    auto result = sgl::Init();
    if (!result) {
        SKIP("X11 konnte nicht initialisiert werden: " + result.error().message);
    }
    REQUIRE(sgl::GetPlatform() == sgl::Platform::X11);

    sgl::WindowHints hints;
    hints.width = 320;
    hints.height = 240;
    hints.title = "sgl Testfenster";
    hints.visible = false;

    auto windowResult = sgl::Window::Create(hints);
    REQUIRE(windowResult.has_value());
    std::unique_ptr<sgl::Window> window = std::move(*windowResult);

    REQUIRE(window->getSize() == ivec2(320, 240));
    REQUIRE(window->getTitle() == "sgl Testfenster");
    REQUIRE_FALSE(window->isVisible());
    REQUIRE(window->isResizable());
    REQUIRE_FALSE(window->isCloseRequested());

    window->setTitle("Neuer Titel");
    REQUIRE(window->getTitle() == "Neuer Titel");

    window->show();
    sgl::PollEvents();
    REQUIRE(window->isVisible());

    window->setSize(ivec2(400, 300));
    for (int i = 0; i < 50 && window->getSize() != ivec2(400, 300); ++i) {
        sgl::WaitEventsTimeout(0.01);
    }
    CAPTURE(window->getSize().x, window->getSize().y);
    REQUIRE(window->getSize() == ivec2(400, 300));

    window->hide();
    sgl::PollEvents();
    REQUIRE_FALSE(window->isVisible());

    window->setCloseRequested(true);
    REQUIRE(window->isCloseRequested());
    window->setCloseRequested(false);
    REQUIRE_FALSE(window->isCloseRequested());

    window->setOpacity(0.5f);
    REQUIRE(window->getOpacity() == Catch::Approx(0.5f));

    const ivec2 framebuffer = window->getFramebufferSize();
    REQUIRE(framebuffer.x > 0);
    REQUIRE(framebuffer.y > 0);

    REQUIRE(window->getNativeHandle().platform == sgl::NativePlatform::X11);

    window->setClipboardString("sgl Zwischenablage");
    REQUIRE(window->getClipboardString() == "sgl Zwischenablage");

    auto cursor = sgl::Cursor::CreateStandard(sgl::StandardCursor::Hand);
    REQUIRE(cursor.has_value());
    window->setCursor(**cursor);
    window->setCursor(nullptr);

    sgl::WindowIcon icon;
    icon.size = ivec2(2, 2);
    icon.rgba8 = std::vector<u8>(16, 255);
    const std::array<sgl::WindowIcon, 1> icons{icon};
    window->setIcon(icons);

    REQUIRE_FALSE(window->isKeyPressed(sgl::Key::Escape));
    REQUIRE_FALSE(window->isMouseButtonPressed(sgl::MouseButton::Left));

    window.reset();
}

TEST_CASE("X11 faellt bei nicht verfuegbarem MSAA zurueck", "[x11][window]") {
    if (std::getenv("DISPLAY") == nullptr) {
        SKIP("DISPLAY ist nicht gesetzt.");
    }

    InitGuard guard;
    sgl::SetInitHint(sgl::InitHint::Platform, static_cast<i64>(sgl::Platform::X11));

    auto result = sgl::Init();
    if (!result) {
        SKIP("X11 konnte nicht initialisiert werden: " + result.error().message);
    }

    sgl::WindowHints hints;
    hints.width = 160;
    hints.height = 120;
    hints.visible = false;
    hints.samples = 64; // von praktisch keinem Treiber unterstuetzt

    auto windowResult = sgl::Window::Create(hints);
    REQUIRE(windowResult.has_value());
    std::unique_ptr<sgl::Window> window = std::move(*windowResult);
    REQUIRE(window->getFramebufferSize().x > 0);
    REQUIRE(window->getFramebufferSize().y > 0);
    window.reset();
}

TEST_CASE("X11 EGL contexts are supported", "[x11][egl]") {
    if (std::getenv("DISPLAY") == nullptr) {
        SKIP("DISPLAY ist nicht gesetzt.");
    }

    InitGuard guard;
    sgl::SetInitHint(sgl::InitHint::Platform, static_cast<i64>(sgl::Platform::X11));
    auto result = sgl::Init();
    if (!result) {
        SKIP("X11 konnte nicht initialisiert werden: " + result.error().message);
    }

    sgl::WindowHints hints;
    hints.width = 160;
    hints.height = 120;
    hints.title = "sgl EGL";
    hints.visible = false;
    hints.contextApi = sgl::ContextApi::EGL;
    hints.contextVersionMajor = 4;
    hints.contextVersionMinor = 6;
    hints.contextProfile = sgl::ContextProfile::Core;

    auto windowResult = sgl::Window::Create(hints);
    if (!windowResult) {
        SKIP("EGL-Kontext nicht verfuegbar: " + windowResult.error().message);
    }
    std::unique_ptr<sgl::Window> window = std::move(*windowResult);
    window->makeContextCurrent();
    REQUIRE(window->getNativeHandle().eglDisplay != nullptr);
    REQUIRE(window->getProcAddress("glGetString") != nullptr);
    window.reset();
}

TEST_CASE("Wayland init fails cleanly without a compositor", "[wayland]") {
    if (std::getenv("WAYLAND_DISPLAY") != nullptr) {
        SKIP("WAYLAND_DISPLAY ist gesetzt.");
    }

    InitGuard guard;
    sgl::SetInitHint(sgl::InitHint::Platform, static_cast<i64>(sgl::Platform::Wayland));
    auto result = sgl::Init();
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("X11 monitors are enumerated", "[x11][monitor]") {
    if (std::getenv("DISPLAY") == nullptr) {
        SKIP("DISPLAY ist nicht gesetzt.");
    }

    InitGuard guard;
    sgl::SetInitHint(sgl::InitHint::Platform, static_cast<i64>(sgl::Platform::X11));
    auto result = sgl::Init();
    if (!result) {
        SKIP("X11 konnte nicht initialisiert werden: " + result.error().message);
    }

    const auto monitors = sgl::GetMonitors();
    REQUIRE_FALSE(monitors.empty());
    for (const auto& monitor : monitors) {
        REQUIRE(monitor.isValid());
        REQUIRE_FALSE(monitor.getName().empty());
        REQUIRE(monitor.getVideoMode().size.x > 0);
        REQUIRE_FALSE(monitor.getVideoModes().empty());
    }
}
