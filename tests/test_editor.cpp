#include <catch_amalgamated.hpp>

#include "doc/Document.hpp"

#include "common/Json.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace editor;

namespace {

fs::path MakeTempDirectory(const std::string &name) {
    const fs::path directory =
        fs::temp_directory_path() / ("sgl-editor-test-" + name + "-" +
                                     std::to_string(std::chrono::steady_clock::now()
                                                        .time_since_epoch()
                                                        .count()));
    std::error_code ec;
    fs::create_directories(directory, ec);
    return directory;
}

Level MakeLevel() {
    Level level = CreateLevel(6, 4, 32, 32, "test");
    level.tileLayers[0].tiles[0] = 1;
    level.tileLayers[0].tiles[7] = 5;
    ObjectLayer &objects = level.objectLayers[0];
    Entity entity;
    entity.id = NewEntityId(level);
    entity.def = "Gegner";
    entity.x = 32.0f;
    entity.y = 64.0f;
    entity.w = 32.0f;
    entity.h = 32.0f;
    entity.attrs["Name"] = "Wache";
    entity.attrs["Leben"] = "3";
    objects.entities.push_back(entity);
    return level;
}

} // namespace

TEST_CASE("Editor: Projekt und Level als JSON speichern und laden", "[editor][io]") {
    const fs::path directory = MakeTempDirectory("project");
    Project project = CreateProject("Testprojekt", directory);
    project.levels = {"levels/test.tmj"};
    project.tilesets.push_back(TileSet{"tiles", "assets/tiles.png", 1, 16, 16, 4, 16, 0, 0});

    std::string error;
    REQUIRE(SaveProject(project, directory / "project.sglproj", error));
    REQUIRE(error.empty());

    Project loaded;
    REQUIRE(LoadProject(directory / "project.sglproj", loaded, error));
    REQUIRE(loaded.name == "Testprojekt");
    REQUIRE(loaded.levels.size() == 1);
    REQUIRE(loaded.tilesets.size() == 1);
    REQUIRE(loaded.tilesets[0].name == "tiles");
    REQUIRE(loaded.entityDefs.size() >= 4);

    const Level level = MakeLevel();
    REQUIRE(SaveLevel(level, directory / "level.json", error));
    Level loadedLevel;
    REQUIRE(LoadLevel(directory / "level.json", loadedLevel, error));
    REQUIRE(loadedLevel.width == 6);
    REQUIRE(loadedLevel.height == 4);
    REQUIRE(loadedLevel.tileLayers.size() == 1);
    REQUIRE(loadedLevel.tileLayers[0].tiles[0] == 1);
    REQUIRE(loadedLevel.tileLayers[0].tiles[7] == 5);
    REQUIRE(loadedLevel.objectLayers.size() == 1);
    REQUIRE(loadedLevel.objectLayers[0].entities.size() == 1);
    REQUIRE(loadedLevel.objectLayers[0].entities[0].attrs.at("Name") == "Wache");

    fs::remove_all(directory);
}

TEST_CASE("Editor: Undo und Redo über Level-Snapshots", "[editor][history]") {
    Level level = MakeLevel();
    LevelHistory history;
    history.reset(level);
    REQUIRE_FALSE(history.canUndo());
    REQUIRE_FALSE(history.canRedo());

    level.tileLayers[0].tiles[0] = 42;
    history.commit(level);
    REQUIRE(history.canUndo());

    Level restored = level;
    REQUIRE(history.undo(restored));
    REQUIRE(restored.tileLayers[0].tiles[0] == 1);
    REQUIRE(history.canRedo());

    REQUIRE(history.redo(restored));
    REQUIRE(restored.tileLayers[0].tiles[0] == 42);

    level.tileLayers[0].tiles[1] = 7;
    history.commit(level);
    REQUIRE_FALSE(history.canRedo());
}

