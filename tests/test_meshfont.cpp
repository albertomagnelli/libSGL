#include <catch_amalgamated.hpp>

#include <filesystem>

#include <sgl/MeshFont.hpp>

using namespace sgl;

TEST_CASE("Font2d tesselliert Glyphen aus einer Systemschrift", "[meshfont]") {
    const std::filesystem::path path = FindSystemFont();
    if (path.empty()) {
        SKIP("Keine TrueType-Systemschrift gefunden.");
    }

    const Font2d font = Font2d::FromFile(path);
    REQUIRE(font.isValid());
    REQUIRE(font.glyphCount() > 0);
    REQUIRE(font.ascent() > 0.0f);
    REQUIRE(font.lineHeight() > 0.0f);
    REQUIRE(font.hasGlyph(U'A'));
    REQUIRE(font.measure("Hallo", 32.0f) > 0.0f);

    const GlyphMesh mesh = font.glyphMesh(U'A', MeshQuality::Normal);
    REQUIRE_FALSE(mesh.isEmpty());
    REQUIRE(mesh.triangleCount() > 0);
    REQUIRE_FALSE(mesh.positions.empty());
    REQUIRE(mesh.indices.size() % 3 == 0);
}

TEST_CASE("Font3d erzeugt extrudierte Glyphennetze", "[meshfont]") {
    const std::filesystem::path path = FindSystemFont();
    if (path.empty()) {
        SKIP("Keine TrueType-Systemschrift gefunden.");
    }

    const Font3d font = Font3d::FromFile(path);
    REQUIRE(font.isValid());

    const GlyphMesh3d mesh = font.glyphMesh3d(U'A', 0.2f, MeshQuality::Low);
    REQUIRE_FALSE(mesh.isEmpty());
    REQUIRE_FALSE(mesh.positions.empty());
    REQUIRE_FALSE(mesh.normals.empty());
    REQUIRE(mesh.normals.size() == mesh.positions.size());
    REQUIRE(mesh.indices.size() % 3 == 0);
}
