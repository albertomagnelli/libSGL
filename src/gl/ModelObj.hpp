#pragma once

#include <filesystem>
#include <span>

#include <sgl/Model.hpp>

namespace sgl::detail {

// Dateiendung .obj (unabhaengig von Gross-/Kleinschreibung).
[[nodiscard]] bool IsObjFile(const std::filesystem::path& file);

// Heuristik fuer Wavefront-OBJ-Text ohne Dateiendung (LoadFromMemory).
[[nodiscard]] bool LooksLikeObj(std::span<const u8> data);

[[nodiscard]] std::expected<Model, ModelError> LoadObjFile(
    const std::filesystem::path& file, const ModelLoadOptions& options);

[[nodiscard]] std::expected<Model, ModelError> LoadObjMemory(
    std::span<const u8> data, const std::filesystem::path& basePath,
    const ModelLoadOptions& options);

}  // namespace sgl::detail
