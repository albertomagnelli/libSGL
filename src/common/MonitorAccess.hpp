#pragma once

#include "sgl/Monitor.hpp"

namespace sgl::detail {

struct MonitorAccess {
    static Monitor make(void* native, Platform platform) {
        Monitor monitor;
        monitor.m_native = native;
        monitor.m_platform = platform;
        return monitor;
    }

    static void* native(const Monitor& monitor) {
        return monitor.m_native;
    }
};

} // namespace sgl::detail
