
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

    std::array<RenderPass::Attachment, 2> attachments = {
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
            },
            RenderPass::Attachment{
                VK_FORMAT_D32_SFLOAT,
                VK_SAMPLE_COUNT_1_BIT,
                VK_ATTACHMENT_LOAD_OP_CLEAR,
                VK_ATTACHMENT_STORE_OP_STORE,
                VK_ATTACHMENT_LOAD_OP_CLEAR,
                VK_ATTACHMENT_STORE_OP_DONT_CARE,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
            }
    };
    RenderPass renderPass(driver._context._device, attachments);


    Image depthImage(driver._context, {
        800, 600, 1, VK_FORMAT_D32_SFLOAT, 1, 1, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
    });


    Image::View depthView = depthImage.getView(VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_DEPTH_BIT);

    ende::Vector<VkFramebuffer> swapchainFramebuffers;

    for (u32 i = 0; i < driver._swapchain.size(); i++) {
        VkImageView fbAttachments[2] = {driver._swapchain.view(i), depthView.view};
        swapchainFramebuffers.push(renderPass.framebuffer(fbAttachments, driver._swapchain.extent().width, driver._swapchain.extent().height));
    }


    VkQueue graphicsQueue = driver._context.getQueue(VK_QUEUE_GRAPHICS_BIT);


    ende::time::StopWatch frameClock;
    frameClock.start();
    f32 dt = 1.f / 60.f;
    f32 fps = 0.f;
    ende::time::Duration frameTime;
    u64 frameCount = 0;

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
            ImGui::Text("Command Buffers: %ld", driver._commands.count());
            ImGui::Text("Pipelines: %ld", buffer ? buffer->_pipelines.size() : 0);
            ImGui::Text("Descriptors: %ld", buffer ? buffer->_descriptorSets.size() : 0);
            ImGui::Text("Uptime: %ld sec", runTime.elapsed().seconds());
            ImGui::Text("Frame: %ld", frameCount);

            ImGui::End();


            ImGui::Render();
        }

        auto frame = driver._swapchain.nextImage();

//        buffer = driver._context._commands->get();
        buffer = driver.beginFrame();
        {
            u32 i = frame.index;

            buffer->begin(renderPass, swapchainFramebuffers[i], {driver._swapchain.extent().width, driver._swapchain.extent().height});

            buffer->bindProgram(program);
            buffer->bindVertexArray({&binding, 1}, {attributes, 2});
            buffer->bindRasterState({});
            buffer->bindDepthState({});
            buffer->bindPipeline();

            buffer->bindBuffer(0, 0, uniformBuffer, 0, sizeof(ende::math::Vec3f));
            buffer->bindDescriptors();

            buffer->bindVertexBuffer(0, vertexBuffer);
            buffer->draw(3, 1, 0, 0);

//            driver.draw({}, {vertexBuffer, 3});

            imGuiContext.render(*buffer);

            buffer->end(renderPass);

            buffer->submit(frame.imageAquired, driver._swapchain.fence());
            driver.endFrame();
        }

        driver._swapchain.present(frame, buffer->signal());


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
        std::cout << hasher(buffer->_descriptorKey[i]) << '\n';
    }

    std::cout << '\n';

    std::cout << "Stored Descriptor Sets\n";
    for (auto& [key, value] : buffer->_descriptorSets) {
        std::cout << hasher(key) << '\n';
    }


    std::cout << "\n\n\nCommand Buffers: " << driver._commands.count();
    std::cout << "\nPipelines: " << buffer ? buffer->_pipelines.size() : 0;
    std::cout << "\nDescriptors: " << buffer ? buffer->_descriptorSets.size() : 0;
    std::cout << "\nUptime: " << runTime.elapsed().seconds() << "sec";
    std::cout << "\nFrame: " << frameCount;


    vkDestroyBuffer(driver._context._device, uniformBuffer, nullptr);
    vkFreeMemory(driver._context._device, uniformBufferMemory, nullptr);

    vkDestroyBuffer(driver._context._device, vertexBuffer, nullptr);
    vkFreeMemory(driver._context._device, vertexBufferMemory, nullptr);

    for (auto& framebuffer : swapchainFramebuffers)
        vkDestroyFramebuffer(driver._context._device, framebuffer, nullptr);

    SDL_DestroyWindow(window);

    return 0;
}