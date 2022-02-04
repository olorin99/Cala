
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
#include <Cala/backend/vulkan/Buffer.h>

#include <Cala/backend/vulkan/SDLPlatform.h>

#include <Cala/ImGuiContext.h>

#include <Cala/Camera.h>
#include <Cala/shapes.h>


#include "../third_party/stb_image.h"

using namespace cala;
using namespace cala::backend::vulkan;


Image loadImage(Driver& driver, const ende::fs::Path& path) {
    i32 width, height, channels;
    u8* data = stbi_load((*path).c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!data) throw "unable load image";
    u32 length = width * height * 4;

    Image image(driver._context, {(u32)width, (u32)height, 1, VK_FORMAT_R8G8B8A8_SRGB});
    image.data(driver, {0, (u32)width, (u32)height, 1, 4, {data, length}});
    stbi_image_free(data);
    return image;
}


int main() {
    auto runTime = ende::time::SystemTime::now();

    bool renderImGui = true;

    SDLPlatform platform("Hello", 800, 600);

    Driver driver(platform);
    ImGuiContext imGuiContext(driver, platform.window());



    Transform cameraTransform({0, 0, -1});
    Camera camera(ende::math::perspective(45.f, 800.f / -600.f, 0.f, 1000.f), cameraTransform);

    Buffer cameraBuffer(driver._context, sizeof(Camera::Data), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    Mesh vertices = cala::shapes::sphere();
    Buffer vertexBuffer = vertices.vertexBuffer(driver._context);
    Buffer indexBuffer = vertices.indexBuffer(driver._context);

    Transform model({0, 0, 0});
    Buffer uniformBuffer(driver._context, sizeof(ende::math::Mat4f), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    // get shader data
    ende::fs::File shaderFile;
    if (!shaderFile.open("../../res/shaders/default.vert.spv"_path, ende::fs::in | ende::fs::binary))
        return -1;

    ende::Vector<u32> vertexShaderData(shaderFile.size() / sizeof(u32));
    shaderFile.read({reinterpret_cast<char*>(vertexShaderData.data()), static_cast<u32>(vertexShaderData.size() * sizeof(u32))});

    if (!shaderFile.open("../../res/shaders/default.frag.spv"_path, ende::fs::in | ende::fs::binary))
        return -2;
    ende::Vector<u32> fragmentShaderData(shaderFile.size() / sizeof(u32));
    shaderFile.read({reinterpret_cast<char*>(fragmentShaderData.data()), static_cast<u32>(fragmentShaderData.size() * sizeof(u32))});

    ShaderProgram program = ShaderProgram::create()
            .addStage(vertexShaderData, VK_SHADER_STAGE_VERTEX_BIT)
            .addStage(fragmentShaderData, VK_SHADER_STAGE_FRAGMENT_BIT)
            .compile(driver);

    //vertex array
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = 14 * sizeof(f32);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attributes[5] = {
            { .location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 0 },
            { .location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = sizeof(f32) * 3 },
            { .location = 2, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = sizeof(f32) * 6 },
            { .location = 3, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = sizeof(f32) * 8 },
            { .location = 4, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = sizeof(f32) * 11 }
    };


    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 0;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.f;
    samplerInfo.minLod = 0.f;
    samplerInfo.maxLod = 0.f;

    VkSampler sampler;
    vkCreateSampler(driver._context._device, &samplerInfo, nullptr, &sampler);


    Image image = loadImage(driver, "../../res/textures/metal-sheet.jpg"_path);
    Image::View view = image.getView(VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT);



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
        {
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_W])
                cameraTransform.addPos(cameraTransform.rot().front() * dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_S])
                cameraTransform.addPos(cameraTransform.rot().back() * dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_A])
                cameraTransform.addPos(cameraTransform.rot().left() * dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_D])
                cameraTransform.addPos(cameraTransform.rot().right() * dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_LSHIFT])
                cameraTransform.addPos(cameraTransform.rot().down() * dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_SPACE])
                cameraTransform.addPos(cameraTransform.rot().up() * dt * 10);
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
            ImGui::Text("Set Layouts: %ld", driver._setLayouts.size());
            ImGui::Text("Uptime: %ld sec", runTime.elapsed().seconds());
            ImGui::Text("Frame: %ld", frameCount);

            ImGui::End();


            ImGui::Render();
        }

        {
            auto cameraData = camera.data();
            cameraBuffer.data({&cameraData, sizeof(cameraData)});

            auto modelData = model.toMat();
            uniformBuffer.data({&modelData, sizeof(modelData)});
        }

        auto frame = driver._swapchain.nextImage();

        cmd = driver.beginFrame();
        {
            u32 i = frame.index;

            cmd->begin(frame.framebuffer);

            cmd->bindProgram(program);
            cmd->bindVertexArray({&binding, 1}, {attributes, 5});
            cmd->bindRasterState({});
            cmd->bindDepthState({true, true, VK_COMPARE_OP_LESS_OR_EQUAL});
            cmd->bindPipeline();

            cmd->bindBuffer(0, 0, cameraBuffer);
            cmd->bindBuffer(1, 0, uniformBuffer);
            cmd->bindImage(2, 0, view.view, sampler);
            cmd->bindDescriptors();

            cmd->bindVertexBuffer(0, vertexBuffer.buffer());
            cmd->bindIndexBuffer(indexBuffer);
//            cmd->draw(vertexBuffer.size() / sizeof(Vertex), 1, 0, 0);
            cmd->draw(indexBuffer.size() / sizeof(u32), 1, 0, 0);

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


        if (cmd->_descriptorSets.size() > 700)
            running = false;
    }

    driver._swapchain.wait();

    std::cout << "\n\nCommand Buffers: " << driver._commands.count();
    std::cout << "\nPipelines: " << (cmd ? cmd->_pipelines.size() : 0);
    std::cout << "\nDescriptors: " << (cmd ? cmd->_descriptorSets.size() : 0);
    std::cout << "\nUptime: " << runTime.elapsed().seconds() << "sec";
    std::cout << "\nFrame: " << frameCount;

    vkDestroySampler(driver._context._device, sampler, nullptr);
    return 0;
}