#include <catch_amalgamated.hpp>

#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include <sgl/Image.hpp>
#include <sgl/Model.hpp>

using namespace sgl;

namespace {

class TempDirectory {
public:
    explicit TempDirectory(const std::string& name) {
        m_path = std::filesystem::temp_directory_path() / ("sgl-obj-" + name);
        std::error_code error;
        std::filesystem::remove_all(m_path, error);
        std::filesystem::create_directories(m_path, error);
    }

    ~TempDirectory() {
        std::error_code error;
        std::filesystem::remove_all(m_path, error);
    }

    TempDirectory(const TempDirectory&) = delete;
    TempDirectory& operator=(const TempDirectory&) = delete;

    [[nodiscard]] const std::filesystem::path& GetPath() const noexcept { return m_path; }

private:
    std::filesystem::path m_path;
};

void WriteText(const std::filesystem::path& path, const std::string& text) {
    std::ofstream stream(path, std::ios::binary);
    REQUIRE(stream);
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    REQUIRE(stream);
}

std::vector<u8> ReadBytes(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    REQUIRE(stream);

    const std::streamoff size = stream.tellg();
    REQUIRE(size >= 0);
    stream.seekg(0, std::ios::beg);

    std::vector<u8> bytes(static_cast<std::size_t>(size));
    if (size > 0) {
        REQUIRE(stream.read(reinterpret_cast<char*>(bytes.data()), size));
    }

    return bytes;
}

std::filesystem::path ModelResource(const std::string& name) {
    return std::filesystem::path(SGL_TEST_MODEL_RESOURCES) / name;
}

} // namespace

TEST_CASE("ModelIO laedt ein OBJ-Modell", "[model][obj]") {
    const std::filesystem::path path = ModelResource("FuturisticGirl.obj");
    REQUIRE(std::filesystem::exists(path));

    const auto result = ModelIO::Load(path);
    REQUIRE(result);

    const Model& model = *result;
    REQUIRE(model.isValid());
    REQUIRE(model.getName() == "FuturisticGirl");
    REQUIRE_FALSE(model.getMeshes().empty());
    REQUIRE(model.getNodes().size() == model.getMeshes().size());

    const Mesh& mesh = model.getMeshes().front();
    REQUIRE(mesh.isValid());
    REQUIRE(mesh.getPrimitiveMode() == MeshPrimitiveMode::Triangles);
    REQUIRE(mesh.getVertices().size() > 1000);
    REQUIRE(mesh.getIndices().size() > 1000);
    REQUIRE(mesh.getIndices().size() % 3 == 0);
    REQUIRE(mesh.getMaterial() != nullptr);
    REQUIRE(mesh.getNodeIndex() >= 0);
    REQUIRE(model.getNodes()[static_cast<std::size_t>(mesh.getNodeIndex())].mesh == 0);

    const auto animations = ModelAnimationIO::Load(path);
    REQUIRE(animations);
    REQUIRE(animations->empty());
}

TEST_CASE("ModelIO trianguliert OBJ-Quads und loest negative Indizes auf", "[model][obj]") {
    TempDirectory directory("quad");
    const std::filesystem::path obj = directory.GetPath() / "quad.obj";
    const std::filesystem::path mtl = directory.GetPath() / "quad.mtl";

    WriteText(mtl, "newmtl Rot\nKd 1 0 0\n");
    WriteText(obj, R"(# Quad
mtllib quad.mtl
v 0 0 0
v 1 0 0
v 1 1 0
v 0 1 0
vt 0 0
vt 1 0
vt 1 1
vt 0 1
usemtl Rot
f -4/-4 -3/-3 -2/-2 -1/-1
)");

    const auto result = ModelIO::Load(obj);
    REQUIRE(result);

    const Model& model = *result;
    REQUIRE(model.isValid());
    REQUIRE(model.getMeshes().size() == 1);

    const Mesh& mesh = model.getMeshes().front();
    // Ohne vn/s erzeugt der Loader Flat-Normalen; die beiden Dreiecke teilen
    // daher keine Eckvertices.
    REQUIRE(mesh.getVertices().size() >= 4);
    REQUIRE(mesh.getIndices().size() == 6);
    REQUIRE(mesh.getMaterial() != nullptr);
    REQUIRE(mesh.getMaterial()->getAlbedoMap().getChannel(0, 0, 0) == 255);
    REQUIRE(mesh.getMaterial()->getAlbedoMap().getChannel(0, 0, 1) == 0);
}

