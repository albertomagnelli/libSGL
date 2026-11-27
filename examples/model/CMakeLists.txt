add_executable(sgl_example_model_viewer model_viewer.cpp)
target_link_libraries(sgl_example_model_viewer PRIVATE sgl::sgl)
target_include_directories(sgl_example_model_viewer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common)
target_compile_definitions(sgl_example_model_viewer PRIVATE
    SGL_MODEL_RESOURCES="${CMAKE_CURRENT_SOURCE_DIR}/resources")

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(sgl_example_model_viewer PRIVATE -Wall -Wextra -Wpedantic)
endif()
