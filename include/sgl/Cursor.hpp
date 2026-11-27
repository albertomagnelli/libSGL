#pragma once

#include <expected>
#include <memory>
#include <span>
#include <vector>

#include "Error.hpp"
#include "Types.hpp"

namespace sgl {

struct Image {
    ivec2 size{0, 0};
    std::vector<u8> rgba8;
};

enum class StandardCursor : i32 {
    Arrow = 0x00036001,
    IBeam = 0x00036002,
    Crosshair = 0x00036003,
    Hand = 0x00036004,
    HResize = 0x00036005,
    VResize = 0x00036006,
};

namespace detail {
class CursorBackend;
}

class Cursor {
public:
    static std::expected<std::unique_ptr<Cursor>, Error> create(const Image& image, ivec2 hotspot);
    static std::expected<std::unique_ptr<Cursor>, Error> createStandard(StandardCursor shape);

    ~Cursor();
    Cursor(const Cursor&) = delete;
    Cursor& operator=(const Cursor&) = delete;
    Cursor(Cursor&&) noexcept;
    Cursor& operator=(Cursor&&) noexcept;

    [[nodiscard]] void* getNativeHandle() const;

private:
    friend class Window;
    explicit Cursor(std::unique_ptr<detail::CursorBackend> backend);
    std::unique_ptr<detail::CursorBackend> m_backend;
};

} // namespace sgl
