#include <catch_amalgamated.hpp>

#include <array>

#include <sgl/Color.hpp>
#include <sgl/Image.hpp>
#include <sgl/Material.hpp>

using namespace sgl;

TEST_CASE("Material hat PBR-Standardwerte als 1x1-Bilder", "[material]") {
    const Material material;

    REQUIRE(material.isValid());
    REQUIRE(material.hasConsistentTextureSize());
    REQUIRE(material.getWidth() == 1);
    REQUIRE(material.getHeight() == 1);

    REQUIRE(material.isAlbedoConstant());
    REQUIRE(material.isMetallicConstant());
    REQUIRE(material.isRoughnessConstant());
    REQUIRE(material.isAmbientOcclusionConstant());
    REQUIRE(material.isNormalConstant());
    REQUIRE(material.isHeightConstant());
    REQUIRE(material.isEmissiveConstant());

    REQUIRE(material.getMetallicValue() == Catch::Approx(0.0f));
    REQUIRE(material.getRoughnessValue() == Catch::Approx(0.5f).margin(0.01f));
    REQUIRE(material.getAmbientOcclusionValue() == Catch::Approx(1.0f));
    REQUIRE(material.getHeightValue() == Catch::Approx(0.0f));

    REQUIRE(material.getAlbedoMap().pixelAt(0, 0) == std::array<u8, 4>{255, 255, 255, 0});
    REQUIRE(material.getNormalMap().pixelAt(0, 0) == std::array<u8, 4>{128, 128, 255, 0});
    REQUIRE(material.getEmissiveMap().pixelAt(0, 0) == std::array<u8, 4>{0, 0, 0, 0});

    REQUIRE(material.getAlphaMode() == MaterialAlphaMode::Opaque);
    REQUIRE(material.getAlphaCutoff() == Catch::Approx(0.5f));
    REQUIRE_FALSE(material.isDoubleSided());
    REQUIRE(material.getNormalScale() == Catch::Approx(1.0f));
    REQUIRE(material.getOcclusionStrength() == Catch::Approx(1.0f));
    REQUIRE(material.getEmissiveStrength() == Catch::Approx(1.0f));
}

TEST_CASE("Material traegt glTF-Renderparameter", "[material]") {
    Material material;

    material.setAlphaMode(MaterialAlphaMode::Mask);
    material.setAlphaCutoff(0.35f);
    material.setDoubleSided(true);
    material.setNormalScale(0.75f);
    material.setOcclusionStrength(0.5f);
    material.setEmissiveStrength(2.0f);

    REQUIRE(material.getAlphaMode() == MaterialAlphaMode::Mask);
    REQUIRE(material.getAlphaCutoff() == Catch::Approx(0.35f));
    REQUIRE(material.isDoubleSided());
    REQUIRE(material.getNormalScale() == Catch::Approx(0.75f));
    REQUIRE(material.getOcclusionStrength() == Catch::Approx(0.5f));
    REQUIRE(material.getEmissiveStrength() == Catch::Approx(2.0f));
}

TEST_CASE("Material erzeugt fuer Einzelwerte 1x1-Bilder", "[material]") {
    Material material;

    material.setMetallic(1.0f);
    material.setRoughness(0.25f);
    material.setAmbientOcclusion(0.75f);
    material.setHeight(0.4f);

    REQUIRE(material.getMetallicMap().width() == 1);
    REQUIRE(material.getMetallicMap().height() == 1);
    REQUIRE(material.getMetallicMap().format() == ImageFormat::Gray);
    REQUIRE(material.getMetallicValue() == Catch::Approx(1.0f));

    REQUIRE(material.getRoughnessValue() == Catch::Approx(0.25f).margin(0.01f));
    REQUIRE(material.getAmbientOcclusionValue() == Catch::Approx(0.75f).margin(0.01f));
    REQUIRE(material.getHeightValue() == Catch::Approx(0.4f).margin(0.01f));

    material.setAlbedo(Color(255, 128, 0));
    material.setEmissive(BLUE);
    material.setNormal(vec3(0.0f, 0.0f, 1.0f));

    REQUIRE(material.getAlbedoMap().width() == 1);
    REQUIRE(material.getAlbedoMap().height() == 1);
    REQUIRE(material.getAlbedoMap().format() == ImageFormat::Rgb);
    REQUIRE(material.getAlbedoMap().pixelAt(0, 0) == std::array<u8, 4>{255, 128, 0, 0});

    REQUIRE(material.getEmissiveMap().pixelAt(0, 0) == std::array<u8, 4>{0, 121, 241, 0});
    REQUIRE(material.getNormalMap().pixelAt(0, 0) == std::array<u8, 4>{128, 128, 255, 0});
}

TEST_CASE("Material extrahiert Kanaele aus gepackten Maps", "[material]") {
    Image packed(2, 2, ImageFormat::Rgba);
    packed.fill(std::array<u8, 4>{10, 20, 30, 40});

    Material material;
    material.setMetallic(packed, 0);
    material.setRoughness(packed, 1);
    material.setAmbientOcclusion(packed, 2);
    material.setHeight(packed, 3);

    REQUIRE(material.getMetallicMap().format() == ImageFormat::Gray);
    REQUIRE(material.getMetallicMap().width() == 2);
    REQUIRE(material.getMetallicMap().height() == 2);
    REQUIRE(material.getMetallicMap().pixelAt(1, 1) == std::array<u8, 4>{10, 0, 0, 0});
    REQUIRE(material.getRoughnessMap().pixelAt(0, 0) == std::array<u8, 4>{20, 0, 0, 0});
    REQUIRE(material.getAmbientOcclusionMap().pixelAt(0, 0) == std::array<u8, 4>{30, 0, 0, 0});
    REQUIRE(material.getHeightMap().pixelAt(0, 0) == std::array<u8, 4>{40, 0, 0, 0});
}

TEST_CASE("Material prueft Map-Groessen", "[material]") {
    Material material;

    Image albedo(4, 2, ImageFormat::Rgb);
    albedo.fill(std::array<u8, 4>{200, 100, 50, 255});
    material.setAlbedo(albedo);

    REQUIRE(material.getWidth() == 4);
    REQUIRE(material.getHeight() == 2);
    REQUIRE(material.hasConsistentTextureSize()); // andere Maps sind 1x1

    Image normal(4, 2, ImageFormat::Rgb);
    normal.fill(std::array<u8, 4>{128, 128, 255, 255});
    material.setNormal(normal);
    REQUIRE(material.hasConsistentTextureSize());

    Image wrongSize(3, 3, ImageFormat::Gray);
    wrongSize.fill(static_cast<u8>(128));
    material.setRoughness(wrongSize);
    REQUIRE_FALSE(material.hasConsistentTextureSize());
}

TEST_CASE("Material lehnt leere Bilder und ungueltige Kanaele ab", "[material]") {
    Material material;

    const Image empty;
    REQUIRE_THROWS_AS(material.setAlbedo(empty), MaterialError);
    REQUIRE_THROWS_AS(material.setMetallic(empty), MaterialError);
    REQUIRE_THROWS_AS(material.setNormal(empty), MaterialError);

    Image rgb(1, 1, ImageFormat::Rgb);
    rgb.fill(std::array<u8, 4>{1, 2, 3, 255});
    REQUIRE_THROWS_AS(material.setRoughness(rgb, 3), MaterialError);
}
