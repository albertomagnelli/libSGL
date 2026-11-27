#include "sgl/Cursor.hpp"

#include "backend/Backend.hpp"

namespace sgl {

std::expected<std::unique_ptr<Cursor>, Error> Cursor::Create(const WindowIcon& image, ivec2 hotspot) {
    auto* backend = detail::GetBackend();
    if (!backend) {
        return std::unexpected(detail::MakeError(ErrorCode::NotInitialized,
                                                 "sgl ist nicht initialisiert."));
    }
    Error error;
    auto cursorBackend = backend->createCursor(image, hotspot, error);
    if (!cursorBackend) {
        if (!error.isError()) {
            error = detail::MakeError(ErrorCode::PlatformError, "Cursor konnte nicht erstellt werden.");
        }
        return std::unexpected(std::move(error));
    }
    return std::unique_ptr<Cursor>(new Cursor(std::move(cursorBackend)));
}

std::expected<std::unique_ptr<Cursor>, Error> Cursor::CreateStandard(StandardCursor shape) {
    auto* backend = detail::GetBackend();
    if (!backend) {
        return std::unexpected(detail::MakeError(ErrorCode::NotInitialized,
                                                 "sgl ist nicht initialisiert."));
    }
    Error error;
    auto cursorBackend = backend->createStandardCursor(shape, error);
    if (!cursorBackend) {
        if (!error.isError()) {
            error = detail::MakeError(ErrorCode::PlatformError, "Standard-Cursor konnte nicht erstellt werden.");
        }
        return std::unexpected(std::move(error));
    }
    return std::unique_ptr<Cursor>(new Cursor(std::move(cursorBackend)));
}

Cursor::Cursor(std::unique_ptr<detail::CursorBackend> backend)
    : m_backend(std::move(backend)) {}

Cursor::~Cursor() = default;
Cursor::Cursor(Cursor&&) noexcept = default;
Cursor& Cursor::operator=(Cursor&&) noexcept = default;

void* Cursor::getNativeHandle() const {
    return m_backend ? m_backend->getNativeHandle() : nullptr;
}

} // namespace sgl
