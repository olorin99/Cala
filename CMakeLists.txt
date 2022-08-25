cmake_minimum_required(VERSION 3.20)
project(Cala)

set(CMAKE_CXX_STANDARD 20)

find_package(SDL2 REQUIRED)
find_package(X11 REQUIRED)
find_package(Vulkan REQUIRED)

add_subdirectory(third_party)
add_subdirectory(examples)
add_subdirectory(libs)

add_library(Cala src/backend/vulkan/Context.cpp include/Cala/backend/vulkan/Context.h src/backend/vulkan/Swapchain.cpp include/Cala/backend/vulkan/Swapchain.h src/backend/vulkan/Driver.cpp include/Cala/backend/vulkan/Driver.h src/backend/vulkan/ShaderProgram.cpp include/Cala/backend/vulkan/ShaderProgram.h src/backend/vulkan/CommandBufferList.cpp include/Cala/backend/vulkan/CommandBufferList.h src/backend/vulkan/CommandBuffer.cpp include/Cala/backend/vulkan/CommandBuffer.h src/backend/vulkan/RenderPass.cpp include/Cala/backend/vulkan/RenderPass.h src/ImGuiContext.cpp include/Cala/ImGuiContext.h src/backend/vulkan/Image.cpp include/Cala/backend/vulkan/Image.h src/backend/vulkan/Framebuffer.cpp include/Cala/backend/vulkan/Framebuffer.h src/backend/vulkan/Buffer.cpp include/Cala/backend/vulkan/Buffer.h src/Camera.cpp include/Cala/Camera.h src/Transform.cpp include/Cala/Transform.h src/MeshData.cpp include/Cala/MeshData.h src/shapes.cpp include/Cala/shapes.h src/backend/vulkan/SDLPlatform.cpp include/Cala/backend/vulkan/SDLPlatform.h include/Cala/backend/vulkan/Platform.h src/Light.cpp include/Cala/Light.h src/Scene.cpp include/Cala/Scene.h src/Material.cpp include/Cala/Material.h src/MaterialInstance.cpp include/Cala/MaterialInstance.h src/backend/ShaderInterface.cpp include/Cala/backend/ShaderInterface.h src/backend/vulkan/Sampler.cpp include/Cala/backend/vulkan/Sampler.h include/Cala/backend/primitives.h include/Cala/backend/vulkan/primitives.h src/backend/vulkan/SamplerArray.cpp include/Cala/backend/vulkan/SamplerArray.h src/backend/vulkan/Timer.cpp include/Cala/backend/vulkan/Timer.h src/Mesh.cpp include/Cala/Mesh.h)

target_include_directories(Cala
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:install>
        PRIVATE
        src
        libs
        third_party)

target_link_libraries(Cala Ende ${Vulkan_LIBRARIES} ${SDL2_LIBRARIES} ${X11_LIBRARIES} imgui spirv-cross-cpp IMGUI)