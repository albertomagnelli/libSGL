#include "JsonWriter.hpp"

#include <charconv>
#include <cstdio>

namespace editor {

std::string JsonWriter::escape(std::string_view text) {
    std::string result;
    result.reserve(text.size() + 8);
    for (const char c : text) {
        switch (c) {
        case '"': result += "\\\""; break;
        case '\\': result += "\\\\"; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                char buffer[8];
                std::snprintf(buffer, sizeof(buffer), "\\u%04x",
                              static_cast<unsigned>(static_cast<unsigned char>(c)));
                result += buffer;
            } else {
                result += c;
            }
            break;
        }
    }
    return result;
}

void JsonWriter::prefix() {
    if (!m_scopes.empty()) {
        if (!m_scopes.back().empty) {
            m_out += ',';
        }
        m_scopes.back().empty = false;
    }
}

void JsonWriter::beginObject() {
    if (!m_afterKey) {
        prefix();
    }
    m_afterKey = false;
    m_out += '{';
    m_scopes.push_back(Scope{true, true});
}

void JsonWriter::endObject() {
    if (!m_scopes.empty()) {
        m_scopes.pop_back();
    }
    m_out += '}';
}

void JsonWriter::beginArray() {
    if (!m_afterKey) {
        prefix();
    }
    m_afterKey = false;
    m_out += '[';
    m_scopes.push_back(Scope{false, true});
}

void JsonWriter::endArray() {
    if (!m_scopes.empty()) {
        m_scopes.pop_back();
    }
    m_out += ']';
}

void JsonWriter::key(std::string_view name) {
    prefix();
    m_out += '"';
    m_out += escape(name);
    m_out += "\":";
    m_afterKey = true;
}

void JsonWriter::value(std::string_view text) {
    if (!m_afterKey) {
        prefix();
    }
    m_afterKey = false;
    m_out += '"';
    m_out += escape(text);
    m_out += '"';
}

void JsonWriter::value(double number) {
    if (!m_afterKey) {
        prefix();
    }
    m_afterKey = false;
    char buffer[64];
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), number);
    if (result.ec == std::errc{}) {
        m_out.append(buffer, result.ptr);
    } else {
        m_out += '0';
    }
}

void JsonWriter::value(bool flag) {
    if (!m_afterKey) {
        prefix();
    }
    m_afterKey = false;
    m_out += flag ? "true" : "false";
}

void JsonWriter::color(const sgl::Color& value) {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "#%02x%02x%02x%02x",
                  static_cast<unsigned>(value.r), static_cast<unsigned>(value.g),
                  static_cast<unsigned>(value.b), static_cast<unsigned>(value.a));
    this->value(std::string_view(buffer));
}

void JsonWriter::null() {
    if (!m_afterKey) {
        prefix();
    }
    m_afterKey = false;
    m_out += "null";
}

std::string JsonWriter::take() {
    while (!m_scopes.empty()) {
        if (m_scopes.back().object) {
            endObject();
        } else {
            endArray();
        }
    }
    return std::move(m_out);
}

} // namespace editor
