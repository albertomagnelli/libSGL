set(SGL_AUDIO_EXAMPLES
    audio_info
    audio_sound_loading
    audio_music_stream
    audio_raw_stream
)

foreach(example IN LISTS SGL_AUDIO_EXAMPLES)
    set(target "sgl_example_${example}")
    add_executable(${target} ${example}.cpp)
    target_link_libraries(${target} PRIVATE sgl::sgl)
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_definitions(${target} PRIVATE
        SGL_AUDIO_RESOURCES="${CMAKE_CURRENT_SOURCE_DIR}/resources")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
