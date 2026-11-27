#include <catch_amalgamated.hpp>

#include <type_traits>

#include <sgl/MeshVertex.hpp>

using namespace sgl;

static_assert(std::is_trivially_copyable_v<MeshVertex>);
static_assert(std::is_standard_layout_v<MeshVertex>);

TEST_CASE("MeshVertex hat sinnvolle Standardwerte", "[meshvertex]") {
    const MeshVertex vertex;

    REQUIRE(vertex.position == vec3(0.0f));
    REQUIRE(vertex.normal == vec3(0.0f, 0.0f, 1.0f));
    REQUIRE(vertex.uv == vec2(0.0f));
    REQUIRE(vertex.tangent == vec4(1.0f, 0.0f, 0.0f, 1.0f));
    REQUIRE(vertex.boneWeights == vec4(0.0f));
    REQUIRE(vertex.boneIds == uvec4(0u));
    REQUIRE_FALSE(vertex.hasSkinning());
    REQUIRE(vertex.influenceCount() == 0);
}

TEST_CASE("MeshVertex laesst sich als Aggregate initialisieren", "[meshvertex]") {
    const MeshVertex vertex{
        .position    = vec3(1.0f, 2.0f, 3.0f),
        .normal      = vec3(0.0f, 1.0f, 0.0f),
        .uv          = vec2(0.25f, 0.75f),
        .tangent     = vec4(1.0f, 0.0f, 0.0f, -1.0f),
        .boneWeights = vec4(0.5f, 0.5f, 0.0f, 0.0f),
        .boneIds     = uvec4(3u, 7u, 0u, 0u),
    };

    REQUIRE(vertex.position == vec3(1.0f, 2.0f, 3.0f));
    REQUIRE(vertex.normal == vec3(0.0f, 1.0f, 0.0f));
    REQUIRE(vertex.uv == vec2(0.25f, 0.75f));
    REQUIRE(vertex.tangent == vec4(1.0f, 0.0f, 0.0f, -1.0f));
    REQUIRE(vertex.boneWeights == vec4(0.5f, 0.5f, 0.0f, 0.0f));
    REQUIRE(vertex.boneIds == uvec4(3u, 7u, 0u, 0u));
    REQUIRE(vertex.hasSkinning());
    REQUIRE(vertex.influenceCount() == 2);
}

TEST_CASE("MeshVertex verwaltet Bone-Einfluesse", "[meshvertex]") {
    MeshVertex vertex;

    REQUIRE(vertex.setBoneInfluence(0, 4u, 0.25f));
    REQUIRE(vertex.setBoneInfluence(1, 9u, 0.75f));
    REQUIRE_FALSE(vertex.setBoneInfluence(4, 1u, 1.0f));

    REQUIRE(vertex.influenceCount() == 2);
    REQUIRE(vertex.boneIds == uvec4(4u, 9u, 0u, 0u));
    REQUIRE(vertex.boneWeights == vec4(0.25f, 0.75f, 0.0f, 0.0f));

    vertex.normalizeBoneWeights();
    REQUIRE(vertex.boneWeights.x == Catch::Approx(0.25f));
    REQUIRE(vertex.boneWeights.y == Catch::Approx(0.75f));

    vertex.setBoneInfluence(2, 12u, -1.0f);
    REQUIRE(vertex.boneWeights.z == 0.0f);
    REQUIRE(vertex.hasSkinning());

    vertex.clearBoneInfluences();
    REQUIRE_FALSE(vertex.hasSkinning());
    REQUIRE(vertex.influenceCount() == 0);
    REQUIRE(vertex.boneIds == uvec4(0u));
}

TEST_CASE("MeshVertex normiert auch ungleiche Gewichte", "[meshvertex]") {
    MeshVertex vertex;
    vertex.boneWeights = vec4(1.0f, 3.0f, 4.0f, 0.0f);
    vertex.boneIds = uvec4(1u, 2u, 3u, 4u);

    vertex.normalizeBoneWeights();

    REQUIRE(vertex.boneWeights.x == Catch::Approx(0.125f));
    REQUIRE(vertex.boneWeights.y == Catch::Approx(0.375f));
    REQUIRE(vertex.boneWeights.z == Catch::Approx(0.5f));
    REQUIRE(vertex.boneWeights.w == Catch::Approx(0.0f));
    REQUIRE(vertex.influenceCount() == 3);
}
