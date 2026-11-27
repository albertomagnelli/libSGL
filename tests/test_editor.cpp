#include <catch_amalgamated.hpp>

#include "doc/Document.hpp"
#include "doc/Playtest.hpp"

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

TEST_CASE("Editor: TMJ mit Base64 und gzip", "[editor][tiled][compression]") {
    const fs::path directory = MakeTempDirectory("tmj-gzip");
    Project project = CreateProject("Komprimiert", directory);
    const fs::path file = directory / "level.tmj";
    const std::string text = R"({
        "type":"map","width":2,"height":2,"tilewidth":32,"tileheight":32,
        "tilesets":[],
        "layers":[{
            "type":"tilelayer","name":"g","width":2,"height":2,
            "data":"H4sIAFX3CWsC/2NkYGBgAmJmIGYBYgDv1AWvEAAAAA==",
            "encoding":"base64","compression":"gzip"
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
    REQUIRE(level.tileLayers[0].tiles[3] == 4);
    fs::remove_all(directory);
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

TEST_CASE("Editor: Kachelanimationen werden gelesen, geschrieben und aufgelöst", "[editor][tiled][animation]") {
    const fs::path directory = MakeTempDirectory("tmj-animation");
    Project project = CreateProject("Animation", directory);
    Level level = MakeLevel();
    TileSet tileset{"anim", "assets/anim.png", 1, 32, 32, 2, 2, 0, 0};
    tileset.animations[0].frames = {{1, 100}, {2, 200}};
    level.tilesets = {tileset};
    level.tileLayers[0].tiles[0] = 1;

    std::string error;
    const fs::path file = directory / "levels" / "anim.tmj";
    REQUIRE(ExportTiled(file, level, project, error));

    Level imported;
    REQUIRE(ImportTiled(file, imported, project, error));
    REQUIRE(imported.tilesets.size() == 1);
    REQUIRE(imported.tilesets[0].animations.size() == 1);
    REQUIRE(imported.tilesets[0].animations.at(0).frames.size() == 2);

    REQUIRE(AnimatedTile(imported, 1, 0.05f) == 2);
    REQUIRE(AnimatedTile(imported, 1, 0.15f) == 3);
    REQUIRE(AnimatedTile(imported, 1, 0.35f) == 2);

    fs::remove_all(directory);
}

TEST_CASE("Editor: Playtest landet, läuft und springt", "[editor][playtest]") {
    Level level = CreateLevel(8, 6, 32, 32, "playtest");
    for (int x = 0; x < 8; ++x) {
        *level.tilePtr(level.tileLayers[0], x, 5) = 1;
    }
    Entity start;
    start.id = NewEntityId(level);
    start.def = "Startpunkt";
    start.x = 64.0f;
    start.y = 128.0f;
    start.w = 32.0f;
    start.h = 32.0f;
    level.objectLayers[0].entities.push_back(start);

    Playtest playtest;
    playtest.reset(level);
    for (int i = 0; i < 120; ++i) {
        playtest.step(level, 1.0f / 60.0f, 0.0f, false);
    }
    REQUIRE(playtest.isGrounded());
    const f32 floor = 5.0f * 32.0f; // Zeile 5
    REQUIRE(playtest.position().y + playtest.size().y <= floor + 2.0f);

    const f32 groundedY = playtest.position().y;
    playtest.step(level, 1.0f / 60.0f, 0.0f, true);
    REQUIRE(playtest.position().y < groundedY);
}

TEST_CASE("Editor: Tiled-Template (.tj) wird aufgelöst und erhalten", "[editor][tiled][template]") {
    const fs::path directory = MakeTempDirectory("template-tj");
    Project project = CreateProject("Templates", directory);
    fs::create_directories(directory / "templates");
    fs::create_directories(directory / "levels");
    {
        std::ofstream tpl(directory / "templates" / "enemy.tj");
        tpl << R"({"type":"template","object":{"type":"Gegner","width":40,"height":50,
            "properties":[{"name":"Leben","type":"int","value":"10"},
                          {"name":"Tempo","type":"float","value":"1.5"}]}})";
    }
    {
        std::ofstream map(directory / "levels" / "map.tmj");
        map << R"({"type":"map","width":4,"height":3,"tilewidth":32,"tileheight":32,
            "tilesets":[],
            "layers":[{"type":"objectgroup","name":"Objekte","objects":[
                {"id":1,"name":"Wache","x":32,"y":64,"template":"../templates/enemy.tj",
                 "properties":[{"name":"Leben","type":"int","value":"25"}]}]}]})";
    }
    Level level;
    std::string error;
    REQUIRE(ImportTiled(directory / "levels" / "map.tmj", level, project, error));
    REQUIRE(level.objectLayers.size() == 1);
    REQUIRE(level.objectLayers[0].entities.size() == 1);
    const Entity &entity = level.objectLayers[0].entities[0];
    REQUIRE(entity.def == "Gegner");
    REQUIRE(entity.w == 40.0f);
    REQUIRE(entity.h == 50.0f);
    REQUIRE(entity.x == 32.0f);
    REQUIRE(entity.y == 64.0f);
    REQUIRE(entity.attrs.at("Name") == "Wache");
    REQUIRE(entity.attrs.at("Leben") == "25");
    REQUIRE(entity.attrs.at("Tempo") == "1.5");
    REQUIRE(entity.templatePath == "templates/enemy.tj");

    const fs::path exported = directory / "levels" / "out.tmj";
    REQUIRE(ExportTiled(exported, level, project, error));
    std::ifstream input(exported);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    REQUIRE(buffer.str().find("../templates/enemy.tj") != std::string::npos);
    fs::remove_all(directory);
}

