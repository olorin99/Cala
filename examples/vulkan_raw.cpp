
#include <iostream>

#include <Ende/platform.h>
#include <Ende/Vector.h>
#include <Ende/filesystem/File.h>
#include <Ende/math/Vec.h>
#include <Ende/time/StopWatch.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <SDL2/SDL_syswm.h>

#include <vulkan/vulkan.h>

#include <Cala/backend/vulkan/ShaderProgram.h>
#include <Cala/backend/vulkan/Driver.h>

#include <Cala/backend/vulkan/CommandBuffer.h>
#include <Cala/backend/vulkan/RenderPass.h>

#include <Cala/ImGuiContext.h>

using namespace cala::backend::vulkan;

struct Vertex {
    ende::math::Vec<2, f32> position;
    ende::math::Vec3f colour;
};

int main() {

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

    // buffer

    const Vertex vertices[] = {
            {{0.f, -0.5f}, {1.f, 0.f, 0.f}},
            {{0.5f, 0.5f}, {0.f, 1.f, 0.f}},
            {{-0.5f, 0.5f}, {0.f, 0.f, 1.f}}
    };

    auto [vertexBuffer, vertexBufferMemory] = driver._context.createBuffer(sizeof(vertices[0]) * 3, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void* data;
    vkMapMemory(driver._context._device, vertexBufferMemory, 0, sizeof(vertices[0]) * 3, 0, &data);
    memcpy(data, vertices, sizeof(vertices[0]) * 3);
    vkUnmapMemory(driver._context._device, vertexBufferMemory);


    auto [uniformBuffer, uniformBufferMemory] = driver._context.createBuffer(sizeof(ende::math::Vec3f), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    void* uniformData;
    vkMapMemory(driver._context._device, uniformBufferMemory, 0, sizeof(ende::math::Vec3f), 0, &uniformData);
    *static_cast<ende::math::Vec3f*>(uniformData) = {0.5, 0.5, 0.5};
    vkUnmapMemory(driver._context._device, uniformBufferMemory);

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

    std::array<RenderPass::Attachment, 1> attachments = {
            RenderPass::Attachment{
                    driver._swapchain.format(),
                    VK_SAMPLE_COUNT_1_BIT,
                    VK_ATTACHMENT_LOAD_OP_CLEAR,
                    VK_ATTACHMENT_STORE_OP_STORE,
                    VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                    VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            }
    };
    RenderPass renderPass(driver._context._device, attachments);


    ende::Vector<VkFramebuffer> swapchainFramebuffers;

    for (u32 i = 0; i < driver._swapchain.size(); i++) {
        swapchainFramebuffers.push(renderPass.framebuffer(driver._swapchain.view(i), driver._swapchain.extent().width, driver._swapchain.extent().height));
    }


    VkQueue graphicsQueue = driver._context.getQueue(VK_QUEUE_GRAPHICS_BIT);


    ende::time::StopWatch frameClock;
    frameClock.start();
    f32 dt = 1.f / 60.f;
    f32 fps = 0.f;
    ende::time::Duration frameTime;

    CommandBuffer* buffer = nullptr;


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
            ImGui::Text("Pipelines: %ld", buffer ? buffer->_pipelines.size() : 0);
            ImGui::Text("Descriptors: %ld", buffer ? buffer->_descriptorSets.size() : 0);

            ImGui::End();


            ImGui::Render();
        }

        auto frame = driver._swapchain.nextImage();

        buffer = driver._context._commands->get();
        {
            u32 i = frame.index;

            buffer->begin(renderPass, swapchainFramebuffers[i], {driver._swapchain.extent().width, driver._swapchain.extent().height});

            buffer->bindProgram(program);
            buffer->bindVertexArray({&binding, 1}, {attributes, 2});
            buffer->bindRasterState({});
            buffer->bindPipeline();

            buffer->bindBuffer(0, 0, uniformBuffer, 0, sizeof(ende::math::Vec3f));
            buffer->bindDescriptors();

            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(buffer->buffer(), 0, 1, &vertexBuffer, offsets);

            vkCmdDraw(buffer->buffer(), 3, 1, 0, 0);

//            driver.draw({}, {vertexBuffer, 3});

            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), buffer->buffer());

            buffer->end(renderPass);

            buffer->submit(frame.imageAquired, driver._swapchain.fence());
            driver._context._commands->flush();
        }

        driver._swapchain.present(frame, buffer->signal());


        frameTime = frameClock.reset();
        dt = frameTime.milliseconds() / 1000.f;
    }

    driver._swapchain.wait();


    vkDestroyBuffer(driver._context._device, uniformBuffer, nullptr);
    vkFreeMemory(driver._context._device, uniformBufferMemory, nullptr);

    vkDestroyBuffer(driver._context._device, vertexBuffer, nullptr);
    vkFreeMemory(driver._context._device, vertexBufferMemory, nullptr);

    for (auto& framebuffer : swapchainFramebuffers)
        vkDestroyFramebuffer(driver._context._device, framebuffer, nullptr);

    SDL_DestroyWindow(window);

    return 0;
}