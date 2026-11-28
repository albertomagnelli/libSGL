// Compiles the vendored tinyobjloader implementation once. The file is built
// with warnings disabled (see CMakeLists.txt) because third-party code is not
// held to the project's -Werror policy.

#define TINYOBJLOADER_IMPLEMENTATION
#define TINYOBJLOADER_USE_MAPBOX_EARCUT
#include "tiny_obj_loader.h"
