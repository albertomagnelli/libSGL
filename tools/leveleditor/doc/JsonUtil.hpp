#pragma once

#include "common/Json.hpp"

#include <string>

namespace editor {

// Serialisiert einen gelesenen JSON-Teilbaum wieder als Text. Wird für
// unveränderte Tiled-Metadaten (z. B. Wang-Sets) im Projektformat benötigt.
std::string JsonToText(const sgl::detail::Json &value);

} // namespace editor
