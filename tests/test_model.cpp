#include <catch_amalgamated.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <vector>

#include <sgl/Model.hpp>

using namespace sgl;

namespace {

std::filesystem::path CharacterModel() {
    return std::filesystem::path(SGL_TEST_MODEL_RESOURCES) / "character.glb";
}

std::vector<u8> ReadBytes(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) return {};

    const std::streamoff size = stream.tellg();
    if (size <= 0) return {};

    std::vector<u8> bytes(static_cast<std::size_t>(size));
    stream.seekg(0, std::ios::beg);
    if (!stream.read(reinterpret_cast<char*>(bytes.data()), size)) return {};

    return bytes;
}

}  // namespace

TEST_CASE("ModelIO laedt ein skinned glTF-Modell", "[model][gltf]") {
    const auto path = CharacterModel();
    REQUIRE(std::filesystem::exists(path));

    const auto result = ModelIO::Load(path);
    REQUIRE(result);

    const Model& model = *result;
    REQUIRE(model.isValid());
    REQUIRE_FALSE(model.meshes.empty());
    REQUIRE_FALSE(model.nodes.empty());
    REQUIRE_FALSE(model.skins.empty());

    const Mesh& mesh = model.meshes.front();
    REQUIRE(mesh.isValid());
    REQUIRE(mesh.primitiveMode == MeshPrimitiveMode::Triangles);
    REQUIRE_FALSE(mesh.vertices.empty());
    REQUIRE_FALSE(mesh.indices.empty());
    REQUIRE(mesh.material != nullptr);
    REQUIRE(mesh.material->isValid());

    REQUIRE(mesh.material->getAlbedoMap().width() > 1);
    REQUIRE(mesh.material->getAlphaMode() == MaterialAlphaMode::Opaque);
    REQUIRE_FALSE(mesh.material->isDoubleSided());
    REQUIRE(mesh.material->getNormalScale() == Catch::Approx(1.0f));
    REQUIRE(mesh.material->getOcclusionStrength() == Catch::Approx(1.0f));
    REQUIRE(mesh.material->getEmissiveStrength() == Catch::Approx(1.0f));

    const bool hasSkinning =
        std::any_of(mesh.vertices.begin(), mesh.vertices.end(),
                    [](const MeshVertex& vertex) { return vertex.hasSkinning(); });
    REQUIRE(hasSkinning);

    REQUIRE(std::all_of(mesh.indices.begin(), mesh.indices.end(),
                        [&mesh](const u32 index) { return index < mesh.vertices.size(); }));

}

TEST_CASE("ModelAnimationIO laedt Animationsdaten", "[model][animation]") {
    const auto path = CharacterModel();
    REQUIRE(std::filesystem::exists(path));

    const auto result = ModelAnimationIO::Load(path);
    REQUIRE(result);
    REQUIRE_FALSE(result->empty());

    const ModelAnimation& animation = result->front();
    REQUIRE(animation.isValid());
    REQUIRE_FALSE(animation.samplers.empty());
    REQUIRE_FALSE(animation.channels.empty());
    REQUIRE(animation.duration > 0.0f);

    for (const AnimationChannel& channel : animation.channels) {
        REQUIRE(channel.node >= 0);
        REQUIRE(channel.sampler >= 0);
        REQUIRE(channel.sampler < static_cast<i32>(animation.samplers.size()));
    }

    const AnimationSampler& sampler = animation.samplers.front();
    REQUIRE(sampler.keyCount() > 0);
    REQUIRE(sampler.components >= 1);
    REQUIRE(sampler.components <= 4);
    REQUIRE(sampler.keyStride() > 0);
}

TEST_CASE("ModelIO und ModelAnimationIO laden aus dem Speicher", "[model][gltf]") {
    const auto path = CharacterModel();
    const std::vector<u8> bytes = ReadBytes(path);
    REQUIRE_FALSE(bytes.empty());

    const auto model = ModelIO::LoadFromMemory(bytes, path.parent_path());
    REQUIRE(model);
    REQUIRE(model->isValid());

    const auto animations = ModelAnimationIO::LoadFromMemory(bytes, path.parent_path());
    REQUIRE(animations);
    REQUIRE_FALSE(animations->empty());
}


