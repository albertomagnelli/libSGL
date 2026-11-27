#include "Document.hpp"
#include "JsonWriter.hpp"

#include "common/Json.hpp"

#include "tinyxml2.h"
#include "miniz.h"


#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string_view>

namespace editor {
namespace {

using sgl::detail::Json;

std::string ReadText(const fs::path &file) {
    std::ifstream input(file, std::ios::binary);
    if (!input) return {};
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::string Trim(std::string_view text) {
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) ++begin;
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) --end;
    return std::string(text.substr(begin, end - begin));
}

std::string MakeProjectRelative(const Project &project, const fs::path &file) {
    if (project.root.empty()) return PathToUtf8(file);
    std::error_code ec;
    const fs::path relative = fs::relative(file, project.root, ec);
    if (!ec && !relative.empty() && PathToUtf8(relative).rfind("..", 0) != 0) {
        return PathToUtf8(relative);
    }
    return PathToUtf8(file);
}

int ParseInt(const char *text, int fallback = 0) {
    if (text == nullptr) return fallback;
    char *end = nullptr;
    const long value = std::strtol(text, &end, 10);
    return end == text ? fallback : static_cast<int>(value);
}

float ParseFloat(const char *text, float fallback = 0.0f) {
    if (text == nullptr) return fallback;
    char *end = nullptr;
    const float value = std::strtof(text, &end);
    return end == text ? fallback : value;
}

bool ParseBool(const char *text, bool fallback = false) {
    if (text == nullptr) return fallback;
    const std::string_view value(text);
    return value == "1" || value == "true" || value == "True";
}

std::vector<u8> DecodeBase64(std::string_view text) {
    static constexpr signed char table[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
        -1,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    };
    std::vector<u8> result;
    result.reserve(text.size() * 3 / 4);
    int accumulator = 0;
    int bits = -8;
    for (const char c : text) {
        if (c == '=') break;
        const int value = table[static_cast<unsigned char>(c)];
        if (value < 0) continue;
        accumulator = (accumulator << 6) | value;
        bits += 6;
        if (bits >= 0) {
            result.push_back(static_cast<u8>((accumulator >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return result;
}

bool DecompressZlib(const std::vector<u8> &input, std::size_t expected, std::vector<u8> &output) {
    output.assign(expected, 0);
    mz_ulong length = static_cast<mz_ulong>(expected);
    if (mz_uncompress(output.data(), &length, input.data(), input.size()) != MZ_OK) {
        output.clear();
        return false;
    }
    output.resize(length);
    return true;
}

bool DecompressGzip(const std::vector<u8> &input, std::size_t expected, std::vector<u8> &output) {
    // Gzip = 10-Byte-Kopf, optionales Zusatzfeld/Name/Kommentar, roher Deflate-
    // Strom und 8-Byte-Trailer. miniz' zlib-API erwartet einen zlib-Kopf,
    // deshalb wird der Gzip-Kopf hier entfernt und roher Deflate entpackt.
    if (input.size() < 18 || input[0] != 0x1f || input[1] != 0x8b || input[2] != 8) {
        return false;
    }
    const u8 flags = input[3];
    std::size_t pos = 10;
    if ((flags & 0x04) != 0) {
        if (pos + 2 > input.size()) return false;
        const std::size_t extra = static_cast<std::size_t>(input[pos]) |
                                  (static_cast<std::size_t>(input[pos + 1]) << 8);
        pos += 2 + extra;
    }
    if ((flags & 0x08) != 0) {
        while (pos < input.size() && input[pos] != 0) ++pos;
        ++pos;
    }
    if ((flags & 0x10) != 0) {
        while (pos < input.size() && input[pos] != 0) ++pos;
        ++pos;
    }
    if ((flags & 0x02) != 0) pos += 2;
    if (pos + 8 > input.size()) return false;
    const std::size_t compressed = input.size() - pos - 8;
    if (compressed == 0) return false;
    output.assign(expected, 0);
    const std::size_t written = tinfl_decompress_mem_to_mem(
        output.data(), expected, input.data() + pos, compressed,
        TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
    if (written != expected) {
        output.clear();
        return false;
    }
    return true;
}

std::string JsonString(const Json *value, std::string_view fallback = {});
std::string JsonText(const Json &value);
std::string WangSetsFromXml(const tinyxml2::XMLElement *wangsets);

bool DecodeLayerData(const Json &layer, int width, int height, std::vector<u32> &tiles,
                     std::string &error) {
    tiles.assign(static_cast<std::size_t>(width) * height, 0);
    const Json *data = layer.find("data");
    if (data == nullptr) {
        error = "Kachelebene ohne Daten.";
        return false;
    }
    if (data->isArray()) {
        const int count = std::min(data->size(), static_cast<int>(tiles.size()));
        for (int i = 0; i < count; ++i) {
            tiles[static_cast<std::size_t>(i)] =
                static_cast<u32>(data->at(i).intValue()) & 0x1FFFFFFFu;
        }
        return true;
    }
    if (!data->isString()) {
        error = "Unbekanntes Datenformat in Kachelebene.";
        return false;
    }
    const std::string encoding = JsonString(layer.find("encoding"));
    const std::string compression = JsonString(layer.find("compression"));
    std::vector<u8> bytes = DecodeBase64(data->string());
    if (encoding != "base64") {
        error = "Nur CSV- und Base64-Kachelebenen werden unterstützt.";
        return false;
    }
    if (compression == "zlib") {
        std::vector<u8> unpacked;
        if (!DecompressZlib(bytes, static_cast<std::size_t>(width) * height * 4, unpacked)) {
            error = "zlib-Daten konnten nicht entpackt werden.";
            return false;
        }
        bytes = std::move(unpacked);
    } else if (compression == "gzip") {
        std::vector<u8> unpacked;
        if (!DecompressGzip(bytes, static_cast<std::size_t>(width) * height * 4, unpacked)) {
            error = "gzip-Daten konnten nicht entpackt werden.";
            return false;
        }
        bytes = std::move(unpacked);
    } else if (!compression.empty() && compression != "none") {
        error = "Kompression '" + compression + "' wird noch nicht unterstützt (bitte CSV oder zlib).";
        return false;
    }
    if (bytes.size() < static_cast<std::size_t>(width) * height * 4) {
        error = "Base64-Kacheldaten sind zu kurz.";
        return false;
    }
    for (std::size_t i = 0; i < tiles.size(); ++i) {
        const u8 *p = bytes.data() + i * 4;
        tiles[i] = (static_cast<u32>(p[0]) | (static_cast<u32>(p[1]) << 8) |
                    (static_cast<u32>(p[2]) << 16) | (static_cast<u32>(p[3]) << 24)) &
                   0x1FFFFFFFu;
    }
    return true;
}

std::string JsonString(const Json *value, std::string_view fallback) {
    return value != nullptr && value->isString() ? value->string() : std::string(fallback);
}

std::string PropertyValueToString(const Json &property) {
    const Json *value = property.find("value");
    if (value == nullptr) return {};
    if (value->isString()) return value->string();
    if (value->isNumber()) {
        const double number = value->number();
        if (number == static_cast<int>(number)) return std::to_string(static_cast<int>(number));
        return std::to_string(number);
    }
    if (value->kind() == Json::Type::Bool) return value->boolValue() ? "true" : "false";
    return {};
}

void AddPropertyDef(Project &project, const std::string &defName, const std::string &name,
                    const std::string &type) {
    EnsureEntityDef(project, defName);
    EntityDef *def = FindEntityDef(project, defName);
    if (def == nullptr) return;
    for (const auto &attribute : def->attributes) {
        if (attribute.name == name) return;
    }
    AttributeDef attribute;
    attribute.name = name;
    if (type == "bool" || type == "int" || type == "float" || type == "color" ||
        type == "file" || type == "string") {
        attribute.type = type == "file" ? "asset" : type;
    } else {
        attribute.type = "string";
    }
    def->attributes.push_back(std::move(attribute));
}

void ReadTmjProperties(const Json *properties, std::map<std::string, std::string> &attrs,
                       Project &project, const std::string &defName) {
    if (properties == nullptr || !properties->isArray()) return;
    for (int i = 0; i < properties->size(); ++i) {
        const Json &property = properties->at(i);
        const std::string name = JsonString(property.find("name"));
        if (name.empty()) continue;
        attrs[name] = PropertyValueToString(property);
        AddPropertyDef(project, defName, name, JsonString(property.find("type"), "string"));
    }
}

TileSet ReadTmjTileSet(const Json &json, int firstGid, const fs::path &baseDir,
                       const Project &project) {
    TileSet tileset;
    if (const Json *tiles = json.find("tiles"); tiles != nullptr && tiles->isArray()) {
        for (int i = 0; i < tiles->size(); ++i) {
            const Json &tile = tiles->at(i);
            const Json *animationJson = tile.find("animation");
            if (animationJson == nullptr || !animationJson->isArray()) continue;
            TileAnimation animation;
            for (int f = 0; f < animationJson->size(); ++f) {
                const Json &frame = animationJson->at(f);
                TileAnimationFrame value;
                value.tileId = frame.find("tileid") != nullptr ? frame.find("tileid")->intValue() : 0;
                value.durationMs =
                    frame.find("duration") != nullptr ? frame.find("duration")->intValue(100) : 100;
                animation.frames.push_back(value);
            }
            if (!animation.frames.empty()) {
                tileset.animations[tile.find("id") != nullptr ? tile.find("id")->intValue() : 0] =
                    std::move(animation);
            }
        }
    }
    tileset.name = JsonString(json.find("name"), "tileset");
    tileset.firstGid = firstGid;
    tileset.tileW = json.find("tilewidth") != nullptr ? json.find("tilewidth")->intValue(32) : 32;
    tileset.tileH = json.find("tileheight") != nullptr ? json.find("tileheight")->intValue(32) : 32;
    tileset.columns = json.find("columns") != nullptr ? json.find("columns")->intValue(32) : 0;
    tileset.tileCount = json.find("tilecount") != nullptr ? json.find("tilecount")->intValue(0) : 0;
    tileset.margin = json.find("margin") != nullptr ? json.find("margin")->intValue(0) : 0;
    tileset.spacing = json.find("spacing") != nullptr ? json.find("spacing")->intValue(0) : 0;
    const std::string image = JsonString(json.find("image"));
    if (!image.empty()) {
        tileset.image = MakeProjectRelative(project, baseDir / PathFromUtf8(image));
    }
    if (const Json *wangsets = json.find("wangsets"); wangsets != nullptr && wangsets->isArray()) {
        tileset.wangsets = JsonText(*wangsets);
    }
    return tileset;
}

bool ImportTsjTileSet(const fs::path &file, int firstGid, const Project &project, TileSet &out,
                      std::string &error) {
    const std::string text = ReadText(file);
    if (text.empty()) {
        error = "Tileset-Datei ist leer: " + PathToUtf8(file);
        return false;
    }
    const Json json = Json::Parse(text);
    if (!json.isValid()) {
        error = "Tileset ist kein gültiges JSON: " + json.error();
        return false;
    }
    out = ReadTmjTileSet(json, firstGid, file.parent_path(), project);
    if (out.name.empty()) out.name = FileStem(file);
    return true;
}

bool ImportTsxTileSet(const fs::path &file, int firstGid, const Project &project, TileSet &out,
                      std::string &error) {
    tinyxml2::XMLDocument document;
    if (document.LoadFile(PathToUtf8(file).c_str()) != tinyxml2::XML_SUCCESS) {
        error = "TSX konnte nicht gelesen werden: " + PathToUtf8(file);
        return false;
    }
    const tinyxml2::XMLElement *root = document.FirstChildElement("tileset");
    if (root == nullptr) {
        error = "TSX enthält kein <tileset>.";
        return false;
    }
    out = {};
    out.name = root->Attribute("name") ? root->Attribute("name") : FileStem(file);
    out.firstGid = firstGid;
    out.tileW = ParseInt(root->Attribute("tilewidth"), 32);
    out.tileH = ParseInt(root->Attribute("tileheight"), 32);
    out.columns = ParseInt(root->Attribute("columns"), 0);
    out.tileCount = ParseInt(root->Attribute("tilecount"), 0);
    out.margin = ParseInt(root->Attribute("margin"), 0);
    out.spacing = ParseInt(root->Attribute("spacing"), 0);
    if (const tinyxml2::XMLElement *image = root->FirstChildElement("image")) {
        if (image->Attribute("source") != nullptr) {
            out.image = MakeProjectRelative(project,
                                            file.parent_path() / PathFromUtf8(image->Attribute("source")));
        }
    }
    out.wangsets = WangSetsFromXml(root->FirstChildElement("wangsets"));
    for (const tinyxml2::XMLElement *tile = root->FirstChildElement("tile"); tile != nullptr;
         tile = tile->NextSiblingElement("tile")) {
        const tinyxml2::XMLElement *animation = tile->FirstChildElement("animation");
        if (animation == nullptr) continue;
        TileAnimation value;
        for (const tinyxml2::XMLElement *frame = animation->FirstChildElement("frame");
             frame != nullptr; frame = frame->NextSiblingElement("frame")) {
            TileAnimationFrame entry;
            entry.tileId = ParseInt(frame->Attribute("tileid"), 0);
            entry.durationMs = ParseInt(frame->Attribute("duration"), 100);
            value.frames.push_back(entry);
        }
        if (!value.frames.empty()) out.animations[ParseInt(tile->Attribute("id"), 0)] = std::move(value);
    }
    return true;
}

void WriteJsonValue(JsonWriter &writer, const Json &value) {
    switch (value.kind()) {
    case Json::Type::Null: writer.null(); break;
    case Json::Type::Bool: writer.value(value.boolValue()); break;
    case Json::Type::Number: writer.value(value.number()); break;
    case Json::Type::String: writer.value(value.string()); break;
    case Json::Type::Array:
        writer.beginArray();
        for (int i = 0; i < value.size(); ++i) WriteJsonValue(writer, value.at(i));
        writer.endArray();
        break;
    case Json::Type::Object:
        writer.beginObject();
        for (int i = 0; i < value.size(); ++i) {
            const std::string &key = value.keyAt(i);
            writer.key(key);
            if (const Json *child = value.find(key); child != nullptr) WriteJsonValue(writer, *child);
            else writer.null();
        }
        writer.endObject();
        break;
    }
}

std::string JsonText(const Json &value) {
    JsonWriter writer;
    WriteJsonValue(writer, value);
    return writer.take();
}

std::string WangSetsFromXml(const tinyxml2::XMLElement *wangsets) {
    if (wangsets == nullptr) return {};
    JsonWriter writer;
    writer.beginArray();
    for (const tinyxml2::XMLElement *set = wangsets->FirstChildElement("wangset");
         set != nullptr; set = set->NextSiblingElement("wangset")) {
        writer.beginObject();
        writer.key("name"); writer.value(set->Attribute("name") ? set->Attribute("name") : "");
        writer.key("type"); writer.value(set->Attribute("type") ? set->Attribute("type") : "corner");
        writer.key("tile"); writer.value(ParseInt(set->Attribute("tile"), -1));
        writer.key("colors");
        writer.beginArray();
        for (const tinyxml2::XMLElement *color = set->FirstChildElement("wangcolor");
             color != nullptr; color = color->NextSiblingElement("wangcolor")) {
            writer.beginObject();
            writer.key("name"); writer.value(color->Attribute("name") ? color->Attribute("name") : "");
            writer.key("color"); writer.value(color->Attribute("color") ? color->Attribute("color") : "#ff0000");
            writer.key("tile"); writer.value(ParseInt(color->Attribute("tile"), -1));
            writer.key("probability"); writer.value(ParseFloat(color->Attribute("probability"), 1.0f));
            writer.endObject();
        }
        writer.endArray();
        writer.key("wangtiles");
        writer.beginArray();
        for (const tinyxml2::XMLElement *tile = set->FirstChildElement("wangtile");
             tile != nullptr; tile = tile->NextSiblingElement("wangtile")) {
            writer.beginObject();
            writer.key("tileid"); writer.value(ParseInt(tile->Attribute("tileid"), 0));
            writer.key("wangid");
            writer.beginArray();
            const std::string_view ids = tile->Attribute("wangid") ? tile->Attribute("wangid") : "";
            std::size_t start = 0;
            while (start <= ids.size()) {
                const std::size_t comma = ids.find(',', start);
                const std::string_view part =
                    comma == std::string_view::npos ? ids.substr(start) : ids.substr(start, comma - start);
                if (!part.empty()) writer.value(ParseInt(std::string(part).c_str(), 0));
                if (comma == std::string_view::npos) break;
                start = comma + 1;
            }
            writer.endArray();
            writer.endObject();
        }
        writer.endArray();
        writer.endObject();
    }
    writer.endArray();
    return writer.take();
}

void WangSetsToXml(tinyxml2::XMLDocument &document, tinyxml2::XMLElement *tileset,
                   const std::string &jsonText) {
    if (jsonText.empty()) return;
    const Json json = Json::Parse(jsonText);
    if (!json.isValid() || !json.isArray()) return;
    auto *wangsets = document.NewElement("wangsets");
    for (int i = 0; i < json.size(); ++i) {
        const Json &set = json.at(i);
        auto *setElement = document.NewElement("wangset");
        setElement->SetAttribute("name", JsonString(set.find("name")).c_str());
        setElement->SetAttribute("type", JsonString(set.find("type"), "corner").c_str());
        setElement->SetAttribute("tile", set.find("tile") ? set.find("tile")->intValue(-1) : -1);
        if (const Json *colors = set.find("colors"); colors != nullptr && colors->isArray()) {
            for (int c = 0; c < colors->size(); ++c) {
                const Json &color = colors->at(c);
                auto *colorElement = document.NewElement("wangcolor");
                colorElement->SetAttribute("name", JsonString(color.find("name")).c_str());
                colorElement->SetAttribute("color", JsonString(color.find("color"), "#ff0000").c_str());
                colorElement->SetAttribute("tile", color.find("tile") ? color.find("tile")->intValue(-1) : -1);
                colorElement->SetAttribute(
                    "probability",
                    color.find("probability") ? color.find("probability")->floatValue(1.0f) : 1.0f);
                setElement->InsertEndChild(colorElement);
            }
        }
        if (const Json *tiles = set.find("wangtiles"); tiles != nullptr && tiles->isArray()) {
            for (int t = 0; t < tiles->size(); ++t) {
                const Json &tile = tiles->at(t);
                auto *tileElement = document.NewElement("wangtile");
                tileElement->SetAttribute("tileid", tile.find("tileid") ? tile.find("tileid")->intValue(0) : 0);
                std::string ids;
                if (const Json *wangid = tile.find("wangid"); wangid != nullptr && wangid->isArray()) {
                    for (int w = 0; w < wangid->size(); ++w) {
                        if (!ids.empty()) ids += ',';
                        ids += std::to_string(wangid->at(w).intValue());
                    }
                }
                tileElement->SetAttribute("wangid", ids.c_str());
                setElement->InsertEndChild(tileElement);
            }
        }
        wangsets->InsertEndChild(setElement);
    }
    tileset->InsertEndChild(wangsets);
}

bool LoadTemplateXml(const fs::path &file, Entity &entity, Project &project) {
    tinyxml2::XMLDocument document;
    if (document.LoadFile(PathToUtf8(file).c_str()) != tinyxml2::XML_SUCCESS) return false;
    const tinyxml2::XMLElement *root = document.FirstChildElement("template");
    if (root == nullptr) return false;
    const tinyxml2::XMLElement *object = root->FirstChildElement("object");
    if (object == nullptr) return false;
    entity = {};
    entity.def = object->Attribute("type") ? object->Attribute("type")
                 : object->Attribute("class") ? object->Attribute("class")
                                               : "";
    entity.w = ParseFloat(object->Attribute("width"), 0.0f);
    entity.h = ParseFloat(object->Attribute("height"), 0.0f);
    entity.rotation = ParseFloat(object->Attribute("rotation"), 0.0f);
    const int gid = ParseInt(object->Attribute("gid"), 0);
    if (gid != 0) entity.attrs["gid"] = std::to_string(gid & 0x1FFFFFFF);
    if (const tinyxml2::XMLElement *properties = object->FirstChildElement("properties")) {
        for (const tinyxml2::XMLElement *property = properties->FirstChildElement("property");
             property != nullptr; property = property->NextSiblingElement("property")) {
            const char *name = property->Attribute("name");
            const char *value = property->Attribute("value");
            if (name == nullptr) continue;
            entity.attrs[name] = value != nullptr ? value : "";
            AddPropertyDef(project, entity.def.empty() ? "Objekt" : entity.def, name,
                           property->Attribute("type") ? property->Attribute("type") : "string");
        }
    }
    return true;
}

bool LoadTemplateJson(const fs::path &file, Entity &entity, Project &project) {
    const std::string text = ReadText(file);
    if (text.empty()) return false;
    const Json json = Json::Parse(text);
    if (!json.isValid()) return false;
    const Json *object = json.find("object");
    if (object == nullptr) object = &json;
    entity = {};
    entity.def = JsonString(object->find("type"));
    if (entity.def.empty()) entity.def = JsonString(object->find("class"));
    entity.w = object->find("width") ? object->find("width")->floatValue(0.0f) : 0.0f;
    entity.h = object->find("height") ? object->find("height")->floatValue(0.0f) : 0.0f;
    entity.rotation = object->find("rotation") ? object->find("rotation")->floatValue(0.0f) : 0.0f;
    if (object->find("gid") != nullptr) {
        entity.attrs["gid"] = std::to_string(object->find("gid")->intValue() & 0x1FFFFFFF);
    }
    ReadTmjProperties(object->find("properties"), entity.attrs, project,
                      entity.def.empty() ? "Objekt" : entity.def);
    return true;
}

bool LoadTemplate(const fs::path &file, Entity &entity, Project &project) {
    const std::string extension = PathToUtf8(file.extension());
    if (extension == ".tx") return LoadTemplateXml(file, entity, project);
    if (extension == ".tj" || extension == ".json") return LoadTemplateJson(file, entity, project);
    return false;
}

bool ImportTmj(const fs::path &file, Level &level, Project &project, std::string &error) {
    const std::string text = ReadText(file);
    if (text.empty()) {
        error = "TMJ ist leer: " + PathToUtf8(file);
        return false;
    }
    const Json map = Json::Parse(text);
    if (!map.isValid()) {
        error = "TMJ ist kein gültiges JSON: " + map.error();
        return false;
    }
    level = {};
    level.name = FileStem(file);
    level.width = std::max(map.find("width") ? map.find("width")->intValue(40) : 40, 1);
    level.height = std::max(map.find("height") ? map.find("height")->intValue(25) : 25, 1);
    level.tileW = std::max(map.find("tilewidth") ? map.find("tilewidth")->intValue(32) : 32, 1);
    level.tileH = std::max(map.find("tileheight") ? map.find("tileheight")->intValue(32) : 32, 1);

    if (const Json *tilesets = map.find("tilesets"); tilesets != nullptr && tilesets->isArray()) {
        for (int i = 0; i < tilesets->size(); ++i) {
            const Json &entry = tilesets->at(i);
            const int firstGid = entry.find("firstgid") ? entry.find("firstgid")->intValue(1) : 1;
            TileSet tileset;
            const std::string source = JsonString(entry.find("source"));
            if (!source.empty()) {
                const fs::path sourcePath = file.parent_path() / PathFromUtf8(source);
                const std::string extension = PathToUtf8(sourcePath.extension());
                if (extension == ".tsx") {
                    if (!ImportTsxTileSet(sourcePath, firstGid, project, tileset, error)) return false;
                } else {
                    if (!ImportTsjTileSet(sourcePath, firstGid, project, tileset, error)) return false;
                }
            } else {
                tileset = ReadTmjTileSet(entry, firstGid, file.parent_path(), project);
            }
            level.tilesets.push_back(tileset);
            bool known = false;
            for (const auto &existing : project.tilesets) {
                if (existing.name == tileset.name) known = true;
            }
            if (!known) project.tilesets.push_back(tileset);
        }
    }

    if (const Json *layers = map.find("layers"); layers != nullptr && layers->isArray()) {
        for (int i = 0; i < layers->size(); ++i) {
            const Json &entry = layers->at(i);
            const std::string type = JsonString(entry.find("type"));
            if (type == "tilelayer") {
                TileLayer layer;
                layer.name = JsonString(entry.find("name"), "Kacheln");
                layer.visible = entry.find("visible") ? entry.find("visible")->boolValue(true) : true;
                layer.opacity = entry.find("opacity") ? entry.find("opacity")->floatValue(1.0f) : 1.0f;
                if (!DecodeLayerData(entry, level.width, level.height, layer.tiles, error)) return false;
                level.tileLayers.push_back(std::move(layer));
            } else if (type == "objectgroup") {
                ObjectLayer layer;
                layer.name = JsonString(entry.find("name"), "Objekte");
                layer.visible = entry.find("visible") ? entry.find("visible")->boolValue(true) : true;
                if (const Json *objects = entry.find("objects"); objects != nullptr && objects->isArray()) {
                    for (int o = 0; o < objects->size(); ++o) {
                        const Json &object = objects->at(o);
                        Entity entity;
                        bool hasTemplate = false;
                        if (const Json *templateJson = object.find("template");
                            templateJson != nullptr && templateJson->isString() &&
                            !templateJson->string().empty()) {
                            const fs::path templateFile =
                                file.parent_path() / PathFromUtf8(templateJson->string());
                            Entity base;
                            if (LoadTemplate(templateFile, base, project)) {
                                entity = std::move(base);
                                entity.templatePath = MakeProjectRelative(project, templateFile);
                                hasTemplate = true;
                            }
                        }
                        entity.id = object.find("id") ? object.find("id")->intValue(level.nextEntityId++)
                                                       : level.nextEntityId++;
                        std::string typeName = JsonString(object.find("type"));
                        if (typeName.empty()) typeName = JsonString(object.find("class"));
                        if (typeName.empty()) typeName = entity.def;
                        const int gid = object.find("gid") ? object.find("gid")->intValue(0) : 0;
                        if (typeName.empty()) typeName = gid != 0 ? "Kachelobjekt" : "Objekt";
                        entity.def = typeName;
                        EnsureEntityDef(project, typeName);
                        entity.x = object.find("x") ? object.find("x")->floatValue(0.0f) : 0.0f;
                        entity.y = object.find("y") ? object.find("y")->floatValue(0.0f) : 0.0f;
                        if (object.find("width") != nullptr) {
                            entity.w = object.find("width")->floatValue(hasTemplate ? entity.w : 32.0f);
                        } else if (!hasTemplate) {
                            entity.w = 32.0f;
                        }
                        if (object.find("height") != nullptr) {
                            entity.h = object.find("height")->floatValue(hasTemplate ? entity.h : 32.0f);
                        } else if (!hasTemplate) {
                            entity.h = 32.0f;
                        }
                        if (object.find("rotation") != nullptr) {
                            entity.rotation = object.find("rotation")->floatValue(entity.rotation);
                        }
                        if (const Json *name = object.find("name"); name != nullptr && name->isString() &&
                                                                       !name->string().empty()) {
                            entity.attrs["Name"] = name->string();
                        }
                        if (gid != 0) entity.attrs["gid"] = std::to_string(gid & 0x1FFFFFFF);
                        ReadTmjProperties(object.find("properties"), entity.attrs, project, typeName);
                        layer.entities.push_back(std::move(entity));
                    }
                }
                level.objectLayers.push_back(std::move(layer));
            }
        }
    }
    if (level.tileLayers.empty() && level.objectLayers.empty()) AddDefaultLayers(level);
    return true;
}

bool ImportTmx(const fs::path &file, Level &level, Project &project, std::string &error) {
    tinyxml2::XMLDocument document;
    if (document.LoadFile(PathToUtf8(file).c_str()) != tinyxml2::XML_SUCCESS) {
        error = "TMX konnte nicht gelesen werden: " + PathToUtf8(file);
        return false;
    }
    const tinyxml2::XMLElement *map = document.FirstChildElement("map");
    if (map == nullptr) {
        error = "TMX enthält kein <map>.";
        return false;
    }
    level = {};
    level.name = FileStem(file);
    level.width = std::max(ParseInt(map->Attribute("width"), 40), 1);
    level.height = std::max(ParseInt(map->Attribute("height"), 25), 1);
    level.tileW = std::max(ParseInt(map->Attribute("tilewidth"), 32), 1);
    level.tileH = std::max(ParseInt(map->Attribute("tileheight"), 32), 1);

    for (const tinyxml2::XMLElement *tilesetElement = map->FirstChildElement("tileset");
         tilesetElement != nullptr; tilesetElement = tilesetElement->NextSiblingElement("tileset")) {
        const int firstGid = ParseInt(tilesetElement->Attribute("firstgid"), 1);
        TileSet tileset;
        if (const char *source = tilesetElement->Attribute("source")) {
            const fs::path sourcePath = file.parent_path() / PathFromUtf8(source);
            if (PathToUtf8(sourcePath.extension()) == ".tsx") {
                if (!ImportTsxTileSet(sourcePath, firstGid, project, tileset, error)) return false;
            } else {
                if (!ImportTsjTileSet(sourcePath, firstGid, project, tileset, error)) return false;
            }
        } else {
            tileset.name = tilesetElement->Attribute("name") ? tilesetElement->Attribute("name") : "tileset";
            tileset.firstGid = firstGid;
            tileset.tileW = ParseInt(tilesetElement->Attribute("tilewidth"), 32);
            tileset.tileH = ParseInt(tilesetElement->Attribute("tileheight"), 32);
            tileset.columns = ParseInt(tilesetElement->Attribute("columns"), 0);
            tileset.tileCount = ParseInt(tilesetElement->Attribute("tilecount"), 0);
            tileset.margin = ParseInt(tilesetElement->Attribute("margin"), 0);
            tileset.spacing = ParseInt(tilesetElement->Attribute("spacing"), 0);
            if (const tinyxml2::XMLElement *image = tilesetElement->FirstChildElement("image")) {
                if (image->Attribute("source") != nullptr) {
                    tileset.image = MakeProjectRelative(
                        project, file.parent_path() / PathFromUtf8(image->Attribute("source")));
                }
            }
            tileset.wangsets = WangSetsFromXml(tilesetElement->FirstChildElement("wangsets"));
            for (const tinyxml2::XMLElement *tile = tilesetElement->FirstChildElement("tile");
                 tile != nullptr; tile = tile->NextSiblingElement("tile")) {
                const tinyxml2::XMLElement *animation = tile->FirstChildElement("animation");
                if (animation == nullptr) continue;
                TileAnimation value;
                for (const tinyxml2::XMLElement *frame = animation->FirstChildElement("frame");
                     frame != nullptr; frame = frame->NextSiblingElement("frame")) {
                    value.frames.push_back(TileAnimationFrame{
                        ParseInt(frame->Attribute("tileid"), 0),
                        ParseInt(frame->Attribute("duration"), 100)});
                }
                if (!value.frames.empty()) {
                    tileset.animations[ParseInt(tile->Attribute("id"), 0)] = std::move(value);
                }
            }
        }
        level.tilesets.push_back(tileset);
        bool known = false;
        for (const auto &existing : project.tilesets) {
            if (existing.name == tileset.name) known = true;
        }
        if (!known) project.tilesets.push_back(tileset);
    }

    for (const tinyxml2::XMLElement *layerElement = map->FirstChildElement("layer");
         layerElement != nullptr; layerElement = layerElement->NextSiblingElement("layer")) {
        TileLayer layer;
        layer.name = layerElement->Attribute("name") ? layerElement->Attribute("name") : "Kacheln";
        layer.visible = ParseBool(layerElement->Attribute("visible"), true);
        layer.opacity = ParseFloat(layerElement->Attribute("opacity"), 1.0f);
        const tinyxml2::XMLElement *data = layerElement->FirstChildElement("data");
        if (data == nullptr) {
            error = "TMX-Kachelebene ohne <data>.";
            return false;
        }
        layer.tiles.assign(static_cast<std::size_t>(level.width) * level.height, 0);
        const char *encoding = data->Attribute("encoding");
        const char *compression = data->Attribute("compression");
        if (encoding == nullptr) {
            int index = 0;
            for (const tinyxml2::XMLElement *tile = data->FirstChildElement("tile");
                 tile != nullptr && index < static_cast<int>(layer.tiles.size());
                 tile = tile->NextSiblingElement("tile"), ++index) {
                layer.tiles[static_cast<std::size_t>(index)] =
                    static_cast<u32>(ParseInt(tile->Attribute("gid"), 0)) & 0x1FFFFFFFu;
            }
        } else if (std::string_view(encoding) == "csv") {
            const char *text = data->GetText();
            if (text == nullptr) {
                error = "CSV-Kacheldaten fehlen.";
                return false;
            }
            std::string_view remaining(text);
            std::size_t index = 0;
            while (index < layer.tiles.size() && !remaining.empty()) {
                const std::size_t comma = remaining.find(',');
                const std::string_view part =
                    comma == std::string_view::npos ? remaining : remaining.substr(0, comma);
                layer.tiles[index++] = static_cast<u32>(std::strtol(Trim(part).c_str(), nullptr, 10)) &
                                       0x1FFFFFFFu;
                if (comma == std::string_view::npos) break;
                remaining.remove_prefix(comma + 1);
            }
        } else if (std::string_view(encoding) == "base64") {
            const char *text = data->GetText();
            if (text == nullptr) {
                error = "Base64-Kacheldaten fehlen.";
                return false;
            }
            std::vector<u8> bytes = DecodeBase64(text);
            const std::size_t expected = layer.tiles.size() * 4;
            if (compression != nullptr && std::string_view(compression) == "zlib") {
                std::vector<u8> unpacked;
                if (!DecompressZlib(bytes, expected, unpacked)) {
                    error = "zlib-Daten konnten nicht entpackt werden.";
                    return false;
                }
                bytes = std::move(unpacked);
            } else if (compression != nullptr && std::string_view(compression) == "gzip") {
                std::vector<u8> unpacked;
                if (!DecompressGzip(bytes, expected, unpacked)) {
                    error = "gzip-Daten konnten nicht entpackt werden.";
                    return false;
                }
                bytes = std::move(unpacked);
            } else if (compression != nullptr && std::string_view(compression) != "none" &&
                       *compression != '\0') {
                error = std::string("Kompression '") + compression +
                        "' wird noch nicht unterstützt (bitte CSV oder zlib).";
                return false;
            }
            if (bytes.size() < expected) {
                error = "Base64-Kacheldaten sind zu kurz.";
                return false;
            }
            for (std::size_t i = 0; i < layer.tiles.size(); ++i) {
                const u8 *p = bytes.data() + i * 4;
                layer.tiles[i] = (static_cast<u32>(p[0]) | (static_cast<u32>(p[1]) << 8) |
                                  (static_cast<u32>(p[2]) << 16) | (static_cast<u32>(p[3]) << 24)) &
                                 0x1FFFFFFFu;
            }
        } else {
            error = "Unbekannte TMX-Kodierung: " + std::string(encoding);
            return false;
        }
        level.tileLayers.push_back(std::move(layer));
    }

    for (const tinyxml2::XMLElement *groupElement = map->FirstChildElement("objectgroup");
         groupElement != nullptr; groupElement = groupElement->NextSiblingElement("objectgroup")) {
        ObjectLayer layer;
        layer.name = groupElement->Attribute("name") ? groupElement->Attribute("name") : "Objekte";
        layer.visible = ParseBool(groupElement->Attribute("visible"), true);
        for (const tinyxml2::XMLElement *object = groupElement->FirstChildElement("object");
             object != nullptr; object = object->NextSiblingElement("object")) {
            Entity entity;
            bool hasTemplate = false;
            if (const char *templateAttribute = object->Attribute("template");
                templateAttribute != nullptr && *templateAttribute != '\0') {
                const fs::path templateFile =
                    file.parent_path() / PathFromUtf8(templateAttribute);
                Entity base;
                if (LoadTemplate(templateFile, base, project)) {
                    entity = std::move(base);
                    entity.templatePath = MakeProjectRelative(project, templateFile);
                    hasTemplate = true;
                }
            }
            entity.id = NewEntityId(level);
            std::string typeName = object->Attribute("type") ? object->Attribute("type")
                                  : object->Attribute("class") ? object->Attribute("class")
                                                               : "";
            if (typeName.empty()) typeName = entity.def;
            const int gid = object->Attribute("gid") != nullptr
                                ? ParseInt(object->Attribute("gid"), 0)
                                : (entity.attrs.count("gid") != 0
                                       ? std::atoi(entity.attrs.at("gid").c_str())
                                       : 0);
            if (typeName.empty()) typeName = gid != 0 ? "Kachelobjekt" : "Objekt";
            entity.def = typeName;
            EnsureEntityDef(project, typeName);
            entity.x = ParseFloat(object->Attribute("x"), 0.0f);
            entity.y = ParseFloat(object->Attribute("y"), 0.0f);
            entity.w = object->Attribute("width") != nullptr
                           ? ParseFloat(object->Attribute("width"), entity.w)
                           : (hasTemplate ? entity.w : 32.0f);
            entity.h = object->Attribute("height") != nullptr
                           ? ParseFloat(object->Attribute("height"), entity.h)
                           : (hasTemplate ? entity.h : 32.0f);
            if (object->Attribute("rotation") != nullptr) {
                entity.rotation = ParseFloat(object->Attribute("rotation"), entity.rotation);
            }
            if (const char *name = object->Attribute("name"); name != nullptr && *name != '\0') {
                entity.attrs["Name"] = name;
            }
            if (gid != 0) entity.attrs["gid"] = std::to_string(gid & 0x1FFFFFFF);
            if (const tinyxml2::XMLElement *properties = object->FirstChildElement("properties")) {
                for (const tinyxml2::XMLElement *property = properties->FirstChildElement("property");
                     property != nullptr; property = property->NextSiblingElement("property")) {
                    const char *name = property->Attribute("name");
                    const char *value = property->Attribute("value");
                    const char *type = property->Attribute("type");
                    if (name == nullptr) continue;
                    entity.attrs[name] = value != nullptr ? value : "";
                    AddPropertyDef(project, typeName, name, type != nullptr ? type : "string");
                }
            }
            layer.entities.push_back(std::move(entity));
        }
        level.objectLayers.push_back(std::move(layer));
    }
    if (level.tileLayers.empty() && level.objectLayers.empty()) AddDefaultLayers(level);
    (void)error;
    return true;
}

std::string TiledImagePath(const Project &project, const TileSet &tileset,
                           const fs::path &outputFile) {
    const fs::path image = PathFromUtf8(tileset.image);
    if (project.root.empty() || image.is_absolute()) return tileset.image;
    std::error_code ec;
    const fs::path relative =
        fs::relative(project.root / image, outputFile.parent_path(), ec);
    if (!ec && !relative.empty()) return PathToUtf8(relative);
    return tileset.image;
}

void WriteTmjTileSet(JsonWriter &writer, const TileSet &tileset, const std::string &imagePath) {
    writer.beginObject();
    writer.key("firstgid"); writer.value(tileset.firstGid);
    writer.key("name"); writer.value(tileset.name);
    writer.key("image"); writer.value(imagePath);
    writer.key("tilewidth"); writer.value(tileset.tileW);
    writer.key("tileheight"); writer.value(tileset.tileH);
    writer.key("columns"); writer.value(tileset.columns);
    writer.key("tilecount"); writer.value(tileset.tileCount);
    writer.key("margin"); writer.value(tileset.margin);
    writer.key("spacing"); writer.value(tileset.spacing);
    if (!tileset.wangsets.empty()) {
        writer.key("wangsets");
        writer.raw(tileset.wangsets);
    }
    if (!tileset.animations.empty()) {
        writer.key("tiles");
        writer.beginArray();
        for (const auto &[id, animation] : tileset.animations) {
            writer.beginObject();
            writer.key("id"); writer.value(id);
            writer.key("animation");
            writer.beginArray();
            for (const auto &frame : animation.frames) {
                writer.beginObject();
                writer.key("tileid"); writer.value(frame.tileId);
                writer.key("duration"); writer.value(frame.durationMs);
                writer.endObject();
            }
            writer.endArray();
            writer.endObject();
        }
        writer.endArray();
    }
    writer.endObject();
}

void WriteTmjLayer(JsonWriter &writer, const TileLayer &layer) {
    writer.beginObject();
    writer.key("type"); writer.value("tilelayer");
    writer.key("name"); writer.value(layer.name);
    writer.key("visible"); writer.value(layer.visible);
    writer.key("opacity"); writer.value(static_cast<double>(layer.opacity));
    writer.key("data");
    writer.beginArray();
    for (const u32 tile : layer.tiles) writer.value(static_cast<int>(tile));
    writer.endArray();
    writer.endObject();
}

std::string TiledTemplatePath(const Project &project, const std::string &stored,
                              const fs::path &outputFile) {
    const fs::path path = PathFromUtf8(stored);
    if (project.root.empty() || path.is_absolute()) return stored;
    std::error_code ec;
    const fs::path relative = fs::relative(project.root / path, outputFile.parent_path(), ec);
    if (!ec && !relative.empty()) return PathToUtf8(relative);
    return stored;
}

void WriteTmjObjectGroup(JsonWriter &writer, const ObjectLayer &layer, const Project &project,
                         const fs::path &outputFile) {
    writer.beginObject();
    writer.key("type"); writer.value("objectgroup");
    writer.key("name"); writer.value(layer.name);
    writer.key("visible"); writer.value(layer.visible);
    writer.key("objects");
    writer.beginArray();
    for (const auto &entity : layer.entities) {
        writer.beginObject();
        writer.key("id"); writer.value(entity.id);
        writer.key("name"); writer.value(entity.attrs.count("Name") != 0 ? entity.attrs.at("Name")
                                                                         : entity.def);
        writer.key("type"); writer.value(entity.def);
        writer.key("x"); writer.value(static_cast<double>(entity.x));
        writer.key("y"); writer.value(static_cast<double>(entity.y));
        writer.key("width"); writer.value(static_cast<double>(entity.w));
        writer.key("height"); writer.value(static_cast<double>(entity.h));
        if (entity.rotation != 0.0f) {
            writer.key("rotation"); writer.value(static_cast<double>(entity.rotation));
        }
        if (entity.attrs.count("gid") != 0) {
            writer.key("gid"); writer.value(std::atoi(entity.attrs.at("gid").c_str()));
        }
        if (!entity.templatePath.empty()) {
            writer.key("template");
            writer.value(TiledTemplatePath(project, entity.templatePath, outputFile));
        }
        writer.key("properties");
        writer.beginArray();
        for (const auto &[name, value] : entity.attrs) {
            if (name == "gid") continue;
            writer.beginObject();
            writer.key("name"); writer.value(name);
            writer.key("type"); writer.value("string");
            writer.key("value"); writer.value(value);
            writer.endObject();
        }
        writer.endArray();
        writer.endObject();
    }
    writer.endArray();
    writer.endObject();
}

std::string ExportTmjText(const Level &level, const Project &project, const fs::path &outputFile) {
    JsonWriter writer;
    writer.beginObject();
    writer.key("type"); writer.value("map");
    writer.key("version"); writer.value("1.10");
    writer.key("tiledversion"); writer.value("1.10.2");
    writer.key("orientation"); writer.value("orthogonal");
    writer.key("renderorder"); writer.value("right-down");
    writer.key("infinite"); writer.value(false);
    writer.key("width"); writer.value(level.width);
    writer.key("height"); writer.value(level.height);
    writer.key("tilewidth"); writer.value(level.tileW);
    writer.key("tileheight"); writer.value(level.tileH);
    writer.key("nextlayerid"); writer.value(static_cast<int>(level.tileLayers.size() +
                                                              level.objectLayers.size() + 1));
    writer.key("nextobjectid"); writer.value(level.nextEntityId);
    writer.key("tilesets");
    writer.beginArray();
    for (const auto &tileset : level.tilesets) WriteTmjTileSet(writer, tileset, TiledImagePath(project, tileset, outputFile));
    writer.endArray();
    writer.key("layers");
    writer.beginArray();
    for (const auto &layer : level.tileLayers) WriteTmjLayer(writer, layer);
    for (const auto &layer : level.objectLayers) WriteTmjObjectGroup(writer, layer, project, outputFile);
    writer.endArray();
    writer.endObject();
    return writer.take();
}

} // namespace

bool ImportTiled(const fs::path &file, Level &level, Project &project, std::string &error) {
    const std::string extension = PathToUtf8(file.extension());
    if (extension == ".tmj") return ImportTmj(file, level, project, error);
    if (extension == ".tmx") return ImportTmx(file, level, project, error);
    error = "Unbekanntes Tiled-Format: " + extension + " (erwartet .tmj oder .tmx)";
    return false;
}

bool ExportTiled(const fs::path &file, const Level &level, const Project &project,
                 std::string &error) {
    const std::string extension = PathToUtf8(file.extension());
    if (extension == ".tmj") {
        std::error_code ec;
        fs::create_directories(file.parent_path(), ec);
        std::ofstream output(file, std::ios::binary | std::ios::trunc);
        if (!output) {
            error = "TMJ konnte nicht geschrieben werden: " + PathToUtf8(file);
            return false;
        }
        const std::string text = ExportTmjText(level, project, file);
        output.write(text.data(), static_cast<std::streamsize>(text.size()));
        return static_cast<bool>(output);
    }
    if (extension != ".tmx") {
        error = "Unbekanntes Tiled-Format: " + extension + " (erwartet .tmj oder .tmx)";
        return false;
    }

    tinyxml2::XMLDocument document;
    auto *declaration = document.NewDeclaration();
    document.InsertFirstChild(declaration);
    auto *map = document.NewElement("map");
    map->SetAttribute("version", "1.10");
    map->SetAttribute("tiledversion", "1.10.2");
    map->SetAttribute("orientation", "orthogonal");
    map->SetAttribute("renderorder", "right-down");
    map->SetAttribute("width", level.width);
    map->SetAttribute("height", level.height);
    map->SetAttribute("tilewidth", level.tileW);
    map->SetAttribute("tileheight", level.tileH);
    map->SetAttribute("infinite", 0);
    map->SetAttribute("nextlayerid", static_cast<int>(level.tileLayers.size() +
                                                       level.objectLayers.size() + 1));
    map->SetAttribute("nextobjectid", level.nextEntityId);
    document.InsertEndChild(map);

    for (const auto &tileset : level.tilesets) {
        auto *element = document.NewElement("tileset");
        element->SetAttribute("firstgid", tileset.firstGid);
        element->SetAttribute("name", tileset.name.c_str());
        element->SetAttribute("tilewidth", tileset.tileW);
        element->SetAttribute("tileheight", tileset.tileH);
        element->SetAttribute("tilecount", tileset.tileCount);
        element->SetAttribute("columns", tileset.columns);
        element->SetAttribute("margin", tileset.margin);
        element->SetAttribute("spacing", tileset.spacing);
        if (!tileset.image.empty()) {
            auto *image = document.NewElement("image");
            const std::string imagePath = TiledImagePath(project, tileset, file);
            image->SetAttribute("source", imagePath.c_str());
            element->InsertEndChild(image);
        }
        WangSetsToXml(document, element, tileset.wangsets);
        for (const auto &[id, animation] : tileset.animations) {
            auto *tileElement = document.NewElement("tile");
            tileElement->SetAttribute("id", id);
            auto *animationElement = document.NewElement("animation");
            for (const auto &frame : animation.frames) {
                auto *frameElement = document.NewElement("frame");
                frameElement->SetAttribute("tileid", frame.tileId);
                frameElement->SetAttribute("duration", frame.durationMs);
                animationElement->InsertEndChild(frameElement);
            }
            tileElement->InsertEndChild(animationElement);
            element->InsertEndChild(tileElement);
        }
        map->InsertEndChild(element);
    }

    int nextLayerId = 1;
    for (const auto &layer : level.tileLayers) {
        auto *element = document.NewElement("layer");
        element->SetAttribute("id", nextLayerId++);
        element->SetAttribute("name", layer.name.c_str());
        element->SetAttribute("width", level.width);
        element->SetAttribute("height", level.height);
        if (!layer.visible) element->SetAttribute("visible", 0);
        if (layer.opacity != 1.0f) element->SetAttribute("opacity", layer.opacity);
        auto *data = document.NewElement("data");
        data->SetAttribute("encoding", "csv");
        std::string csv;
        csv.reserve(layer.tiles.size() * 4);
        for (std::size_t i = 0; i < layer.tiles.size(); ++i) {
            if (i != 0) csv += ',';
            csv += std::to_string(layer.tiles[i]);
            if (i % 32 == 31) csv += '\n';
        }
        data->SetText(csv.c_str());
        element->InsertEndChild(data);
        map->InsertEndChild(element);
    }

    for (const auto &layer : level.objectLayers) {
        auto *element = document.NewElement("objectgroup");
        element->SetAttribute("id", nextLayerId++);
        element->SetAttribute("name", layer.name.c_str());
        if (!layer.visible) element->SetAttribute("visible", 0);
        for (const auto &entity : layer.entities) {
            auto *object = document.NewElement("object");
            object->SetAttribute("id", entity.id);
            object->SetAttribute("name",
                                 entity.attrs.count("Name") != 0 ? entity.attrs.at("Name").c_str()
                                                                  : entity.def.c_str());
            object->SetAttribute("type", entity.def.c_str());
            object->SetAttribute("x", entity.x);
            object->SetAttribute("y", entity.y);
            object->SetAttribute("width", entity.w);
            object->SetAttribute("height", entity.h);
            if (entity.rotation != 0.0f) object->SetAttribute("rotation", entity.rotation);
            if (entity.attrs.count("gid") != 0) {
                object->SetAttribute("gid", std::atoi(entity.attrs.at("gid").c_str()));
            }
            if (!entity.templatePath.empty()) {
                const std::string templatePath =
                    TiledTemplatePath(project, entity.templatePath, file);
                object->SetAttribute("template", templatePath.c_str());
            }
            if (!entity.attrs.empty()) {
                auto *properties = document.NewElement("properties");
                for (const auto &[name, value] : entity.attrs) {
                    if (name == "gid") continue;
                    auto *property = document.NewElement("property");
                    property->SetAttribute("name", name.c_str());
                    property->SetAttribute("type", "string");
                    property->SetAttribute("value", value.c_str());
                    properties->InsertEndChild(property);
                }
                object->InsertEndChild(properties);
            }
            element->InsertEndChild(object);
        }
        map->InsertEndChild(element);
    }

    std::error_code ec;
    fs::create_directories(file.parent_path(), ec);
    if (document.SaveFile(PathToUtf8(file).c_str()) != tinyxml2::XML_SUCCESS) {
        error = "TMX konnte nicht geschrieben werden: " + PathToUtf8(file);
        return false;
    }
    return true;
}

} // namespace editor
