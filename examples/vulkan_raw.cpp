
#include <iostream>

#include <Ende/platform.h>
#include <Ende/Vector.h>
#include <Ende/filesystem/File.h>
#include <Ende/math/Vec.h>
#include <Ende/time/StopWatch.h>
#include <Ende/time/time.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <SDL2/SDL_syswm.h>

#include <vulkan/vulkan.h>

#include <Cala/backend/vulkan/ShaderProgram.h>
#include <Cala/backend/vulkan/Driver.h>

#include <Cala/backend/vulkan/CommandBuffer.h>
#include <Cala/backend/vulkan/RenderPass.h>
#include <Cala/backend/vulkan/Image.h>
#include <Cala/backend/vulkan/Buffer.h>

#include <Cala/ImGuiContext.h>

using namespace cala::backend::vulkan;

struct Vertex {
    ende::math::Vec<2, f32> position;
    ende::math::Vec3f colour;
};

int main() {

    auto runTime = ende::time::SystemTime::now();

    bool renderImGui = true;

    auto window = SDL_CreateWindow("hello", 0, 0, 800, 600, SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN);

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(window, &wmInfo);

    u32 extensionCount = 0;
    SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, nullptr);
    ende::Vector<const char*> extensionNames(extensionCount);
    SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, extensionNames.data());

    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    ende::Vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
    for (const auto& extension : extensions)
        std::cout << '\t' << extension.extensionName << '\n';

    Driver driver(extensionNames, &wmInfo.info.x11.window, wmInfo.info.x11.display);

    ImGuiContext imGuiContext(driver, window);

    // cmd

    const Vertex vertices[] = {
            {{0.f, -0.5f}, {1.f, 0.f, 0.f}},
            {{0.5f, 0.5f}, {0.f, 1.f, 0.f}},
            {{-0.5f, 0.5f}, {0.f, 0.f, 1.f}}
    };
    Buffer vertexBuffer(driver._context, sizeof(vertices[0]) * 3, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vertexBuffer.data({vertices, sizeof(Vertex) * 3});

    Buffer uniformBuffer(driver._context, sizeof(ende::math::Vec3f), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    ende::math::Vec3f uniformData = {0.5, 0.5, 0.5};
    uniformBuffer.data({&uniformData, sizeof(uniformData)});

    // get shader data
    ende::fs::File shaderFile;
    if (!shaderFile.open("../../res/shaders/triangle.vert.spv"_path, ende::fs::in | ende::fs::binary))
        return -1;

    ende::Vector<u32> vertexShaderData(shaderFile.size() / sizeof(u32));
    shaderFile.read({reinterpret_cast<char*>(vertexShaderData.data()), static_cast<u32>(vertexShaderData.size() * sizeof(u32))});

    if (!shaderFile.open("../../res/shaders/triangle.frag.spv"_path, ende::fs::in | ende::fs::binary))
        return -2;
    ende::Vector<u32> fragmentShaderData(shaderFile.size() / sizeof(u32));
    shaderFile.read({reinterpret_cast<char*>(fragmentShaderData.data()), static_cast<u32>(fragmentShaderData.size() * sizeof(u32))});

    ShaderProgram program = ShaderProgram::create()
            .addStage(vertexShaderData, VK_SHADER_STAGE_VERTEX_BIT)
            .addStage(fragmentShaderData, VK_SHADER_STAGE_FRAGMENT_BIT)
            .compile(driver._context._device);

    //vertex array
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = 5 * sizeof(f32);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attributes[2] = {
            { .location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = 0 },
            { .location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = sizeof(f32) * 2 }
    };



    ende::time::StopWatch frameClock;
    frameClock.start();
    f32 dt = 1.f / 60.f;
    f32 fps = 0.f;
    ende::time::Duration frameTime;
    u64 frameCount = 0;

    CommandBuffer* cmd = nullptr;


    bool running = true;
    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
            }
        }

        driver._swapchain.wait();

        if (renderImGui) {

            imGuiContext.newFrame();
            ImGui::ShowDemoWindow();

            ImGui::Begin("Stats");

            ImGui::Text("FrameTime: %f", frameTime.microseconds() / 1000.f);
            ImGui::Text("FPS: %f", 1000.f / (frameTime.microseconds() / 1000.f));
            ImGui::Text("Command Buffers: %ld", driver._commands.count());
            ImGui::Text("Pipelines: %ld", cmd ? cmd->_pipelines.size() : 0);
            ImGui::Text("Descriptors: %ld", cmd ? cmd->_descriptorSets.size() : 0);
            ImGui::Text("Uptime: %ld sec", runTime.elapsed().seconds());
            ImGui::Text("Frame: %ld", frameCount);

            ImGui::End();


            ImGui::Render();
        }

        auto frame = driver._swapchain.nextImage();

        cmd = driver.beginFrame();
        {
            u32 i = frame.index;

            cmd->begin(frame.framebuffer);

            cmd->bindProgram(program);
            cmd->bindVertexArray({&binding, 1}, {attributes, 2});
            cmd->bindRasterState({});
            cmd->bindDepthState({});
            cmd->bindPipeline();

            cmd->bindBuffer(0, 0, uniformBuffer);
            cmd->bindDescriptors();

            cmd->bindVertexBuffer(0, vertexBuffer.buffer());
            cmd->draw(3, 1, 0, 0);

//            driver.draw({}, {vertexBuffer, 3});

            imGuiContext.render(*cmd);

            cmd->end(frame.framebuffer);

            cmd->submit(frame.imageAquired, frame.fence);
        }
        driver.endFrame();
        driver._swapchain.present(frame, cmd->signal());


        frameTime = frameClock.reset();
        dt = frameTime.milliseconds() / 1000.f;
        frameCount++;

//        if (frameCount > 10)
//            running = false;
    }

    driver._swapchain.wait();

    ende::util::MurmurHash<CommandBuffer::DescriptorKey> hasher;

    std::cout << sizeof(CommandBuffer::DescriptorKey) << '\n';
    std::cout << sizeof(CommandBuffer::DescriptorKey) / 4 << '\n';

    std::cout << "Current Descriptor Sets\n";
    for (u32 i = 0; i < SET_COUNT; i++) {
        std::cout << hasher(cmd->_descriptorKey[i]) << '\n';
    }

    std::cout << '\n';

    std::cout << "Stored Descriptor Sets\n";
    for (auto& [key, value] : cmd->_descriptorSets) {
        std::cout << hasher(key) << '\n';
    }


    std::cout << "\n\n\nCommand Buffers: " << driver._commands.count();
    std::cout << "\nPipelines: " << (cmd ? cmd->_pipelines.size() : 0);
    std::cout << "\nDescriptors: " << (cmd ? cmd->_descriptorSets.size() : 0);
    std::cout << "\nUptime: " << runTime.elapsed().seconds() << "sec";
    std::cout << "\nFrame: " << frameCount;

    SDL_DestroyWindow(window);

    return 0;
}