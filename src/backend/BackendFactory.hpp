#pragma once

#include <memory>

#include "sgl/Error.hpp"

namespace sgl::detail {

class Backend;

std::unique_ptr<Backend> CreateX11Backend(Error& error);
std::unique_ptr<Backend> CreateWaylandBackend(Error& error);
std::unique_ptr<Backend> CreateWin32Backend(Error& error);
std::unique_ptr<Backend> CreateCocoaBackend(Error& error);
std::unique_ptr<Backend> CreateNullBackend(Error& error);

} // namespace sgl::detail
