//
// Created by ingsto on 14.11.26.
//
// Json: kleiner Leser (rekursiver Abstieg) fuer Atlas-Metadaten.
//
#include "common/Json.hpp"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace sgl::detail {

namespace {
  const Json g_empty;
}

class JsonParser {
public:
  explicit JsonParser(const std::string_view text) : m_text(text) {}

  bool parse(Json &out) {
    skipSpace();

    if (!parseValue(out, 0)) return false;

    skipSpace();

    if (m_position != m_text.size()) return fail("ueberzaehlige Zeichen am Ende");

    return true;
  }

  const std::string &error() const { return m_error; }

private:
  static constexpr i32 MaxDepth = 64;

  bool fail(const std::string &message) {
    if (m_error.empty()) {
      m_error = message + " (Position " + std::to_string(m_position) + ")";
    }

    return false;
  }

  bool atEnd() const { return m_position >= m_text.size(); }

  char peek() const { return atEnd() ? '\0' : m_text[m_position]; }

  void skipSpace() {
    while (!atEnd()) {
      const char current = m_text[m_position];

      if ((current == ' ') || (current == '\t') || (current == '\n') || (current == '\r')) {
        ++m_position;
        continue;
      }

      break;
    }
  }

  bool parseValue(Json &out, const i32 depth) {
    if (depth > MaxDepth) return fail("Verschachtelung zu tief");

    skipSpace();

    if (atEnd()) return fail("unerwartetes Ende");

    switch (peek()) {
      case '{': return parseObject(out, depth);
      case '[': return parseArray(out, depth);
      case '"': {
        std::string text;

        if (!parseString(text)) return false;

        out.m_type   = Json::Type::String;
        out.m_string = std::move(text);

        return true;
      }
      case 't':
        if (!consume("true")) return false;

        out.m_type = Json::Type::Bool;
        out.m_bool = true;

        return true;
      case 'f':
        if (!consume("false")) return false;

        out.m_type = Json::Type::Bool;
        out.m_bool = false;

        return true;
      case 'n':
        if (!consume("null")) return false;

        out.m_type = Json::Type::Null;

        return true;
      default: return parseNumber(out);
    }
  }

  bool consume(const char *literal) {
    const std::string_view expected(literal);

    if (m_text.substr(m_position, expected.size()) != expected) return fail("unbekanntes Literal");

    m_position += expected.size();

    return true;
  }

  bool parseObject(Json &out, const i32 depth) {
    ++m_position; // '{'

    out.m_type = Json::Type::Object;
    out.m_keys.clear();
    out.m_values.clear();

    skipSpace();

    if (peek() == '}') {
      ++m_position;

      return true;
    }

    while (true) {
      skipSpace();

      if (peek() != '"') return fail("Schluessel erwartet");

      std::string key;

      if (!parseString(key)) return false;

      skipSpace();

      if (peek() != ':') return fail("':' erwartet");

      ++m_position;

      Json value;

      if (!parseValue(value, depth + 1)) return false;

      out.m_keys.push_back(std::move(key));
      out.m_values.push_back(std::move(value));

      skipSpace();

      if (peek() == ',') {
        ++m_position;
        continue;
      }

      if (peek() == '}') {
        ++m_position;

        return true;
      }

      return fail("',' oder '}' erwartet");
    }
  }

  bool parseArray(Json &out, const i32 depth) {
    ++m_position; // '['

    out.m_type = Json::Type::Array;
    out.m_elements.clear();

    skipSpace();

    if (peek() == ']') {
      ++m_position;

      return true;
    }

    while (true) {
      Json element;

      if (!parseValue(element, depth + 1)) return false;

      out.m_elements.push_back(std::move(element));

      skipSpace();

      if (peek() == ',') {
        ++m_position;
        continue;
      }

      if (peek() == ']') {
        ++m_position;

        return true;
      }

      return fail("',' oder ']' erwartet");
    }
  }

