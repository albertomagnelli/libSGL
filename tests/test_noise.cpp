#include <catch_amalgamated.hpp>

#include <cstdlib>
#include <memory>

#include <sgl/GL.hpp>
#include <sgl/Noise.hpp>
#include <sgl/sgl.hpp>

using namespace sgl;

TEST_CASE("Noise erzeugt eine Rauschtextur", "[noise][gl]") {
    if (std::getenv("DISPLAY") == nullptr) {
        SKIP("DISPLAY ist nicht gesetzt.");
    }

    if (auto result = sgl::Init(); !result) {
        SKIP("sgl konnte nicht initialisiert werden.");
    }

    sgl::WindowHints hints;
    hints.width = 64;
    hints.height = 64;
    hints.title = "sgl noise test";
    hints.visible = false;
    hints.contextVersionMajor = 4;
    hints.contextVersionMinor = 6;
    hints.contextProfile = sgl::ContextProfile::Core;

    auto windowResult = sgl::Window::Create(hints);
    if (!windowResult) {
        sgl::Terminate();
        SKIP("Kein OpenGL-Kontext verfuegbar.");
    }
    std::unique_ptr<sgl::Window> window = std::move(*windowResult);
    window->makeContextCurrent();
    GL::Load();

    {
        NoiseConfig config;
        config.dimension = 2;
        config.width = 64;
        config.height = 64;
        config.octaves = 3;
        config.seed = 1234;

        Noise noise;
        REQUIRE(noise.create(config));
        REQUIRE(noise.isValid());
        REQUIRE(noise.width() == 64);
        REQUIRE(noise.height() == 64);
        REQUIRE(noise.dimension() == 2);
        REQUIRE_FALSE(noise.is3D());

        const Image image = noise.toImage();
        REQUIRE(image.width() == 64);
        REQUIRE(image.height() == 64);
        REQUIRE(image.format() == ImageFormat::Rgba);
        REQUIRE_FALSE(image.isEmpty());
    }

    window.reset();
    sgl::Terminate();
}
