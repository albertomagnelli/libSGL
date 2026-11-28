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
    REQUIRE_FALSE(model.getMeshes().empty());
    REQUIRE_FALSE(model.getNodes().empty());
    REQUIRE_FALSE(model.getSkins().empty());

    const Mesh& mesh = model.getMeshes().front();
    REQUIRE(mesh.isValid());
    REQUIRE(mesh.getPrimitiveMode() == MeshPrimitiveMode::Triangles);
    REQUIRE_FALSE(mesh.getVertices().empty());
    REQUIRE_FALSE(mesh.getIndices().empty());
    REQUIRE(mesh.getMaterial() != nullptr);
    REQUIRE(mesh.getMaterial()->isValid());

    REQUIRE(mesh.getMaterial()->getAlbedoMap().getWidth() > 1);
    REQUIRE(mesh.getMaterial()->getAlphaMode() == MaterialAlphaMode::Opaque);
    REQUIRE_FALSE(mesh.getMaterial()->isDoubleSided());
    REQUIRE(mesh.getMaterial()->getNormalScale() == Catch::Approx(1.0f));
    REQUIRE(mesh.getMaterial()->getOcclusionStrength() == Catch::Approx(1.0f));
    REQUIRE(mesh.getMaterial()->getEmissiveStrength() == Catch::Approx(1.0f));

    const bool hasSkinning =
        std::any_of(mesh.getVertices().begin(), mesh.getVertices().end(),
                    [](const MeshVertex& vertex) { return vertex.hasSkinning(); });
    REQUIRE(hasSkinning);

    REQUIRE(std::all_of(mesh.getIndices().begin(), mesh.getIndices().end(),
                        [&mesh](const u32 index) { return index < mesh.getVertices().size(); }));

}

TEST_CASE("ModelAnimationIO laedt Animationsdaten", "[model][animation]") {
    const auto path = CharacterModel();
    REQUIRE(std::filesystem::exists(path));

    const auto result = ModelAnimationIO::Load(path);
    REQUIRE(result);
    REQUIRE_FALSE(result->empty());

    const ModelAnimation& animation = result->front();
    REQUIRE(animation.isValid());
    REQUIRE_FALSE(animation.getSamplers().empty());
    REQUIRE_FALSE(animation.getChannels().empty());
    REQUIRE(animation.getDuration() > 0.0f);

    for (const AnimationChannel& channel : animation.getChannels()) {
        REQUIRE(channel.node >= 0);
        REQUIRE(channel.sampler >= 0);
        REQUIRE(channel.sampler < static_cast<i32>(animation.getSamplers().size()));
    }

    const AnimationSampler& sampler = animation.getSamplers().front();
    REQUIRE(sampler.getKeyCount() > 0);
    REQUIRE(sampler.components >= 1);
    REQUIRE(sampler.components <= 4);
    REQUIRE(sampler.getKeyStride() > 0);
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
    REQUIRE(model.getMeshes().size() >= 2);
    REQUIRE_FALSE(model.getNodes().empty());
    REQUIRE_FALSE(model.getSkins().empty());

    const Mesh& mesh = model.getMeshes().front();
    REQUIRE(mesh.isValid());
    REQUIRE_FALSE(mesh.getVertices().empty());
    REQUIRE_FALSE(mesh.getIndices().empty());
    REQUIRE(mesh.getMaterial() != nullptr);
    REQUIRE(mesh.getMaterial()->isValid());
    REQUIRE(mesh.getMaterial()->getAlbedoMap().getWidth() > 1);
    REQUIRE(mesh.getNodeIndex() >= 0);

    // Regression: ufbx_triangulate_face() liefert Dreiecke, nicht Indizes.
    REQUIRE(model.getMeshes().size() >= 2);
    REQUIRE(model.getMeshes()[0].getIndices().size() == 492);
    REQUIRE(model.getMeshes()[1].getIndices().size() == 13152);

    for (const Mesh& candidate : model.getMeshes()) {
        REQUIRE(candidate.getIndices().size() % 3 == 0);
    }

    const bool hasSkinning =
        std::any_of(mesh.getVertices().begin(), mesh.getVertices().end(),
                    [](const MeshVertex& vertex) { return vertex.hasSkinning(); });
    REQUIRE(hasSkinning);

    const auto animations = ModelAnimationIO::Load(path);
    REQUIRE(animations);
    REQUIRE_FALSE(animations->empty());

    const ModelAnimation& animation = animations->front();
    REQUIRE(animation.isValid());
    REQUIRE(animation.getDuration() > 0.0f);
    REQUIRE_FALSE(animation.getChannels().empty());

    ModelLoadOptions rawOptions;
    rawOptions.flipFbxV = false;
    const auto raw = ModelIO::Load(path, rawOptions);
    REQUIRE(raw);
    REQUIRE(raw->getMeshes().size() == model.getMeshes().size());
    REQUIRE(raw->getMeshes()[0].getVertices().size() == mesh.getVertices().size());
    REQUIRE(mesh.getVertices()[0].getUv().y ==
            Catch::Approx(1.0f - raw->getMeshes()[0].getVertices()[0].getUv().y));

    // walk.fbx enthält nur das Skelett und die Animation; die Kanäle müssen
    // sich über die Node-Namen auf das Zombie-Skelett übertragen lassen.
    const auto walk = ModelAnimationIO::Load(
        std::filesystem::path(SGL_TEST_MODEL_RESOURCES) / "walk.fbx");
    REQUIRE(walk);
    REQUIRE_FALSE(walk->empty());
    REQUIRE_FALSE(walk->front().getChannels().empty());

    for (const AnimationChannel& channel : walk->front().getChannels()) {
        REQUIRE_FALSE(channel.nodeName.empty());
        const auto node = std::find_if(
            model.getNodes().begin(), model.getNodes().end(),
            [&channel](const ModelNode& candidate) { return candidate.name == channel.nodeName; });
        REQUIRE(node != model.getNodes().end());
    }
}

TEST_CASE("ModelIO meldet fehlende Dateien", "[model][gltf]") {
    const auto result = ModelIO::Load(
        std::filesystem::path(SGL_TEST_MODEL_RESOURCES) / "does-not-exist.glb");
    REQUIRE_FALSE(result);
    REQUIRE(result.error().what() != nullptr);
}
