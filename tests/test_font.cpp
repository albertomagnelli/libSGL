#include <catch_amalgamated.hpp>

#include <filesystem>
#include <string>
#include <vector>

#include <sgl/Font.hpp>

using namespace sgl;

TEST_CASE("UTF-8 wird korrekt dekodiert und kodiert", "[font]") {
    const std::vector<u32> codepoints = DecodeUtf8("AÄ€😀");
    REQUIRE(codepoints.size() == 4);
    REQUIRE(codepoints[0] == U'A');
    REQUIRE(codepoints[1] == U'Ä');
    REQUIRE(codepoints[2] == U'€');
    REQUIRE(codepoints[3] == 0x1F600u);

    REQUIRE(EncodeUtf8(U'A') == "A");
    REQUIRE(EncodeUtf8(U'Ä') == "Ä");
    REQUIRE(EncodeUtf8(0x1F600u) == "😀");
    REQUIRE(EncodeUtf8(0xD800u) == "\xEF\xBF\xBD");
}

TEST_CASE("Eingebettete Standardschrift ist verfügbar", "[font]") {
    const Font font = Font::LoadDefault();
    REQUIRE(font.isValid());
    REQUIRE_FALSE(font.getAtlas().isEmpty());
    REQUIRE(font.hasGlyph(U'A'));
    REQUIRE(font.hasGlyph(U'Ä'));
    REQUIRE(font.hasGlyph(U'?'));
    REQUIRE(font.getGlyph(U'A') != nullptr);
    REQUIRE(font.measure("Hallo", 16.0f) > 0.0f);
    REQUIRE(font.bounds("Hallo", vec2(10.0f, 20.0f), 16.0f).width > 0.0f);
    REQUIRE(Font::LineCount("a\nb\nc") == 3);
}

TEST_CASE("Font lässt sich speichern und wieder laden", "[font]") {
    const Font font = Font::LoadDefault();
    REQUIRE(font.isValid());

    const std::filesystem::path directory =
        std::filesystem::temp_directory_path() / "sgl-font-test";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const std::filesystem::path file = directory / "default.font";

    font.save(file);
    REQUIRE(std::filesystem::exists(file));
    REQUIRE(std::filesystem::exists(directory / "default.png"));

    const Font loaded = Font::Load(file);
    REQUIRE(loaded.isValid());
    REQUIRE(loaded.getGlyphs().size() == font.getGlyphs().size());
    REQUIRE(loaded.hasGlyph(U'A'));
    REQUIRE(loaded.measure("Hallo", 16.0f) ==
            Catch::Approx(font.measure("Hallo", 16.0f)));

    std::filesystem::remove_all(directory);
}
