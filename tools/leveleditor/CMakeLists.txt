add_library(sgl_editor_core STATIC
    doc/Document.cpp
    doc/JsonWriter.cpp
    doc/TiledIO.cpp
    third_party/tinyxml2/tinyxml2.cpp
    third_party/miniz/miniz.c
    third_party/miniz/miniz_tdef.c
    third_party/miniz/miniz_tinfl.c
    third_party/miniz/miniz_zip.c
)
target_link_libraries(sgl_editor_core PUBLIC sgl::sgl)
target_include_directories(sgl_editor_core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tinyxml2
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniz)
target_compile_features(sgl_editor_core PUBLIC cxx_std_23)

if(MSVC)
    set_source_files_properties(
        third_party/tinyxml2/tinyxml2.cpp
        third_party/miniz/miniz.c
        third_party/miniz/miniz_tdef.c
        third_party/miniz/miniz_tinfl.c
        third_party/miniz/miniz_zip.c
        PROPERTIES COMPILE_OPTIONS "/w")
else()
    set_source_files_properties(
        third_party/tinyxml2/tinyxml2.cpp
        third_party/miniz/miniz.c
        third_party/miniz/miniz_tdef.c
        third_party/miniz/miniz_tinfl.c
        third_party/miniz/miniz_zip.c
        PROPERTIES COMPILE_OPTIONS "-w")
endif()

add_executable(sgl_editor
    main.cpp
    app/EditorApp.cpp
    app/EditorAppTools.cpp
    ui/Ui.cpp
)
target_link_libraries(sgl_editor PRIVATE sgl_editor_core)
target_include_directories(sgl_editor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(sgl_editor PRIVATE
    SGL_EDITOR_FONT="${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/DejaVuSans.ttf")
target_compile_features(sgl_editor PRIVATE cxx_std_23)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(sgl_editor PRIVATE -Wall -Wextra -Wpedantic)
endif()