TEST_CASE("ModelLoadOptions::flipObjV dreht OBJ-UVs", "[model][obj]") {
    TempDirectory directory("flip");
    const std::filesystem::path obj = directory.GetPath() / "triangle.obj";

    WriteText(obj, R"(v 0 0 0
v 1 0 0
v 0 1 0
vt 0 0
vt 1 0
vt 0 1
f 1/1 2/2 3/3
)");

    ModelLoadOptions options;
    options.flipObjV = false;

    const auto unflipped = ModelIO::Load(obj, options);
    REQUIRE(unflipped);
    REQUIRE(unflipped->getMeshes().front().getVertices().front().getUv().y == Catch::Approx(0.0f));

    options.flipObjV = true;

    const auto flipped = ModelIO::Load(obj, options);
    REQUIRE(flipped);
    REQUIRE(flipped->getMeshes().front().getVertices().front().getUv().y == Catch::Approx(1.0f));
}

TEST_CASE("OBJ ohne vn erzeugt gueltige Normalen", "[model][obj]") {
    TempDirectory directory("normals");
    const std::filesystem::path obj = directory.GetPath() / "flat.obj";

    WriteText(obj, R"(v 0 0 0
v 1 0 0
v 0 1 0
f 1 2 3
)");

    const auto result = ModelIO::Load(obj);
    REQUIRE(result);

    const Mesh& mesh = result->getMeshes().front();
    REQUIRE(mesh.getVertices().size() == 3);

    for (const MeshVertex& vertex : mesh.getVertices()) {
        REQUIRE(glm::length(vertex.getNormal()) == Catch::Approx(1.0f));
        REQUIRE(vertex.getNormal().z == Catch::Approx(1.0f));
    }
}

TEST_CASE("OBJ-Smoothing-Gruppen teilen Normalen", "[model][obj]") {
    TempDirectory directory("smooth");
    const std::filesystem::path obj = directory.GetPath() / "smooth.obj";

    WriteText(obj, R"(v 0 0 0
v 1 0 0
v 1 1 0
v 0 1 0
s 1
f 1 2 3
f 1 3 4
)");

    const auto result = ModelIO::Load(obj);
    REQUIRE(result);

    const Mesh& mesh = result->getMeshes().front();
    REQUIRE(mesh.getVertices().size() == 4);
    REQUIRE(mesh.getIndices().size() == 6);

    for (const MeshVertex& vertex : mesh.getVertices()) {
        REQUIRE(glm::length(vertex.getNormal()) == Catch::Approx(1.0f));
        REQUIRE(vertex.getNormal().z == Catch::Approx(1.0f));
    }
}

TEST_CASE("ModelIO laedt OBJ mit MTL-Texturen aus dem Speicher", "[model][obj]") {
    TempDirectory directory("textured");
    const std::filesystem::path obj = directory.GetPath() / "textured.obj";
    const std::filesystem::path mtl = directory.GetPath() / "textured.mtl";
    const std::filesystem::path texture = directory.GetPath() / "texture.png";

    Image image(2, 2, ImageFormat::Rgb);
    image.fill(std::array<u8, 4>{20, 40, 60, 255});
    ImageIO::Save(image, texture);
    REQUIRE(std::filesystem::exists(texture));

    WriteText(mtl, R"(newmtl Textured
Kd 1 1 1
Ke 0.2 0.1 0.0
d 0.5
map_Kd texture.png
)");

    WriteText(obj, R"(mtllib textured.mtl
v 0 0 0
v 1 0 0
v 0 1 0
vt 0 0
vt 1 0
vt 0 1
usemtl Textured
f 1/1 2/2 3/3
)");

    const std::vector<u8> bytes = ReadBytes(obj);

    const auto fromMemory = ModelIO::LoadFromMemory(bytes, directory.GetPath());
    REQUIRE(fromMemory);

    const Mesh& mesh = fromMemory->getMeshes().front();
    REQUIRE(mesh.getMaterial() != nullptr);
    REQUIRE_FALSE(mesh.getMaterial()->isAlbedoConstant());
    REQUIRE(mesh.getMaterial()->getAlbedoMap().getWidth() == 2);
    REQUIRE(mesh.getMaterial()->getAlphaMode() == MaterialAlphaMode::Blend);

    const auto animations = ModelAnimationIO::LoadFromMemory(bytes, directory.GetPath());
    REQUIRE(animations);
    REQUIRE(animations->empty());

    ModelLoadOptions options;
    options.loadMaterials = false;

    const auto withoutMaterials = ModelIO::Load(obj, options);
    REQUIRE(withoutMaterials);
    REQUIRE(withoutMaterials->getMeshes().front().getMaterial() != nullptr);
    REQUIRE(withoutMaterials->getMeshes().front().getMaterial()->isAlbedoConstant());
    REQUIRE(withoutMaterials->getMeshes().front().getMaterial()->getAlbedoMap().getChannel(
                0, 0, 0) == 255);
}

TEST_CASE("ModelIO meldet OBJ ohne Flaechen", "[model][obj]") {
    TempDirectory directory("empty");
    const std::filesystem::path obj = directory.GetPath() / "empty.obj";

    WriteText(obj, "v 0 0 0\nv 1 0 0\nv 0 1 0\n");

    const auto result = ModelIO::Load(obj);
    REQUIRE_FALSE(result);
    REQUIRE(result.error().what() != nullptr);
}
