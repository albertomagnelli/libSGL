cmake_minimum_required(VERSION 3.25)
project(sgl VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_OBJCXX_STANDARD 23)
set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(SGL_BACKEND_X11     "Build the X11/GLX backend" ON)
option(SGL_BACKEND_WAYLAND "Build the Wayland/EGL backend" ON)
option(SGL_BACKEND_WIN32   "Build the Win32/WGL backend" ON)
option(SGL_BACKEND_COCOA   "Build the Cocoa/NSOpenGL backend" ON)
option(SGL_BACKEND_OSMESA  "Build the OSMesa context helper" ON)
option(SGL_BUILD_TESTS     "Build the Catch2 test suite" ON)
option(SGL_BUILD_EXAMPLES  "Build the example applications" ON)
option(SGL_BUILD_SHARED    "Build sgl as a shared library" OFF)
option(SGL_INSTALL         "Generate install and CMake package rules" ON)
option(SGL_HEADLESS_RENDER_TESTS
       "Add headless rendering smoke tests (requires a working EGL/OSMesa setup)" OFF)
set(SGL_SANITIZE "" CACHE STRING
    "Comma-separated sanitizers for GCC/Clang, e.g. 'address,undefined'")

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if(SGL_SANITIZE)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR "SGL_SANITIZE is only supported with GCC or Clang")
    endif()
    add_compile_options(-fsanitize=${SGL_SANITIZE} -fno-omit-frame-pointer)
    add_link_options(-fsanitize=${SGL_SANITIZE})
endif()

if(MSVC)
    # Quelltext und String-Literale sind UTF-8; ohne /utf-8 interpretiert
    # MSVC sie in der lokalen Codepage (C2015 bei U'Ä' o. ä.).
    add_compile_options(/utf-8 /Zc:__cplusplus)
endif()

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated/sgl")
configure_file(cmake/Version.hpp.in
               "${CMAKE_CURRENT_BINARY_DIR}/generated/sgl/Version.hpp" @ONLY)

find_package(glm CONFIG REQUIRED)

set(SGL_SOURCES
    src/common/Backend.cpp
    src/common/Context.cpp
    src/common/Cursor.cpp
    src/common/Error.cpp
    src/common/Events.cpp
    src/common/GamepadMapping.cpp
    src/common/GamepadMappingsGenerated.cpp
    src/common/Init.cpp
    src/common/Input.cpp
    src/common/Joystick.cpp
    src/common/Monitor.cpp
    src/common/Native.cpp
    src/common/Time.cpp
    src/common/Window.cpp
    src/common/Log.cpp
    src/gl/GL.cpp
    src/gl/Image.cpp
    src/gl/Font.cpp
    src/gl/MeshFont.cpp
    src/gl/DefaultFont.cpp
    src/gl/Color.cpp
    src/gl/Geometry.cpp
    src/gl/Material.cpp
    src/gl/MeshVertex.cpp
    src/gl/Model.cpp
    src/gl/Noise.cpp
    src/gl/Shader.cpp
    src/gl/Texture.cpp
    src/gl/AnimatedSprite.cpp
    src/gl/Audio.cpp
    src/gl/AudioCodecs.cpp
    src/gl/CollisionShape.cpp
    src/gl/Framebuffer.cpp
    src/gl/Sprite.cpp
    src/common/Json.cpp
    third_party/ttf2mesh/ttf2mesh.c
    third_party/audio/miniaudio.c
    third_party/audio/stb_vorbis.c
    third_party/audio/qoa_impl.c
    third_party/audio/jar_xm_impl.c
    third_party/model/cgltf_impl.c
    third_party/model/ufbx.c
)

set(SGL_DEFINITIONS "")
set(SGL_PRIVATE_INCLUDE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/audio"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/model"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ttf2mesh"
)
set(SGL_LINK_LIBS glm::glm)

