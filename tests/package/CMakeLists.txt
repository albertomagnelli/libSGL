cmake_minimum_required(VERSION 3.25)
project(sgl_package_consumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(sgl 0.1 CONFIG REQUIRED)

add_executable(sgl_package_consumer main.cpp)
target_link_libraries(sgl_package_consumer PRIVATE sgl::sgl)
