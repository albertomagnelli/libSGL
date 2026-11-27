add_executable(sgl_tests
    test_sgl.cpp
)

target_link_libraries(sgl_tests PRIVATE sgl::sgl sgl_catch2)
target_compile_features(sgl_tests PRIVATE cxx_std_23)

add_test(NAME sgl_tests COMMAND sgl_tests)
