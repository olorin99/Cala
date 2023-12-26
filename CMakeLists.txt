cmake_minimum_required(VERSION 3.20)
project(Cala)

set(CMAKE_CXX_STANDARD 23)

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DIMGUI_IMPL_VULKAN_NO_PROTOTYPES")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DENDE_PROFILE -DIMGUI_IMPL_VULKAN_NO_PROTOTYPES")

#add_compile_options(-fsanitize=address)
#add_link_options(-fsanitize=address)

find_package(SDL2 REQUIRED)

add_subdirectory(third_party)
add_subdirectory(examples)

add_library(Cala src/vulkan/Context.cpp include/Cala/vulkan/Context.h src/vulkan/Swapchain.cpp include/Cala/vulkan/Swapchain.h src/vulkan/Device.cpp include/Cala/vulkan/Device.h src/vulkan/ShaderProgram.cpp include/Cala/vulkan/ShaderProgram.h src/vulkan/CommandBuffer.cpp include/Cala/vulkan/CommandBuffer.h src/vulkan/RenderPass.cpp include/Cala/vulkan/RenderPass.h src/ImGuiContext.cpp include/Cala/ImGuiContext.h src/vulkan/Image.cpp include/Cala/vulkan/Image.h src/vulkan/Framebuffer.cpp include/Cala/vulkan/Framebuffer.h src/vulkan/Buffer.cpp include/Cala/vulkan/Buffer.h src/Camera.cpp include/Cala/Camera.h src/Transform.cpp include/Cala/Transform.h src/MeshData.cpp include/Cala/MeshData.h src/shapes.cpp include/Cala/shapes.h src/vulkan/SDLPlatform.cpp include/Cala/vulkan/SDLPlatform.h include/Cala/vulkan/Platform.h src/Light.cpp include/Cala/Light.h src/Scene.cpp include/Cala/Scene.h src/Material.cpp include/Cala/Material.h src/MaterialInstance.cpp include/Cala/MaterialInstance.h src/vulkan/ShaderInterface.cpp include/Cala/vulkan/ShaderInterface.h src/vulkan/Sampler.cpp include/Cala/vulkan/Sampler.h include/Cala/vulkan/primitives.h src/vulkan/Timer.cpp include/Cala/vulkan/Timer.h src/Mesh.cpp include/Cala/Mesh.h src/Engine.cpp include/Cala/Engine.h src/Renderer.cpp include/Cala/Renderer.h src/Probe.cpp include/Cala/Probe.h src/RenderGraph.cpp include/Cala/RenderGraph.h src/Model.cpp include/Cala/Model.h include/Cala/ui/Window.h src/ui/ProfileWindow.cpp include/Cala/ui/ProfileWindow.h src/ui/StatisticsWindow.cpp include/Cala/ui/StatisticsWindow.h src/ui/RendererSettings.cpp include/Cala/ui/RendererSettingsWindow.h src/ui/LightWindow.cpp include/Cala/ui/LightWindow.h src/vulkan/CommandPool.cpp include/Cala/vulkan/CommandPool.h src/ui/ResourceViewer.cpp include/Cala/ui/ResourceViewer.h src/vulkan/OfflinePlatform.cpp include/Cala/vulkan/OfflinePlatform.h src/vulkan/Handle.cpp include/Cala/vulkan/Handle.h
        src/renderPasses/debugPasses.cpp
        src/renderPasses/debugPasses.h
        src/ui/RenderGraphViewer.cpp
        include/Cala/ui/RenderGraphViewer.h
        src/vulkan/Semaphore.cpp
        include/Cala/vulkan/Semaphore.h
        src/ui/ImageView.cpp
        include/Cala/ui/ImageView.h
        src/ui/GuiWindow.cpp
        include/Cala/ui/GuiWindow.h
        src/AssetManager.cpp
        include/Cala/AssetManager.h
        src/vulkan/ShaderModule.cpp
        include/Cala/vulkan/ShaderModule.h
        src/util.cpp
        include/Cala/util.h
        src/vulkan/ShaderModuleInterface.cpp
        include/Cala/vulkan/ShaderModuleInterface.h
        src/ui/AssetManagerWindow.cpp
        include/Cala/ui/AssetManagerWindow.h
        src/vulkan/PipelineLayout.cpp
        include/Cala/vulkan/PipelineLayout.h
        src/renderPasses/shadowPasses.cpp
        src/renderPasses/shadowPasses.h
        src/ui/RenderGraphWindow.cpp
        include/Cala/ui/RenderGraphWindow.h
        src/ui/BackbufferWindow.cpp
        include/Cala/ui/BackbufferWindow.h
        src/ui/SceneGraphWindow.cpp
        include/Cala/ui/SceneGraphWindow.h
        include/Cala/shaderBridge.h)

target_include_directories(Cala
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:install>
        third_party
        PRIVATE
        src
        third_party)

target_link_libraries(Cala Ende ${SDL2_LIBRARIES} ${X11_LIBRARIES} imgui node_editor implot assimp spirv-cross-cpp shaderc_combined volk fastgltf stb_image stb_image_write meshoptimizer)