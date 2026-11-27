#include <sgl/Init.hpp>
#include <sgl/Version.hpp>

#include <cstdio>

int main() {
    const sgl::Version version = sgl::GetVersion();
    std::printf("sgl %s (%d.%d.%d)\n", SGL_VERSION_STRING, version.major,
                version.minor, version.revision);
    return (version.major == SGL_VERSION_MAJOR) ? 0 : 1;
}
