cmake_minimum_required(VERSION 3.20)
project(Cala)

set(CMAKE_CXX_STANDARD 20)

find_package(SDL2 REQUIRED)
find_package(X11 REQUIRED)
find_package(Vulkan REQUIRED)

add_subdirectory(third_party)
add_subdirectory(examples)
add_subdirectory(libs)

add_library(Cala src/backend/vulkan/Context.cpp include/Cala/backend/vulkan/Context.h src/backend/vulkan/Swapchain.cpp include/Cala/backend/vulkan/Swapchain.h src/backend/vulkan/Driver.cpp include/Cala/backend/vulkan/Driver.h src/backend/vulkan/ShaderProgram.cpp include/Cala/backend/vulkan/ShaderProgram.h src/backend/vulkan/CommandBufferList.cpp include/Cala/backend/vulkan/CommandBufferList.h src/backend/vulkan/PipelineList.cpp include/Cala/backend/vulkan/PipelineList.h src/backend/vulkan/CommandBuffer.cpp include/Cala/backend/vulkan/CommandBuffer.h)

target_include_directories(Cala
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:install>
        PRIVATE
        src
        libs)

target_link_libraries(Cala Ende ${Vulkan_LIBRARIES} ${SDL2_LIBRARIES} ${X11_LIBRARIES} imgui)