add_executable(sgl_example_meshfont2d meshfont_2d_example.cpp)
target_link_libraries(sgl_example_meshfont2d PRIVATE sgl::sgl)
target_include_directories(sgl_example_meshfont2d PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(sgl_example_meshfont3d meshfont_3d_example.cpp)
target_link_libraries(sgl_example_meshfont3d PRIVATE sgl::sgl)
target_include_directories(sgl_example_meshfont3d PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(sgl_example_meshfont2d PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(sgl_example_meshfont3d PRIVATE -Wall -Wextra -Wpedantic)
endif()
