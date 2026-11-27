#include <catch_amalgamated.hpp>

#include <cstdlib>
#include <memory>

#include <sgl/Framebuffer.hpp>
#include <sgl/GL.hpp>
#include <sgl/Image.hpp>
#include <sgl/Sprite.hpp>
#include <sgl/sgl.hpp>

using namespace sgl;

namespace {

struct FramebufferTestContext {
    FramebufferTestContext() {
        sgl::SetInitHint(sgl::InitHint::Platform, static_cast<i64>(sgl::Platform::X11));
        if (auto result = sgl::Init(); !result) {
            return;
        }
        sgl::WindowHints hints;
        hints.width = 64;
        hints.height = 64;
        hints.title = "sgl framebuffer test";
        hints.visible = false;
        hints.contextVersionMajor = 4;
        hints.contextVersionMinor = 6;
        hints.contextProfile = sgl::ContextProfile::Core;
        auto window = sgl::Window::Create(hints);
        if (!window) {
            sgl::Terminate();
            return;
        }
        this->window = std::move(*window);
        this->window->makeContextCurrent();
        gl::Load();
    }

    ~FramebufferTestContext() {
        window.reset();
        sgl::Terminate();
    }

    std::unique_ptr<sgl::Window> window;
};

}  // namespace

TEST_CASE("Framebuffer legt Anhaenge an und liest Pixel zurueck", "[gl][framebuffer]") {
    if (std::getenv("DISPLAY") == nullptr) {
        SKIP("DISPLAY ist nicht gesetzt.");
    }

    FramebufferTestContext context;
    if (!context.window) {
        SKIP("Kein OpenGL-Kontext verfügbar.");
    }

    FramebufferOptions options;
    options.isDepth = true;
    Framebuffer framebuffer(32, 32, options);

    REQUIRE(framebuffer.isValid());
    REQUIRE(framebuffer.isComplete());
    REQUIRE(framebuffer.width() == 32);
    REQUIRE(framebuffer.height() == 32);
    REQUIRE(framebuffer.hasDepth());
    REQUIRE_FALSE(framebuffer.isMultisampled());
    REQUIRE_FALSE(framebuffer.describe().empty());

    framebuffer.clear(Color(255, 0, 0, 255));

    const Image image = framebuffer.readPixels();
    REQUIRE(image.width() == 32);
    REQUIRE(image.height() == 32);
    REQUIRE(image.pixelAt(0, 0) == std::array<u8, 4>{255, 0, 0, 255});

    std::array<u8, 4> pixel{};
    REQUIRE(framebuffer.readPixel(5, 5, pixel.data()));
    REQUIRE(pixel[0] == 255);
    REQUIRE(pixel[1] == 0);
    REQUIRE(pixel[2] == 0);
    REQUIRE(pixel[3] == 255);

    const GLuint id = framebuffer.id();
    framebuffer.resize(16, 16);
    REQUIRE(framebuffer.id() == id);
    REQUIRE(framebuffer.width() == 16);
    REQUIRE(framebuffer.height() == 16);
    REQUIRE(framebuffer.isComplete());

    framebuffer.clear(Color(0, 0, 255, 255));
    REQUIRE(framebuffer.readPixels().pixelAt(0, 0) == std::array<u8, 4>{0, 0, 255, 255});
}

TEST_CASE("Sprite zeichnet in ein Framebuffer", "[gl][framebuffer][sprite]") {
    if (std::getenv("DISPLAY") == nullptr) {
        SKIP("DISPLAY ist nicht gesetzt.");
    }

    FramebufferTestContext context;
    if (!context.window) {
        SKIP("Kein OpenGL-Kontext verfügbar.");
    }

    Image image(8, 8, ImageFormat::Rgba);
    image.fill(std::array<u8, 4>{255, 0, 0, 255});

    Sprite sprite;
    REQUIRE(sprite.loadFromImage(image));
    REQUIRE(sprite.isValid());
    REQUIRE(sprite.hasShape());

    Framebuffer framebuffer(32, 32);
    framebuffer.bind();
    framebuffer.setViewport();
    framebuffer.clear(Color(0, 0, 0, 255));

    DrawSprite(sprite, vec2(16.0f, 16.0f), 0.0f);

    Framebuffer::BindDefault();

    const Image result = framebuffer.readPixels();
    const std::array<u8, 4> pixel = result.pixelAt(16, 16);
    REQUIRE(pixel[0] > 200);
    REQUIRE(pixel[1] < 50);
    REQUIRE(pixel[2] < 50);
    REQUIRE(pixel[3] > 200);
}

TEST_CASE("MSAA-Framebuffer laesst sich aufloesen", "[gl][framebuffer]") {
    if (std::getenv("DISPLAY") == nullptr) {
        SKIP("DISPLAY ist nicht gesetzt.");
    }

    FramebufferTestContext context;
    if (!context.window) {
        SKIP("Kein OpenGL-Kontext verfügbar.");
    }

    FramebufferOptions options;
    options.samples = SampleCount::Four;
    Framebuffer multisampled(32, 32, options);
    Framebuffer resolved(32, 32);

    REQUIRE(multisampled.isMultisampled());
    REQUIRE(resolved.isMultisampled() == false);

    multisampled.clear(Color(0, 255, 0, 255));
    ResolveFramebuffer(multisampled, resolved);

    const Image image = resolved.readPixels();
    REQUIRE(image.pixelAt(0, 0) == std::array<u8, 4>{0, 255, 0, 255});
}
