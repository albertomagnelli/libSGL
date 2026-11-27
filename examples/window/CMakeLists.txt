set(SGL_WINDOW_EXAMPLES
    basic_window
    cursor
    frameless
    icon
    fullscreen
    window_attributes
    input
    clipboard
    drop
    monitors
    joystick
)

foreach(example IN LISTS SGL_WINDOW_EXAMPLES)
    set(target "sgl_example_${example}")
    add_executable(${target} ${example}.cpp)
    target_link_libraries(${target} PRIVATE sgl::sgl)
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../common)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
