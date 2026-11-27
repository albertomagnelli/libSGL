#include "JsonUtil.hpp"

#include "JsonWriter.hpp"

namespace editor {
namespace {

void WriteValue(JsonWriter &writer, const sgl::detail::Json &value) {
    switch (value.kind()) {
    case sgl::detail::Json::Type::Null: writer.null(); break;
    case sgl::detail::Json::Type::Bool: writer.value(value.boolValue()); break;
    case sgl::detail::Json::Type::Number: writer.value(value.number()); break;
    case sgl::detail::Json::Type::String: writer.value(value.string()); break;
    case sgl::detail::Json::Type::Array:
        writer.beginArray();
        for (int i = 0; i < value.size(); ++i) WriteValue(writer, value.at(i));
        writer.endArray();
        break;
    case sgl::detail::Json::Type::Object:
        writer.beginObject();
        for (int i = 0; i < value.size(); ++i) {
            const std::string &key = value.keyAt(i);
            writer.key(key);
            if (const auto *child = value.find(key); child != nullptr) WriteValue(writer, *child);
            else writer.null();
        }
        writer.endObject();
        break;
    }
}

} // namespace

std::string JsonToText(const sgl::detail::Json &value) {
    JsonWriter writer;
    WriteValue(writer, value);
    return writer.take();
}

} // namespace editor
