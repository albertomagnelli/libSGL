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
    REQUIRE(shape.IsValid());
    REQUIRE(shape.Width() == 4);
    REQUIRE(shape.Height() == 4);
    REQUIRE(shape.OpaqueCount() == 1);
    REQUIRE(shape.Coverage() == Catch::Approx(1.0f / 16.0f));

    // Standard-Pivot ist die Mitte (2,2); lokale Koordinate (0,0) trifft Pixel (2,2).
    REQUIRE(shape.ContainsPoint(vec2(0.0f, 0.0f)));
    REQUIRE_FALSE(shape.ContainsPoint(vec2(2.0f, 2.0f)));
    REQUIRE(shape.ContainsWorldPoint(vec2(10.0f, 10.0f), vec2(10.0f, 10.0f), 0.0f));
}

TEST_CASE("CollisionShape erkennt Ueberlappungen", "[collision][geometry]") {
    std::vector<u8> mask(8 * 8, 255);
    const CollisionShape shape = CollisionShape::FromAlphaMask(mask.data(), 8, 8);

    REQUIRE(shape.Overlaps(shape, vec2(0.0f, 0.0f), 0.0f, vec2(4.0f, 0.0f), 0.0f));
    REQUIRE_FALSE(shape.Overlaps(shape, vec2(0.0f, 0.0f), 0.0f, vec2(100.0f, 0.0f), 0.0f));
}

TEST_CASE("Sprite baut ohne GL-Kontext eine Deckungsform", "[sprite]") {
    Image image(4, 4, ImageFormat::Rgba);
    image.fill(std::array<u8, 4>{0, 0, 0, 255});

    Sprite sprite;
    REQUIRE(sprite.LoadFromImage(image));
    REQUIRE_FALSE(sprite.IsValid());  // ohne GL-Kontext keine Textur
    REQUIRE(sprite.HasShape());
    REQUIRE(sprite.Width() == 4);
    REQUIRE(sprite.Height() == 4);

    REQUIRE(sprite.ContainsPoint(vec2(2.0f, 2.0f), vec2(2.0f, 2.0f), 0.0f));
    REQUIRE_FALSE(sprite.ContainsPoint(vec2(20.0f, 20.0f), vec2(2.0f, 2.0f), 0.0f));
}

TEST_CASE("AnimatedSprite spielt Raster-Animationen ab", "[animatedsprite]") {
    Image sheet(8, 8, ImageFormat::Rgba);
    sheet.fill(std::array<u8, 4>{0, 0, 0, 255});

    AnimatedSprite sprite;
    REQUIRE(sprite.LoadFromGrid(sheet, 4, 4, 0.1f));
    REQUIRE(sprite.IsValid());
    REQUIRE(sprite.AnimationCount() == 1);
    REQUIRE(sprite.TotalFrameCount() == 4);
    REQUIRE(sprite.FrameCount() == 4);
    REQUIRE(sprite.CurrentAnimation() == "default");
    REQUIRE(sprite.HasShape());
    REQUIRE(sprite.Shape().IsValid());
    REQUIRE(sprite.Size() == vec2(4.0f, 4.0f));

    REQUIRE(sprite.FrameIndex() == 0);
    sprite.Update(0.15f);
    REQUIRE(sprite.FrameIndex() == 1);
    sprite.SetFrame(3);
    REQUIRE(sprite.FrameIndex() == 3);

    REQUIRE(sprite.ContainsPoint(vec2(2.0f, 2.0f), vec2(2.0f, 2.0f), 0.0f));
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
    ImageIO::save(sheet, imageFile);

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
    REQUIRE(sprite.LoadFromFile(imageFile.string()));
    REQUIRE(sprite.IsValid());
    REQUIRE(sprite.AnimationCount() == 1);
    REQUIRE(sprite.HasAnimation("laufen"));
    REQUIRE(sprite.TotalFrameCount() == 2);
    REQUIRE(sprite.Play("laufen"));
    REQUIRE(sprite.CurrentAnimation() == "laufen");
    REQUIRE(sprite.FrameCount() == 2);
    REQUIRE(sprite.CurrentFrameDuration() == Catch::Approx(0.1f));

    std::filesystem::remove(imageFile);
    std::filesystem::remove(jsonFile);
}
