#include <catch_amalgamated.hpp>

#include <array>
#include <filesystem>
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

    const std::vector<u8> encoded = ImageIO::Encode(image, ImageFileFormat::Png);
    REQUIRE_FALSE(encoded.empty());

    const Image loaded = ImageIO::LoadFromMemory(encoded);
    REQUIRE(loaded.width() == 3);
    REQUIRE(loaded.height() == 2);
    REQUIRE(loaded.channelCount() == 4);
    REQUIRE(loaded.pixelAt(0, 0) == std::array<u8, 4>{10, 20, 30, 255});
}

TEST_CASE("ImageIO speichert und lädt Dateien", "[image][io]") {
    Image image(3, 2, ImageFormat::Rgba);
    image.fill(std::array<u8, 4>{10, 20, 30, 255});

    const std::filesystem::path file =
        std::filesystem::temp_directory_path() / "sgl-imageio-test.png";
    std::filesystem::remove(file);

    ImageIO::Save(image, file);
    REQUIRE(std::filesystem::exists(file));

    const Image loaded = ImageIO::Load(file);
    REQUIRE(loaded.width() == 3);
    REQUIRE(loaded.height() == 2);
    REQUIRE(loaded.pixelAt(0, 0) == std::array<u8, 4>{10, 20, 30, 255});

    std::filesystem::remove(file);
}

TEST_CASE("ImageIO Try-Varianten melden Fehler ohne Ausnahmen", "[image][io]") {
    const std::filesystem::path missing =
        std::filesystem::temp_directory_path() / "sgl-imageio-does-not-exist.png";
    std::filesystem::remove(missing);

    const auto loadResult = ImageIO::TryLoad(missing);
    REQUIRE_FALSE(loadResult.has_value());
    REQUIRE(*loadResult.error().what() != '\0');

    const auto memoryResult = ImageIO::TryLoadFromMemory({});
    REQUIRE_FALSE(memoryResult.has_value());
    REQUIRE(*memoryResult.error().what() != '\0');

    Image empty;
    const auto encodeResult = ImageIO::TryEncode(empty, ImageFileFormat::Png);
    REQUIRE_FALSE(encodeResult.has_value());

    const std::filesystem::path unsupported =
        std::filesystem::temp_directory_path() / "sgl-imageio-test.unknown";
    const auto saveResult = ImageIO::TrySave(empty, unsupported);
    REQUIRE_FALSE(saveResult.has_value());
}

TEST_CASE("ImageIO Try-Varianten runden PNG im Speicher", "[image][io]") {
    Image image(2, 2, ImageFormat::Rgba);
    image.fill(std::array<u8, 4>{1, 2, 3, 255});

    const auto encoded = ImageIO::TrySaveToMemory(image, ImageFileFormat::Png);
    REQUIRE(encoded.has_value());
    REQUIRE_FALSE(encoded->empty());

    const auto decoded = ImageIO::TryLoadFromMemory(*encoded);
    REQUIRE(decoded.has_value());
    REQUIRE(decoded->width() == 2);
    REQUIRE(decoded->height() == 2);
    REQUIRE(decoded->pixelAt(0, 0) == std::array<u8, 4>{1, 2, 3, 255});

    const std::filesystem::path file =
        std::filesystem::temp_directory_path() / "sgl-imageio-try-test.png";
    std::filesystem::remove(file);

    const auto saved = ImageIO::TrySave(image, file);
    REQUIRE(saved.has_value());
    REQUIRE(std::filesystem::exists(file));

    const auto loaded = ImageIO::TryLoad(file);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->width() == 2);

    std::filesystem::remove(file);
}
