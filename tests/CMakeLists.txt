add_executable(sgl_tests
    test_sgl.cpp
    test_gamepad_mapping.cpp
    test_logger.cpp
    test_image.cpp
    test_font.cpp
    test_meshfont.cpp
    test_shader_texture.cpp
    test_color_geometry.cpp
    test_material.cpp
    test_meshvertex.cpp
    test_model.cpp
    test_noise.cpp
    test_collision_animatedsprite.cpp
    test_audio.cpp
    test_framebuffer.cpp
)

target_link_libraries(sgl_tests PRIVATE sgl::sgl sgl_catch2)
target_compile_features(sgl_tests PRIVATE cxx_std_23)
target_include_directories(sgl_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_compile_definitions(sgl_tests PRIVATE
    SGL_TEST_AUDIO_RESOURCES="${CMAKE_CURRENT_SOURCE_DIR}/../examples/audio/resources"
    SGL_TEST_MODEL_RESOURCES="${CMAKE_CURRENT_SOURCE_DIR}/../examples/model/resources")

add_test(NAME sgl_tests COMMAND sgl_tests)