TEST_CASE("ModelIO und ModelAnimationIO laden FBX", "[model][fbx]") {
    const std::filesystem::path path =
        std::filesystem::path(SGL_TEST_MODEL_RESOURCES) / "zombie.fbx";
    REQUIRE(std::filesystem::exists(path));

    const auto result = ModelIO::Load(path);
    REQUIRE(result);

    const Model& model = *result;
    REQUIRE(model.isValid());
    REQUIRE(model.meshes.size() >= 2);
    REQUIRE_FALSE(model.nodes.empty());
    REQUIRE_FALSE(model.skins.empty());

    const Mesh& mesh = model.meshes.front();
    REQUIRE(mesh.isValid());
    REQUIRE_FALSE(mesh.vertices.empty());
    REQUIRE_FALSE(mesh.indices.empty());
    REQUIRE(mesh.material != nullptr);
    REQUIRE(mesh.material->isValid());
    REQUIRE(mesh.material->getAlbedoMap().width() > 1);
    REQUIRE(mesh.nodeIndex >= 0);

    // Regression: ufbx_triangulate_face() liefert Dreiecke, nicht Indizes.
    REQUIRE(model.meshes.size() >= 2);
    REQUIRE(model.meshes[0].indices.size() == 492);
    REQUIRE(model.meshes[1].indices.size() == 13152);

    for (const Mesh& candidate : model.meshes) {
        REQUIRE(candidate.indices.size() % 3 == 0);
    }

    const bool hasSkinning =
        std::any_of(mesh.vertices.begin(), mesh.vertices.end(),
                    [](const MeshVertex& vertex) { return vertex.hasSkinning(); });
    REQUIRE(hasSkinning);

    const auto animations = ModelAnimationIO::Load(path);
    REQUIRE(animations);
    REQUIRE_FALSE(animations->empty());

    const ModelAnimation& animation = animations->front();
    REQUIRE(animation.isValid());
    REQUIRE(animation.duration > 0.0f);
    REQUIRE_FALSE(animation.channels.empty());

    ModelLoadOptions rawOptions;
    rawOptions.flipFbxV = false;
    const auto raw = ModelIO::Load(path, rawOptions);
    REQUIRE(raw);
    REQUIRE(raw->meshes.size() == model.meshes.size());
    REQUIRE(raw->meshes[0].vertices.size() == mesh.vertices.size());
    REQUIRE(mesh.vertices[0].uv.y ==
            Catch::Approx(1.0f - raw->meshes[0].vertices[0].uv.y));

    // walk.fbx enthält nur das Skelett und die Animation; die Kanäle müssen
    // sich über die Node-Namen auf das Zombie-Skelett übertragen lassen.
    const auto walk = ModelAnimationIO::Load(
        std::filesystem::path(SGL_TEST_MODEL_RESOURCES) / "walk.fbx");
    REQUIRE(walk);
    REQUIRE_FALSE(walk->empty());
    REQUIRE_FALSE(walk->front().channels.empty());

    for (const AnimationChannel& channel : walk->front().channels) {
        REQUIRE_FALSE(channel.nodeName.empty());
        const auto node = std::find_if(
            model.nodes.begin(), model.nodes.end(),
            [&channel](const ModelNode& candidate) { return candidate.name == channel.nodeName; });
        REQUIRE(node != model.nodes.end());
    }
}

TEST_CASE("ModelIO meldet fehlende Dateien", "[model][gltf]") {
    const auto result = ModelIO::Load(
        std::filesystem::path(SGL_TEST_MODEL_RESOURCES) / "does-not-exist.glb");
    REQUIRE_FALSE(result);
    REQUIRE(result.error().what() != nullptr);
}
