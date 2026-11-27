#pragma once

#include <memory>

#include "sgl/Error.hpp"

namespace sgl::detail {

class Backend;

std::unique_ptr<Backend> createX11Backend(Error& error);
std::unique_ptr<Backend> createWaylandBackend(Error& error);
std::unique_ptr<Backend> createWin32Backend(Error& error);
std::unique_ptr<Backend> createCocoaBackend(Error& error);
std::unique_ptr<Backend> createNullBackend(Error& error);

} // namespace sgl::detail
