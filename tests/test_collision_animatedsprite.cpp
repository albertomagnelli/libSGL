#include <catch_amalgamated.hpp>

#include <array>
#include <filesystem>
#include <fstream>
#include <vector>

#include <sgl/AnimatedSprite.hpp>
#include <sgl/CollisionShape.hpp>
#include <sgl/Image.hpp>
#include <sgl/Sprite.hpp>

using namespace sgl;

TEST_CASE("CollisionShape arbeitet mit einer Alpha-Maske", "[collision][geometry]") {
    std::vector<u8> mask(4 * 4, 0);
    mask[2 * 4 + 2] = 255;

    const CollisionShape shape = CollisionShape::FromAlphaMask(mask.data(), 4, 4);
    REQUIRE(shape.isValid());
    REQUIRE(shape.width() == 4);
    REQUIRE(shape.height() == 4);
    REQUIRE(shape.opaqueCount() == 1);
    REQUIRE(shape.coverage() == Catch::Approx(1.0f / 16.0f));

    // Standard-Pivot ist die Mitte (2,2); lokale Koordinate (0,0) trifft Pixel (2,2).
    REQUIRE(shape.containsPoint(vec2(0.0f, 0.0f)));
    REQUIRE_FALSE(shape.containsPoint(vec2(2.0f, 2.0f)));
    REQUIRE(shape.containsWorldPoint(vec2(10.0f, 10.0f), vec2(10.0f, 10.0f), 0.0f));
}

TEST_CASE("CollisionShape erkennt Ueberlappungen", "[collision][geometry]") {
    std::vector<u8> mask(8 * 8, 255);
    const CollisionShape shape = CollisionShape::FromAlphaMask(mask.data(), 8, 8);

    REQUIRE(shape.overlaps(shape, vec2(0.0f, 0.0f), 0.0f, vec2(4.0f, 0.0f), 0.0f));
    REQUIRE_FALSE(shape.overlaps(shape, vec2(0.0f, 0.0f), 0.0f, vec2(100.0f, 0.0f), 0.0f));
}

TEST_CASE("Sprite baut ohne GL-Kontext eine Deckungsform", "[sprite]") {
    Image image(4, 4, ImageFormat::Rgba);
    image.fill(std::array<u8, 4>{0, 0, 0, 255});

    Sprite sprite;
    REQUIRE(sprite.loadFromImage(image));
    REQUIRE_FALSE(sprite.isValid());  // ohne GL-Kontext keine Textur
    REQUIRE(sprite.hasShape());
    REQUIRE(sprite.width() == 4);
    REQUIRE(sprite.height() == 4);

    REQUIRE(sprite.containsPoint(vec2(2.0f, 2.0f), vec2(2.0f, 2.0f), 0.0f));
    REQUIRE_FALSE(sprite.containsPoint(vec2(20.0f, 20.0f), vec2(2.0f, 2.0f), 0.0f));
}

TEST_CASE("AnimatedSprite spielt Raster-Animationen ab", "[animatedsprite]") {
    Image sheet(8, 8, ImageFormat::Rgba);
    sheet.fill(std::array<u8, 4>{0, 0, 0, 255});

    AnimatedSprite sprite;
    REQUIRE(sprite.loadFromGrid(sheet, 4, 4, 0.1f));
    REQUIRE(sprite.isValid());
    REQUIRE(sprite.animationCount() == 1);
    REQUIRE(sprite.totalFrameCount() == 4);
    REQUIRE(sprite.frameCount() == 4);
    REQUIRE(sprite.currentAnimation() == "default");
    REQUIRE(sprite.hasShape());
    REQUIRE(sprite.shape().isValid());
    REQUIRE(sprite.size() == vec2(4.0f, 4.0f));

    REQUIRE(sprite.frameIndex() == 0);
    sprite.update(0.15f);
    REQUIRE(sprite.frameIndex() == 1);
    sprite.setFrame(3);
    REQUIRE(sprite.frameIndex() == 3);

    REQUIRE(sprite.containsPoint(vec2(2.0f, 2.0f), vec2(2.0f, 2.0f), 0.0f));
}

TEST_CASE("AnimatedSprite liest Aseprite-Metadaten", "[animatedsprite][json]") {
    const std::filesystem::path imageFile =
        std::filesystem::temp_directory_path() / "sgl-animated-test.png";
    const std::filesystem::path jsonFile =
        std::filesystem::temp_directory_path() / "sgl-animated-test.json";
    std::filesystem::remove(imageFile);
    std::filesystem::remove(jsonFile);

    Image sheet(8, 4, ImageFormat::Rgba);
    sheet.fill(std::array<u8, 4>{0, 0, 0, 255});
    ImageIO::Save(sheet, imageFile);

    {
        std::ofstream metadata(jsonFile);
        metadata << R"({
  "frames": [
    {"frame": {"x": 0, "y": 0, "w": 4, "h": 4}, "duration": 100},
    {"frame": {"x": 4, "y": 0, "w": 4, "h": 4}, "duration": 200}
  ],
  "meta": {
    "frameTags": [
      {"name": "laufen", "from": 0, "to": 1, "direction": "forward"}
    ]
  }
})";
    }

    AnimatedSprite sprite;
    REQUIRE(sprite.loadFromFile(imageFile.string()));
    REQUIRE(sprite.isValid());
    REQUIRE(sprite.animationCount() == 1);
    REQUIRE(sprite.hasAnimation("laufen"));
    REQUIRE(sprite.totalFrameCount() == 2);
    REQUIRE(sprite.play("laufen"));
    REQUIRE(sprite.currentAnimation() == "laufen");
    REQUIRE(sprite.frameCount() == 2);
    REQUIRE(sprite.currentFrameDuration() == Catch::Approx(0.1f));

    std::filesystem::remove(imageFile);
    std::filesystem::remove(jsonFile);
}
