cmake_minimum_required(VERSION 3.20)
project(CalaEditor)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -DENDE_PROFILE)

find_package(SDL2 REQUIRED)

add_library(CalaEditor src/Editor.cpp include/Editor/Editor.h include/Editor/Panel.h)

target_include_directories(CalaEditor
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:install>
        PRIVATE
        src
        libs
        third_party)

target_link_libraries(CalaEditor Cala Ende ${SDL2_LIBRARIES} imgui assimp spirv-cross-cpp)