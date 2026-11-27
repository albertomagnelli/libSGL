#include <catch_amalgamated.hpp>

#include <array>
#include <vector>

#include <sgl/Image.hpp>

using namespace sgl;

TEST_CASE("Image erzeugen, füllen und lesen", "[image]") {
    Image image(4, 3, ImageFormat::Rgba, 0);
    REQUIRE(image.width() == 4);
    REQUIRE(image.height() == 3);
    REQUIRE(image.channelCount() == 4);
    REQUIRE_FALSE(image.isEmpty());

    image.fill(std::array<u8, 4>{1, 2, 3, 4});
    REQUIRE(image.pixelAt(0, 0) == std::array<u8, 4>{1, 2, 3, 4});
    REQUIRE(image.pixelAt(3, 2) == std::array<u8, 4>{1, 2, 3, 4});

    image.setAt(1, 1, 0, 200);
    REQUIRE(image.at(1, 1, 0) == 200);

    image.fill(static_cast<u8>(7));
    REQUIRE(image.at(0, 0, 0) == 7);
}

TEST_CASE("Image spiegeln, drehen und skalieren", "[image]") {
    Image image(2, 2, ImageFormat::Gray);
    image.setAt(0, 0, 0, 1);
    image.setAt(1, 0, 0, 2);
    image.setAt(0, 1, 0, 3);
    image.setAt(1, 1, 0, 4);

    const Image flipped = image.flippedHorizontally();
    REQUIRE(flipped.at(0, 0, 0) == 2);
    REQUIRE(flipped.at(1, 0, 0) == 1);

    const Image rotated = image.rotated(90);
    REQUIRE(rotated.width() == 2);
    REQUIRE(rotated.height() == 2);
    REQUIRE(rotated.at(0, 0, 0) == 3);

    const Image scaled = image.scaled(1, 1, Interpolation::Nearest);
    REQUIRE(scaled.width() == 1);
    REQUIRE(scaled.height() == 1);

    const Image rgb = image.converted(ImageFormat::Rgb);
    REQUIRE(rgb.channelCount() == 3);
}

TEST_CASE("Image als PNG kodieren und wieder laden", "[image]") {
    Image image(3, 2, ImageFormat::Rgba);
    image.fill(std::array<u8, 4>{10, 20, 30, 255});

    const std::vector<u8> encoded = image.encode(ImageFileFormat::Png);
    REQUIRE_FALSE(encoded.empty());

    const Image loaded = Image::loadFromMemory(encoded);
    REQUIRE(loaded.width() == 3);
    REQUIRE(loaded.height() == 2);
    REQUIRE(loaded.channelCount() == 4);
    REQUIRE(loaded.pixelAt(0, 0) == std::array<u8, 4>{10, 20, 30, 255});
}
