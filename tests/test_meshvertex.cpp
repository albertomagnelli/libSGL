#include <catch_amalgamated.hpp>

#include <type_traits>

#include <sgl/MeshVertex.hpp>

using namespace sgl;

static_assert(std::is_trivially_copyable_v<MeshVertex>);
static_assert(std::is_standard_layout_v<MeshVertex>);

TEST_CASE("MeshVertex hat sinnvolle Standardwerte", "[meshvertex]") {
    const MeshVertex vertex;

    REQUIRE(vertex.getPosition() == vec3(0.0f));
    REQUIRE(vertex.getNormal() == vec3(0.0f, 0.0f, 1.0f));
    REQUIRE(vertex.getUv() == vec2(0.0f));
    REQUIRE(vertex.getTangent() == vec4(1.0f, 0.0f, 0.0f, 1.0f));
    REQUIRE(vertex.getBoneWeights() == vec4(0.0f));
    REQUIRE(vertex.getBoneIds() == uvec4(0u));
    REQUIRE_FALSE(vertex.hasSkinning());
    REQUIRE(vertex.getInfluenceCount() == 0);
}

TEST_CASE("MeshVertex-Attribute lassen sich per Setter setzen", "[meshvertex]") {
    MeshVertex vertex;
    vertex.setPosition(vec3(1.0f, 2.0f, 3.0f));
    vertex.setNormal(vec3(0.0f, 1.0f, 0.0f));
    vertex.setUv(vec2(0.25f, 0.75f));
    vertex.setTangent(vec4(1.0f, 0.0f, 0.0f, -1.0f));
    vertex.setBoneWeights(vec4(0.5f, 0.5f, 0.0f, 0.0f));
    vertex.setBoneIds(uvec4(3u, 7u, 0u, 0u));

    REQUIRE(vertex.getPosition() == vec3(1.0f, 2.0f, 3.0f));
    REQUIRE(vertex.getNormal() == vec3(0.0f, 1.0f, 0.0f));
    REQUIRE(vertex.getUv() == vec2(0.25f, 0.75f));
    REQUIRE(vertex.getTangent() == vec4(1.0f, 0.0f, 0.0f, -1.0f));
    REQUIRE(vertex.getBoneWeights() == vec4(0.5f, 0.5f, 0.0f, 0.0f));
    REQUIRE(vertex.getBoneIds() == uvec4(3u, 7u, 0u, 0u));
    REQUIRE(vertex.hasSkinning());
    REQUIRE(vertex.getInfluenceCount() == 2);
}

TEST_CASE("MeshVertex verwaltet Bone-Einfluesse", "[meshvertex]") {
    MeshVertex vertex;

    REQUIRE(vertex.setBoneInfluence(0, 4u, 0.25f));
    REQUIRE(vertex.setBoneInfluence(1, 9u, 0.75f));
    REQUIRE_FALSE(vertex.setBoneInfluence(4, 1u, 1.0f));

    REQUIRE(vertex.getInfluenceCount() == 2);
    REQUIRE(vertex.getBoneIds() == uvec4(4u, 9u, 0u, 0u));
    REQUIRE(vertex.getBoneWeights() == vec4(0.25f, 0.75f, 0.0f, 0.0f));

    vertex.normalizeBoneWeights();
    REQUIRE(vertex.getBoneWeights().x == Catch::Approx(0.25f));
    REQUIRE(vertex.getBoneWeights().y == Catch::Approx(0.75f));

    vertex.setBoneInfluence(2, 12u, -1.0f);
    REQUIRE(vertex.getBoneWeights().z == 0.0f);
    REQUIRE(vertex.hasSkinning());

    vertex.clearBoneInfluences();
    REQUIRE_FALSE(vertex.hasSkinning());
    REQUIRE(vertex.getInfluenceCount() == 0);
    REQUIRE(vertex.getBoneIds() == uvec4(0u));
}

TEST_CASE("MeshVertex normiert auch ungleiche Gewichte", "[meshvertex]") {
    MeshVertex vertex;
    vertex.setBoneWeights(vec4(1.0f, 3.0f, 4.0f, 0.0f));
    vertex.setBoneIds(uvec4(1u, 2u, 3u, 4u));

    vertex.normalizeBoneWeights();

    REQUIRE(vertex.getBoneWeights().x == Catch::Approx(0.125f));
    REQUIRE(vertex.getBoneWeights().y == Catch::Approx(0.375f));
    REQUIRE(vertex.getBoneWeights().z == Catch::Approx(0.5f));
    REQUIRE(vertex.getBoneWeights().w == Catch::Approx(0.0f));
    REQUIRE(vertex.getInfluenceCount() == 3);
}