if(UNIX AND NOT APPLE)
    list(APPEND SGL_SOURCES src/common/LinuxJoystick.cpp)
    list(APPEND SGL_DEFINITIONS SGL_HAS_EVDEV=1)
else()
    list(APPEND SGL_DEFINITIONS SGL_HAS_EVDEV=0)
endif()

find_package(Threads REQUIRED)
list(APPEND SGL_LINK_LIBS Threads::Threads)

# miniaudio lädt seine Backends unter Linux zur Laufzeit (dlopen).
if(UNIX)
    list(APPEND SGL_LINK_LIBS ${CMAKE_DL_LIBS} m)
endif()

# ---------------------------------------------------------------------------
# X11 / GLX
# ---------------------------------------------------------------------------
if(UNIX AND NOT APPLE AND SGL_BACKEND_X11)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(X11 REQUIRED IMPORTED_TARGET x11 xrandr xinerama xcursor xi xext gl egl)
    list(APPEND SGL_SOURCES
        src/backend/x11/X11Backend.cpp
        src/backend/x11/X11Window.cpp
        src/backend/x11/X11Input.cpp
        src/backend/x11/X11Clipboard.cpp
    )
    list(APPEND SGL_LINK_LIBS PkgConfig::X11)
    list(APPEND SGL_DEFINITIONS SGL_HAS_X11=1)
else()
    list(APPEND SGL_DEFINITIONS SGL_HAS_X11=0)
endif()

