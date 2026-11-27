#pragma once
// sgl · Json
// Winziger, abhaengigkeitsfreier JSON-Leser fuer Metadaten (z. B. Aseprite-Atlas).
//
//   Json document = Json::Parse(text);
//   if (!document.IsValid()) { /* document.Error() */ }
//   const Json *frames = document.Find("frames");
//
// Bewusst nur Lesen, keine Serialisierung. Objekte behalten die Reihenfolge der
// Datei, damit Atlas-Rahmen in der Reihenfolge bleiben, in der sie im Blatt stehen.

#include <string>
#include <string_view>
#include <vector>

#include <sgl/Types.hpp>

namespace sgl::detail {

class Json {
public:
  enum class Type { Null, Bool, Number, String, Array, Object };

  Json() = default;

  // Text auswerten; bei Fehlern ist das Ergebnis ungueltig und Error() gefuellt
  static Json Parse(std::string_view text);

  // Datei lesen und auswerten
  static Json ParseFile(const std::string &path);

  bool isValid() const;
  bool isNull() const;
  bool isObject() const;
  bool isArray() const;
  bool isString() const;
  bool isNumber() const;

  Type kind() const;

  // Objektzugriff (nullptr, wenn kein Objekt oder Schluessel fehlt)
  const Json *find(std::string_view key) const;
  bool        has(std::string_view key) const;

  // Array- und Objektinhalt
  i32 size() const; // Anzahl Elemente/Mitglieder
  const Json &at(i32 index) const;
  const std::string &keyAt(i32 index) const;

  // Werte mit Rueckfallwerten (nie werfen)
  f64                number(f64 fallback = 0.0) const;
  i32                intValue(i32 fallback = 0) const;
  f32                floatValue(f32 fallback = 0.0f) const;
  bool               boolValue(bool fallback = false) const;
  const std::string &string() const;

  const std::string &error() const;

private:
  Type               m_type{Type::Null};
  bool               m_bool{false};
  f64                m_number{0.0};
  std::string        m_string;
  std::vector<Json>        m_elements; // Array
  std::vector<std::string> m_keys;     // Objekt: Schluessel ...
  std::vector<Json>        m_values;   // ... und Werte (Reihenfolge der Datei)

  std::string m_error;

  friend class JsonParser;
};

}  // namespace sgl::detail