TEST_CASE("Editor: Tiled-Template (.tx) per TMX", "[editor][tiled][template]") {
    const fs::path directory = MakeTempDirectory("template-tx");
    Project project = CreateProject("Templates", directory);
    fs::create_directories(directory / "templates");
    {
        std::ofstream tpl(directory / "templates" / "pickup.tx");
        tpl << R"(<?xml version="1.0" encoding="UTF-8"?>
            <template><object type="Beute" width="16" height="16">
              <properties><property name="Wert" type="int" value="5"/></properties>
            </object></template>)";
    }
    const fs::path mapFile = directory / "map.tmx";
    {
        std::ofstream map(mapFile);
        map << R"(<?xml version="1.0" encoding="UTF-8"?>
            <map version="1.10" orientation="orthogonal" width="4" height="3" tilewidth="32" tileheight="32">
              <objectgroup name="Objekte">
                <object id="1" name="Münze" x="32" y="32" template="templates/pickup.tx">
                  <properties><property name="Wert" type="int" value="9"/></properties>
                </object>
              </objectgroup>
            </map>)";
    }
    Level level;
    std::string error;
    REQUIRE(ImportTiled(mapFile, level, project, error));
    REQUIRE(level.objectLayers.size() == 1);
    REQUIRE(level.objectLayers[0].entities.size() == 1);
    const Entity &entity = level.objectLayers[0].entities[0];
    REQUIRE(entity.def == "Beute");
    REQUIRE(entity.w == 16.0f);
    REQUIRE(entity.attrs.at("Wert") == "9");
    REQUIRE(entity.templatePath == "templates/pickup.tx");
    fs::remove_all(directory);
}

TEST_CASE("Editor: Wang-Metadaten bleiben im TMJ erhalten", "[editor][tiled][wang]") {
    const fs::path directory = MakeTempDirectory("wang-tmj");
    Project project = CreateProject("Wang", directory);
    const fs::path file = directory / "map.tmj";
    {
        std::ofstream map(file);
        map << R"({"type":"map","width":2,"height":2,"tilewidth":32,"tileheight":32,
            "tilesets":[{"firstgid":1,"name":"terrain","image":"terrain.png",
              "tilewidth":32,"tileheight":32,"tilecount":4,"columns":2,
              "wangsets":[{"name":"Erde","type":"corner","tile":-1,
                "colors":[{"name":"Gras","color":"#00ff00","tile":-1,"probability":1}],
                "wangtiles":[{"tileid":0,"wangid":[1,1,1,1]}]}]}],
            "layers":[{"type":"tilelayer","name":"Kacheln","width":2,"height":2,
              "data":[1,1,1,1]}]})";
    }
    Level level;
    std::string error;
    REQUIRE(ImportTiled(file, level, project, error));
    REQUIRE(level.tilesets.size() == 1);
    REQUIRE(level.tilesets[0].wangsets.find("\"Erde\"") != std::string::npos);
    REQUIRE(level.tilesets[0].wangsets.find("\"wangtiles\"") != std::string::npos);

    const fs::path exported = directory / "out.tmj";
    REQUIRE(ExportTiled(exported, level, project, error));
    std::ifstream input(exported);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    REQUIRE(buffer.str().find("\"wangsets\"") != std::string::npos);
    REQUIRE(buffer.str().find("\"Erde\"") != std::string::npos);
    fs::remove_all(directory);
}