  static void AppendUtf8(std::string &out, const u32 codepoint) {
    if (codepoint < 0x80) {
      out.push_back(static_cast<char>(codepoint));
    } else if (codepoint < 0x800) {
      out.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
      out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint < 0x10000) {
      out.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
      out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else {
      out.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
      out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
  }

  bool parseHex4(u32 &value) {
    if ((m_position + 4) > m_text.size()) return fail("\\u braucht vier Hexziffern");

    value = 0;

    for (i32 i = 0; i < 4; ++i) {
      const char digit = m_text[m_position + static_cast<std::size_t>(i)];

      u32 nibble = 0;

      if ((digit >= '0') && (digit <= '9')) {
        nibble = static_cast<u32>(digit - '0');
      } else if ((digit >= 'a') && (digit <= 'f')) {
        nibble = static_cast<u32>(digit - 'a' + 10);
      } else if ((digit >= 'A') && (digit <= 'F')) {
        nibble = static_cast<u32>(digit - 'A' + 10);
      } else {
        return fail("ungueltige Hexziffer");
      }

      value = (value << 4) | nibble;
    }

    m_position += 4;

    return true;
  }

  bool parseString(std::string &out) {
    ++m_position; // '"'

    out.clear();

    while (true) {
      if (atEnd()) return fail("Zeichenkette nicht geschlossen");

      const char current = m_text[m_position++];

      if (current == '"') return true;

      if (current != '\\') {
        out.push_back(current);
        continue;
      }

      if (atEnd()) return fail("Escape am Ende");

      switch (const char escape = m_text[m_position++]) {
        case '"': out.push_back('"'); break;
        case '\\': out.push_back('\\'); break;
        case '/': out.push_back('/'); break;
        case 'b': out.push_back('\b'); break;
        case 'f': out.push_back('\f'); break;
        case 'n': out.push_back('\n'); break;
        case 'r': out.push_back('\r'); break;
        case 't': out.push_back('\t'); break;
        case 'u': {
          u32 first = 0;

          if (!parseHex4(first)) return false;

          // Surrogatpaar
          if ((first >= 0xD800) && (first <= 0xDBFF) && ((m_position + 6) <= m_text.size()) &&
              (m_text[m_position] == '\\') && (m_text[m_position + 1] == 'u')) {
            const std::size_t saved = m_position;

            m_position += 2;

            u32 second = 0;

            if (!parseHex4(second)) return false;

            if ((second >= 0xDC00) && (second <= 0xDFFF)) {
              first = 0x10000 + ((first - 0xD800) << 10) + (second - 0xDC00);
            } else {
              m_position = saved;
            }
          }

          AppendUtf8(out, first);

          break;
        }
        default: return fail(std::string("unbekanntes Escape \\") + escape);
      }
    }
  }

  bool parseNumber(Json &out) {
    const std::size_t start = m_position;

    if (peek() == '-') ++m_position;

    while (!atEnd() && (std::isdigit(static_cast<unsigned char>(peek())) != 0)) ++m_position;

    if (peek() == '.') {
      ++m_position;

      while (!atEnd() && (std::isdigit(static_cast<unsigned char>(peek())) != 0)) ++m_position;
    }

    if ((peek() == 'e') || (peek() == 'E')) {
      ++m_position;

      if ((peek() == '+') || (peek() == '-')) ++m_position;

      while (!atEnd() && (std::isdigit(static_cast<unsigned char>(peek())) != 0)) ++m_position;
    }

    if (m_position == start) return fail("Zahl erwartet");

    const std::string text(m_text.substr(start, m_position - start));

    out.m_type   = Json::Type::Number;
    out.m_number = std::strtod(text.c_str(), nullptr);

    return true;
  }

  std::string_view m_text;
  std::size_t      m_position{0};
  std::string      m_error;
};

// -----------------------------------------------------------------------------

Json Json::Parse(const std::string_view text) {
  Json       document;
  JsonParser parser(text);

  if (!parser.parse(document)) {
    Json failed;
    failed.m_error = parser.error();

    return failed;
  }

  return document;
}

Json Json::ParseFile(const std::string &path) {
  std::ifstream stream(path, std::ios::binary);

  if (!stream) {
    Json failed;
    failed.m_error = "Datei \"" + path + "\" nicht lesbar";

    return failed;
  }

  std::ostringstream buffer;
  buffer << stream.rdbuf();

  return Parse(buffer.str());
}

bool Json::isValid() const { return m_error.empty(); }

bool Json::isNull() const { return m_type == Type::Null; }

bool Json::isObject() const { return m_type == Type::Object; }

bool Json::isArray() const { return m_type == Type::Array; }

bool Json::isString() const { return m_type == Type::String; }

bool Json::isNumber() const { return m_type == Type::Number; }

Json::Type Json::kind() const { return m_type; }

const Json *Json::find(const std::string_view key) const {
  if (m_type != Type::Object) return nullptr;

  for (std::size_t i = 0; i < m_keys.size(); ++i) {
    if (m_keys[i] == key) return &m_values[i];
  }

  return nullptr;
}

bool Json::has(const std::string_view key) const { return find(key) != nullptr; }

i32 Json::size() const {
  switch (m_type) {
    case Type::Array: return static_cast<i32>(m_elements.size());
    case Type::Object: return static_cast<i32>(m_keys.size());
    default: return 0;
  }
}

const Json &Json::at(const i32 index) const {
  if ((m_type != Type::Array) || (index < 0) || (index >= static_cast<i32>(m_elements.size()))) return g_empty;

  return m_elements[static_cast<std::size_t>(index)];
}

const std::string &Json::keyAt(const i32 index) const {
  static const std::string empty;

  if ((m_type != Type::Object) || (index < 0) || (index >= static_cast<i32>(m_keys.size()))) return empty;

  return m_keys[static_cast<std::size_t>(index)];
}

f64 Json::number(const f64 fallback) const {
  if (m_type == Type::Number) return m_number;
  if (m_type == Type::Bool) return m_bool ? 1.0 : 0.0;

  return fallback;
}

i32 Json::intValue(const i32 fallback) const {
  if ((m_type != Type::Number) && (m_type != Type::Bool)) return fallback;

  return static_cast<i32>(std::llround(number()));
}

f32 Json::floatValue(const f32 fallback) const {
  if ((m_type != Type::Number) && (m_type != Type::Bool)) return fallback;

  return static_cast<f32>(number());
}

bool Json::boolValue(const bool fallback) const {
  if (m_type == Type::Bool) return m_bool;
  if (m_type == Type::Number) return m_number != 0.0;

  return fallback;
}

const std::string &Json::string() const {
  static const std::string empty;

  return (m_type == Type::String) ? m_string : empty;
}

const std::string &Json::error() const { return m_error; }

}  // namespace sgl::detail

