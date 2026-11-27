#include <catch_amalgamated.hpp>

#include <array>
#include <cstdlib>
#include <memory>
#include <string>

#include <sgl/GL.hpp>
#include <sgl/Shader.hpp>
#include <sgl/Texture.hpp>
#include <sgl/sgl.hpp>

using namespace sgl;

namespace {

struct GlTestContext {
    GlTestContext() {
        sgl::SetInitHint(sgl::InitHint::Platform, static_cast<i64>(sgl::Platform::X11));
        if (auto result = sgl::Init(); !result) {
            return;
        }
        sgl::WindowHints hints;
        hints.width = 64;
        hints.height = 64;
        hints.title = "sgl shader/texture test";
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
        GL::Load();
    }

    ~GlTestContext() {
        window.reset();
        sgl::Terminate();
    }

    std::unique_ptr<sgl::Window> window;
};

} // namespace

TEST_CASE("Shader und Texture arbeiten mit einem aktuellen GL-Kontext", "[gl][shader][texture]") {
    if (std::getenv("DISPLAY") == nullptr) {
        SKIP("DISPLAY ist nicht gesetzt.");
    }

    GlTestContext context;
    if (!context.window) {
        SKIP("Kein OpenGL-Kontext verfügbar.");
    }
    REQUIRE(GL::glCreateShader != nullptr);
    REQUIRE(GL::glCreateTextures != nullptr);

    SECTION("Shader kompilieren, linken und Uniform setzen") {
        const std::string source = R"(
#pragma stage vertex
layout(location = 0) in vec3 aPosition;
void main() {
    gl_Position = vec4(aPosition, 1.0);
}

#pragma stage fragment
layout(location = 0) out vec4 fragColor;
uniform vec4 uColor;
void main() {
    fragColor = uColor;
}
)";

        Shader shader = Shader::FromSource(source);
        REQUIRE(shader.isValid());
        REQUIRE(shader.hasUniform("uColor"));
        shader.use();
        shader.setUniform("uColor", vec4(0.25f, 0.5f, 0.75f, 1.0f));
        REQUIRE(shader.uniformType("uColor") == GL_FLOAT_VEC4);
    }

    SECTION("Texture2d aus Image hochladen und zurücklesen") {
        Image image(2, 2, ImageFormat::Rgba);
        image.fill(std::array<u8, 4>{10, 20, 30, 255});
        image.setAt(1, 1, 0, 200);

        TextureOptions options;
        options.mipmaps = MipmapMode::None;
        Texture2d texture = Texture2d::FromImage(image, options);
        REQUIRE(texture.isValid());
        REQUIRE(texture.width() == 2);
        REQUIRE(texture.height() == 2);

        const Image read = texture.readBack();
        REQUIRE(read.width() == 2);
        REQUIRE(read.height() == 2);
        REQUIRE(read.at(0, 0, 0) == 10);
        REQUIRE(read.at(1, 1, 0) == 200);
    }
}