TEST_CASE("Editor: Wang-Metadaten werden zwischen TMX und JSON übersetzt", "[editor][tiled][wang]") {
    const fs::path directory = MakeTempDirectory("wang-tmx");
    Project project = CreateProject("Wang", directory);
    const fs::path file = directory / "map.tmx";
    {
        std::ofstream map(file);
        map << R"(<?xml version="1.0" encoding="UTF-8"?>
            <map version="1.10" orientation="orthogonal" width="2" height="2" tilewidth="32" tileheight="32">
              <tileset firstgid="1" name="terrain" tilewidth="32" tileheight="32" tilecount="4" columns="2">
                <image source="terrain.png"/>
                <wangsets>
                  <wangset name="Erde" type="corner" tile="-1">
                    <wangcolor name="Gras" color="#00ff00" tile="-1" probability="1"/>
                    <wangtile tileid="0" wangid="1,1,1,1"/>
                  </wangset>
                </wangsets>
              </tileset>
              <layer name="Kacheln" width="2" height="2">
                <data encoding="csv">1,1,1,1</data>
              </layer>
            </map>)";
    }
    Level level;
    std::string error;
    REQUIRE(ImportTiled(file, level, project, error));
    REQUIRE(level.tilesets.size() == 1);
    REQUIRE(level.tilesets[0].wangsets.find("\"Erde\"") != std::string::npos);

    const fs::path exported = directory / "out.tmx";
    REQUIRE(ExportTiled(exported, level, project, error));
    std::ifstream input(exported);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    REQUIRE(buffer.str().find("<wangsets>") != std::string::npos);
    REQUIRE(buffer.str().find("Erde") != std::string::npos);
    fs::remove_all(directory);
}

TEST_CASE("Editor: Projektformat erhält Wang- und Animationsmetadaten", "[editor][io][metadata]") {
    const fs::path directory = MakeTempDirectory("metadata");
    Project project = CreateProject("Metadaten", directory);
    TileSet tileset{"terrain", "assets/terrain.png", 1, 32, 32, 8, 32, 0, 0};
    tileset.wangsets = R"([{"name":"Erde","type":"corner"}])";
    tileset.animations[3].frames = {{3, 200}, {11, 300}};
    project.tilesets = {tileset};
    project.levels = {"levels/a.tmj"};

    std::string error;
    REQUIRE(SaveProject(project, directory / "project.sglproj", error));
    Project loadedProject;
    REQUIRE(LoadProject(directory / "project.sglproj", loadedProject, error));
    REQUIRE(loadedProject.tilesets.size() == 1);
    REQUIRE(loadedProject.tilesets[0].wangsets.find("Erde") != std::string::npos);
    REQUIRE(loadedProject.tilesets[0].animations.size() == 1);
    REQUIRE(loadedProject.tilesets[0].animations.at(3).frames.size() == 2);

    Level level = MakeLevel();
    level.tilesets = {tileset};
    REQUIRE(SaveLevel(level, directory / "level.json", error));
    Level loadedLevel;
    REQUIRE(LoadLevel(directory / "level.json", loadedLevel, error));
    REQUIRE(loadedLevel.tilesets.size() == 1);
    REQUIRE(loadedLevel.tilesets[0].wangsets.find("Erde") != std::string::npos);
    REQUIRE(loadedLevel.tilesets[0].animations.at(3).frames[1].tileId == 11);
    fs::remove_all(directory);
}