# ---------------------------------------------------------------------------
# Wayland / EGL
# ---------------------------------------------------------------------------
if(UNIX AND NOT APPLE AND SGL_BACKEND_WAYLAND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(WAYLAND REQUIRED IMPORTED_TARGET
        wayland-client wayland-egl wayland-cursor xkbcommon egl)
    find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)

    set(WAYLAND_PROTOCOLS_DIR "/usr/share/wayland-protocols")
    if(NOT EXISTS "${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml")
        message(FATAL_ERROR "wayland-protocols not found in ${WAYLAND_PROTOCOLS_DIR}")
    endif()

    set(SGL_WAYLAND_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/wayland")
    file(MAKE_DIRECTORY "${SGL_WAYLAND_GENERATED_DIR}")

    function(sgl_generate_wayland_protocol xml_file base_name)
        set(header "${SGL_WAYLAND_GENERATED_DIR}/${base_name}-client-protocol.h")
        set(source "${SGL_WAYLAND_GENERATED_DIR}/${base_name}-protocol.c")
        add_custom_command(
            OUTPUT "${header}"
            COMMAND ${WAYLAND_SCANNER} client-header "${xml_file}" "${header}"
            DEPENDS "${xml_file}"
            VERBATIM)
        add_custom_command(
            OUTPUT "${source}"
            COMMAND ${WAYLAND_SCANNER} private-code "${xml_file}" "${source}"
            DEPENDS "${xml_file}" "${header}"
            VERBATIM)
        list(APPEND SGL_WAYLAND_GENERATED_SOURCES "${source}")
        set(SGL_WAYLAND_GENERATED_SOURCES "${SGL_WAYLAND_GENERATED_SOURCES}" PARENT_SCOPE)
        list(APPEND SGL_WAYLAND_GENERATED_HEADERS "${header}")
        set(SGL_WAYLAND_GENERATED_HEADERS "${SGL_WAYLAND_GENERATED_HEADERS}" PARENT_SCOPE)
    endfunction()

    sgl_generate_wayland_protocol(
        "${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml" xdg-shell)
    sgl_generate_wayland_protocol(
        "${WAYLAND_PROTOCOLS_DIR}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml"
        xdg-decoration-unstable-v1)
    sgl_generate_wayland_protocol(
        "${WAYLAND_PROTOCOLS_DIR}/stable/viewporter/viewporter.xml" viewporter)
    sgl_generate_wayland_protocol(
        "${WAYLAND_PROTOCOLS_DIR}/unstable/xdg-output/xdg-output-unstable-v1.xml"
        xdg-output-unstable-v1)

    list(APPEND SGL_SOURCES
        src/backend/wayland/WaylandBackend.cpp
        src/backend/wayland/WaylandWindow.cpp
        ${SGL_WAYLAND_GENERATED_SOURCES}
    )
    list(APPEND SGL_PRIVATE_INCLUDE_DIRS "${SGL_WAYLAND_GENERATED_DIR}")
    list(APPEND SGL_LINK_LIBS PkgConfig::WAYLAND)
    list(APPEND SGL_DEFINITIONS SGL_HAS_WAYLAND=1)
else()
    list(APPEND SGL_DEFINITIONS SGL_HAS_WAYLAND=0)
endif()

# ---------------------------------------------------------------------------
# Win32 / WGL
# ---------------------------------------------------------------------------
if(WIN32 AND SGL_BACKEND_WIN32)
    list(APPEND SGL_SOURCES
        src/backend/win32/Win32Backend.cpp
    )
    list(APPEND SGL_LINK_LIBS opengl32 gdi32 user32 shell32 ole32 winmm)
    list(APPEND SGL_DEFINITIONS SGL_HAS_WIN32=1 UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
else()
    list(APPEND SGL_DEFINITIONS SGL_HAS_WIN32=0)
endif()

# ---------------------------------------------------------------------------
# Cocoa / NSOpenGL
# ---------------------------------------------------------------------------
if(APPLE AND SGL_BACKEND_COCOA)
    enable_language(OBJCXX)
    list(APPEND SGL_SOURCES
        src/backend/cocoa/CocoaBackend.mm
    )
    list(APPEND SGL_LINK_LIBS
        "-framework Cocoa"
        "-framework OpenGL"
        "-framework IOKit"
        "-framework CoreVideo"
        "-framework QuartzCore")
    list(APPEND SGL_DEFINITIONS SGL_HAS_COCOA=1)
else()
    list(APPEND SGL_DEFINITIONS SGL_HAS_COCOA=0)
endif()

# ---------------------------------------------------------------------------
# Null platform (always available)
# ---------------------------------------------------------------------------
list(APPEND SGL_SOURCES src/backend/null/NullBackend.cpp)

# ---------------------------------------------------------------------------
# Shared context helpers
# ---------------------------------------------------------------------------
if(UNIX AND NOT APPLE AND SGL_BACKEND_WAYLAND)
    list(APPEND SGL_DEFINITIONS SGL_HAS_EGL=1)
else()
    list(APPEND SGL_DEFINITIONS SGL_HAS_EGL=0)
endif()

if(SGL_BACKEND_OSMESA)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(OSMESA IMPORTED_TARGET osmesa)
    endif()
    if(PkgConfig_FOUND AND OSMESA_FOUND)
        list(APPEND SGL_LINK_LIBS PkgConfig::OSMESA)
        list(APPEND SGL_DEFINITIONS SGL_HAS_OSMESA=1)
        set(SGL_PACKAGE_OSMESA ON)
    else()
        message(STATUS "OSMesa not found - OSMesa context support disabled")
        list(APPEND SGL_DEFINITIONS SGL_HAS_OSMESA=0)
        set(SGL_PACKAGE_OSMESA OFF)
    endif()
else()
    list(APPEND SGL_DEFINITIONS SGL_HAS_OSMESA=0)
    set(SGL_PACKAGE_OSMESA OFF)
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(SGL_THIRD_PARTY_C_SOURCES
    third_party/ttf2mesh/ttf2mesh.c
    third_party/audio/miniaudio.c
    third_party/audio/stb_vorbis.c
    third_party/audio/qoa_impl.c
    third_party/audio/jar_xm_impl.c
    third_party/model/cgltf_impl.c
    third_party/model/ufbx.c
)

if(MSVC)
    set_source_files_properties(${SGL_THIRD_PARTY_C_SOURCES} PROPERTIES COMPILE_OPTIONS "/w")
else()
    set(SGL_THIRD_PARTY_COMPILE_OPTIONS "-w")
    if(SGL_SANITIZE)
        # Vendorte C-Bibliotheken (z. B. ttf2mesh) haben eigene, bekannte
        # Alignmentschwächen; sie sollen den Sanitizer-Lauf nicht blockieren.
        list(APPEND SGL_THIRD_PARTY_COMPILE_OPTIONS "-fno-sanitize=${SGL_SANITIZE}")
    endif()
    set_source_files_properties(${SGL_THIRD_PARTY_C_SOURCES}
        PROPERTIES COMPILE_OPTIONS "${SGL_THIRD_PARTY_COMPILE_OPTIONS}")
endif()

if(SGL_BUILD_SHARED)
    add_library(sgl SHARED ${SGL_SOURCES})
    set_target_properties(sgl PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
else()
    add_library(sgl STATIC ${SGL_SOURCES})
endif()
add_library(sgl::sgl ALIAS sgl)
set_target_properties(sgl PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(DEFINED SGL_WAYLAND_GENERATED_HEADERS AND SGL_WAYLAND_GENERATED_HEADERS)
    add_custom_target(sgl_wayland_protocols ALL DEPENDS ${SGL_WAYLAND_GENERATED_HEADERS})
    add_dependencies(sgl sgl_wayland_protocols)
endif()

target_compile_features(sgl PUBLIC cxx_std_23)
target_include_directories(sgl
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE ${SGL_PRIVATE_INCLUDE_DIRS})
target_compile_definitions(sgl PRIVATE ${SGL_DEFINITIONS})
target_link_libraries(sgl PUBLIC ${SGL_LINK_LIBS})

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(sgl PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(SGL_BUILD_TESTS)
    add_library(sgl_catch2 STATIC third_party/catch2/catch_amalgamated.cpp)
    target_include_directories(sgl_catch2 PUBLIC third_party/catch2)
    target_compile_features(sgl_catch2 PUBLIC cxx_std_17)

    enable_testing()
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
# Examples
# ---------------------------------------------------------------------------
if(SGL_BUILD_EXAMPLES)
    add_subdirectory(examples/gl_loader)
    add_subdirectory(examples/window)
    add_subdirectory(examples/font)
    add_subdirectory(examples/audio)
    add_subdirectory(examples/model)
endif()

# ---------------------------------------------------------------------------
# Optional headless rendering smoke tests
# ---------------------------------------------------------------------------
if(SGL_BUILD_TESTS AND SGL_BUILD_EXAMPLES AND SGL_HEADLESS_RENDER_TESTS)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/headless")
    add_test(NAME sgl_render_font2d
             COMMAND sgl_example_meshfont2d
                     --frame "${CMAKE_CURRENT_BINARY_DIR}/headless/font2d.png" 320 180)
    add_test(NAME sgl_render_font3d
             COMMAND sgl_example_meshfont3d
                     --frame "${CMAKE_CURRENT_BINARY_DIR}/headless/font3d.png" 320 180)
    add_test(NAME sgl_render_model
             COMMAND sgl_example_model_viewer
                     --frame "${CMAKE_CURRENT_BINARY_DIR}/headless/model.png" --time 0.5)
    set_tests_properties(sgl_render_font2d sgl_render_font3d sgl_render_model
                         PROPERTIES TIMEOUT 180)
endif()

# ---------------------------------------------------------------------------
# Installation / find_package(sgl)
# ---------------------------------------------------------------------------
if(SGL_INSTALL)
    install(TARGETS sgl EXPORT sglTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(DIRECTORY include/sgl DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/generated/sgl/Version.hpp"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/sgl)
    install(EXPORT sglTargets
        FILE sglTargets.cmake
        NAMESPACE sgl::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sgl)

    configure_package_config_file(cmake/sglConfig.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/sglConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sgl)
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/sglConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion)
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/sglConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/sglConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sgl)
endif()