TEST_CASE("Editor: TMJ-Roundtrip erhält Kacheln und Entities", "[editor][tiled]") {
    const fs::path directory = MakeTempDirectory("tmj");
    Project project = CreateProject("Tiled", directory);
    project.tilesets.push_back(TileSet{"terrain", "assets/terrain.png", 1, 32, 32, 8, 32, 0, 0});
    Level level = MakeLevel();
    level.tilesets = project.tilesets;

    std::string error;
    const fs::path file = directory / "levels" / "level.tmj";
    REQUIRE(ExportTiled(file, level, project, error));
    REQUIRE(fs::exists(file));

    const std::string text = [&] {
        std::ifstream input(file);
        std::ostringstream buffer;
        buffer << input.rdbuf();
        return buffer.str();
    }();
    const sgl::detail::Json document = sgl::detail::Json::Parse(text);
    REQUIRE(document.isValid());
    const sgl::detail::Json *tilesets = document.find("tilesets");
    REQUIRE(tilesets != nullptr);
    REQUIRE(tilesets->isArray());
    REQUIRE(tilesets->size() == 1);
    REQUIRE(tilesets->at(0).find("image") != nullptr);
    REQUIRE(tilesets->at(0).find("image")->string() == "../assets/terrain.png");

    Level imported;
    REQUIRE(ImportTiled(file, imported, project, error));
    REQUIRE(imported.tilesets.size() == 1);
    REQUIRE(imported.tilesets[0].image == "assets/terrain.png");
    REQUIRE(imported.width == level.width);
    REQUIRE(imported.height == level.height);
    REQUIRE(imported.tileLayers.size() == 1);
    REQUIRE(imported.tileLayers[0].tiles[0] == 1);
    REQUIRE(imported.tileLayers[0].tiles[7] == 5);
    REQUIRE(imported.objectLayers.size() == 1);
    REQUIRE(imported.objectLayers[0].entities.size() == 1);
    REQUIRE(imported.objectLayers[0].entities[0].def == "Gegner");
    REQUIRE(imported.objectLayers[0].entities[0].attrs.at("Name") == "Wache");
    REQUIRE(imported.objectLayers[0].entities[0].attrs.at("Leben") == "3");

    fs::remove_all(directory);
}

TEST_CASE("Editor: TMX-Roundtrip mit CSV-Kacheldaten", "[editor][tiled]") {
    const fs::path directory = MakeTempDirectory("tmx");
    Project project = CreateProject("Tiled", directory);
    const Level level = MakeLevel();

    std::string error;
    const fs::path file = directory / "level.tmx";
    REQUIRE(ExportTiled(file, level, project, error));
    REQUIRE(fs::exists(file));

    Level imported;
    REQUIRE(ImportTiled(file, imported, project, error));
    REQUIRE(imported.width == level.width);
    REQUIRE(imported.height == level.height);
    REQUIRE(imported.tileLayers.size() == 1);
    REQUIRE(imported.tileLayers[0].tiles[0] == 1);
    REQUIRE(imported.tileLayers[0].tiles[7] == 5);
    REQUIRE(imported.objectLayers.size() == 1);
    REQUIRE(imported.objectLayers[0].entities.size() == 1);
    REQUIRE(imported.objectLayers[0].entities[0].def == "Gegner");

    fs::remove_all(directory);
}

TEST_CASE("Editor: Pfad- und Farbhelfer", "[editor][helpers]") {
    REQUIRE(PathToUtf8(PathFromUtf8("assets/level.tmj")) == "assets/level.tmj");
    REQUIRE(FileStem(PathFromUtf8("assets/level.tmj")) == "level");

    const sgl::Color color = ParseColor("#ff000080");
    REQUIRE(color.r == 255);
    REQUIRE(color.g == 0);
    REQUIRE(color.b == 0);
    REQUIRE(color.a == 128);
}

TEST_CASE("Editor: TMJ mit Base64 und zlib", "[editor][tiled][compression]") {
    const fs::path directory = MakeTempDirectory("tmj-zlib");
    Project project = CreateProject("Komprimiert", directory);
    const fs::path file = directory / "level.tmj";
    const std::string text = R"({
        "type":"map","width":2,"height":2,"tilewidth":32,"tileheight":32,
        "tilesets":[],
        "layers":[{
            "type":"tilelayer","name":"z","width":2,"height":2,
            "data":"eJxjZGBgYAJiZiBmAWIAAGAACw==",
            "encoding":"base64","compression":"zlib"
        }]
    })";
    {
        std::ofstream output(file, std::ios::binary);
        output << text;
    }
    Level level;
    std::string error;
    REQUIRE(ImportTiled(file, level, project, error));
    REQUIRE(level.tileLayers.size() == 1);
    REQUIRE(level.tileLayers[0].tiles.size() == 4);
    REQUIRE(level.tileLayers[0].tiles[0] == 1);
    REQUIRE(level.tileLayers[0].tiles[1] == 2);
    REQUIRE(level.tileLayers[0].tiles[2] == 3);
    REQUIRE(level.tileLayers[0].tiles[3] == 4);
    fs::remove_all(directory);
}
