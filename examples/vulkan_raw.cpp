
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
#include <Cala/backend/vulkan/Sampler.h>

#include <Cala/backend/vulkan/SDLPlatform.h>

#include <Cala/ImGuiContext.h>

#include <Cala/Camera.h>
#include <Cala/shapes.h>
#include <Cala/Light.h>
#include <Cala/Scene.h>
#include <Cala/Material.h>

#include "../third_party/stb_image.h"

using namespace cala;
using namespace cala::backend::vulkan;


Image loadImage(Driver& driver, const ende::fs::Path& path) {
    i32 width, height, channels;
    u8* data = stbi_load((*path).c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!data) throw "unable load image";
    u32 length = width * height * 4;

    Image image(driver._context, {(u32)width, (u32)height, 1, backend::Format::RGBA8_SRGB});
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

    Scene scene;

    Transform cameraTransform({0, 0, -1});
    Camera camera(ende::math::perspective((f32)ende::math::rad(54.4), 800.f / -600.f, 0.1f, 1000.f), cameraTransform);

    Buffer cameraBuffer(driver._context, sizeof(Camera::Data), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    Transform model({0, 0, 0});

    Mesh vertices = cala::shapes::sphereUV();
    scene._renderables.push({Scene::Renderable{std::move(vertices.vertexBuffer(driver._context)), std::move(vertices.indexBuffer(driver._context))}, model});

    vertices = cala::shapes::sphereNormalized();
    model.addPos({1, 0, 0});
    scene._renderables.push({Scene::Renderable{std::move(vertices.vertexBuffer(driver._context)), std::move(vertices.indexBuffer(driver._context))}, model});
    model.addPos({-1, 0, 0});

    vertices = cala::shapes::sphereCube();
    model.addPos({-1, 0, 0});
    scene._renderables.push({Scene::Renderable{std::move(vertices.vertexBuffer(driver._context)), std::move(vertices.indexBuffer(driver._context))}, model});
    model.addPos({1, 0, 0});

    vertices = cala::shapes::icosahedron();
    model.addPos({-2, 0, 0});
    scene._renderables.push({Scene::Renderable{std::move(vertices.vertexBuffer(driver._context)), std::move(vertices.indexBuffer(driver._context))}, model});
    model.addPos({2, 0, 0});

    vertices = cala::shapes::cube(0.5);
    model.addPos({2, 0, 0});
    scene._renderables.push({Scene::Renderable{std::move(vertices.vertexBuffer(driver._context)), std::move(vertices.indexBuffer(driver._context))}, model});
    model.addPos({-2, 0, 0});

    vertices = cala::shapes::quad();
    model.addPos({0, 0, 0});
    scene._renderables.push({Scene::Renderable{std::move(vertices.vertexBuffer(driver._context)), std::move(vertices.indexBuffer(driver._context))}, model});
    vertices = cala::shapes::quad();
    model.addPos({0, 0, 1});
    scene._renderables.push({Scene::Renderable{std::move(vertices.vertexBuffer(driver._context)), std::move(vertices.indexBuffer(driver._context))}, model});
    vertices = cala::shapes::quad();
    model.rotate({1, 0, 0}, ende::math::rad(90.f));
    model.addPos({0.5, 0, 0});
    scene._renderables.push({Scene::Renderable{std::move(vertices.vertexBuffer(driver._context)), std::move(vertices.indexBuffer(driver._context))}, model});
    vertices = cala::shapes::quad();
    model.addPos({-1.5, 0, 0});
    scene._renderables.push({Scene::Renderable{std::move(vertices.vertexBuffer(driver._context)), std::move(vertices.indexBuffer(driver._context))}, model});

    // light marker
    vertices = cala::shapes::sphereNormalized(0.1);
    model.setPos({0, -1, 1});
    scene._renderables.push({Scene::Renderable{std::move(vertices.vertexBuffer(driver._context)), std::move(vertices.indexBuffer(driver._context))}, model});

    ende::Vector<ende::math::Mat4f> models;
    for (auto& transform : scene._renderables)
        models.push(transform.second.toMat());

    Buffer uniformBuffer(driver._context, sizeof(ende::math::Mat4f) * models.size(), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    uniformBuffer.data({models.data(), static_cast<u32>(models.size() * sizeof(ende::math::Mat4f))});

    PointLight light;
    light.position = {0, -1, 1};
    light.colour = {10, 10, 10};

    Buffer lightBuffer(driver._context, sizeof(light), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    lightBuffer.data({&light, sizeof(light)});

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

    if (!shaderFile.open("../../res/shaders/default.comp.spv"_path, ende::fs::in | ende::fs::binary))
        return -2;
    ende::Vector<u32> computeShaderData(shaderFile.size() / sizeof(u32));
    shaderFile.read({reinterpret_cast<char*>(computeShaderData.data()), static_cast<u32>(computeShaderData.size() * sizeof(u32))});

    ShaderProgram program = ShaderProgram::create()
            .addStage(vertexShaderData, VK_SHADER_STAGE_VERTEX_BIT)
            .addStage(fragmentShaderData, VK_SHADER_STAGE_FRAGMENT_BIT)
            .compile(driver);

    ShaderProgram computeProgram = ShaderProgram::create()
            .addStage(computeShaderData, VK_SHADER_STAGE_COMPUTE_BIT)
            .compile(driver);

    //vertex array
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = 14 * sizeof(f32);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    Attribute attributes[5] = {
            {0, 0, AttribType::Vec3f},
            {1, 0, AttribType::Vec3f},
            {2, 0, AttribType::Vec2f},
            {3, 0, AttribType::Vec3f},
            {4, 0, AttribType::Vec3f}
    };

    Sampler sampler(driver._context, {});

    Image brickwall = loadImage(driver, "../../res/textures/brickwall.jpg"_path);
    Image brickwall_normal = loadImage(driver, "../../res/textures/brickwall_normal.jpg"_path);
    Image brickwall_specular = loadImage(driver, "../../res/textures/brickwall_specular.jpg"_path);

    Image brickwall_copy(driver._context, {1024, 1024, 1, backend::Format::RGBA8_UNORM, 1, 1, backend::ImageUsage::STORAGE});
    auto brickwall_copy_view = brickwall_copy.getView();

    Material material(driver, std::move(program));
    material._rasterState = {
            backend::CullMode::BACK,
            backend::FrontFace::CCW,
            backend::PolygonMode::FILL
    };
//    material._rasterState = {.polygonMode=backend::PolygonMode::LINE, .cullMode=backend::CullMode::BACK};
    material._depthState = {true, true, VK_COMPARE_OP_LESS};

    auto matInstance = material.instance();

    matInstance.setSampler("diffuseMap", brickwall.getView());
    matInstance.setSampler("normalMap", brickwall_normal.getView());
    matInstance.setSampler("specularMap", brickwall_specular.getView());
    matInstance.setUniform("mixColour", ende::math::Vec3f{0.5, 1, 1.5});

    ende::time::StopWatch frameClock;
    f32 dt = 1.f / 60.f;
    f32 fps = 0.f;
    ende::time::Duration frameTime;
    u64 frameCount = 0;

    ende::time::Duration sumAvgFrameTime;
    ende::time::Duration avgFrameTime;
    ende::time::Duration maxDeviation;

    CommandBuffer* cmd = nullptr;

    bool running = true;
    bool lockMouse = false;
    SDL_Event event;
    frameClock.start();
    while (running) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_MOUSEMOTION:
                    if (lockMouse) {
                        ende::math::Vec<2, i32> delta = ende::math::Vec<2, i32>{event.motion.x, event.motion.y};
                        bool rotY = delta.x() != 0;
                        bool rotX = delta.y() != 0;
                        if (rotY)
                            cameraTransform.rotate({0, 1, 0}, ende::math::rad(delta.x() * 0.1f));
                        if (rotX)
                            cameraTransform.rotate(cameraTransform.rot().right(), ende::math::rad(delta.y() * 0.1f));
                        if (rotY || rotX)
                            SDL_WarpMouseInWindow(platform.window(), 400, 300);
                    }
                    break;
                case SDL_KEYDOWN:
                    switch (event.key.keysym.scancode) {
                        case SDL_SCANCODE_M:
                            lockMouse = !lockMouse;
                            break;
                    }
                    break;
            }
        }
        {
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_W])
                cameraTransform.addPos(cameraTransform.rot().front() * dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_S])
                cameraTransform.addPos(cameraTransform.rot().back() * dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_A])
                cameraTransform.addPos(cameraTransform.rot().left() * -dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_D])
                cameraTransform.addPos(cameraTransform.rot().right() * -dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_LSHIFT])
                cameraTransform.addPos(cameraTransform.rot().down() * dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_SPACE])
                cameraTransform.addPos(cameraTransform.rot().up() * dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_LEFT])
                cameraTransform.rotate({0, 1, 0}, ende::math::rad(45) * dt);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_RIGHT])
                cameraTransform.rotate({0, 1, 0}, ende::math::rad(-45) * dt);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_UP])
                cameraTransform.rotate(cameraTransform.rot().right(), ende::math::rad(-45) * dt);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_DOWN])
                cameraTransform.rotate(cameraTransform.rot().right(), ende::math::rad(45) * dt);
        }

        driver._swapchain.wait();

        if (renderImGui) {
            imGuiContext.newFrame();
            ImGui::ShowDemoWindow();

            ImGui::Begin("Stats");

            ImGui::Text("FrameTime: %f", frameTime.microseconds() / 1000.f);
            ImGui::Text("Average FrameTime: %f", avgFrameTime.microseconds() / 1000.f);
            ImGui::Text("Sum Average FrameTime: %f", sumAvgFrameTime.microseconds() / 1000.f);
            ImGui::Text("Max Deviation FrameTime: %f", maxDeviation.microseconds() / 1000.f);
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
        }

        auto frame = driver._swapchain.nextImage();

        cmd = driver.beginFrame();
        {
            VkImageMemoryBarrier barriers[] = { brickwall_copy.barrier(VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL) };
            cmd->pipelineBarrier(VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, barriers);

            cmd->clearDescriptors();
            cmd->bindProgram(computeProgram);
//            cmd->bindImage(0, 0, brickwall_view.view, sampler.sampler());
            cmd->bindImage(0, 0, brickwall_copy_view.view, sampler.sampler(), true);
            cmd->bindPipeline();
            cmd->bindDescriptors();
            cmd->dispatchCompute(1024 / 16, 1024 / 16, 1);

            barriers[0] = brickwall_copy.barrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL);
            cmd->pipelineBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, 0, barriers);

            cmd->begin(frame.framebuffer);

            cmd->bindProgram(material._program);
            cmd->bindBindings({&binding, 1});
            cmd->bindAttributes(attributes);
            cmd->bindRasterState(material._rasterState);
            cmd->bindDepthState(material._depthState);
            cmd->bindPipeline();

            cmd->bindBuffer(0, 0, cameraBuffer);

            for (u32 i = 0; i < matInstance._samplers.size(); i++) {
                cmd->bindImage(2, i, matInstance._samplers[i].view, sampler.sampler());
            }
            cmd->bindBuffer(2, 3, matInstance.material()->_uniformBuffer);
            cmd->bindBuffer(3, 0, lightBuffer);

            u32 i = 0;
            for (auto& renderable : scene._renderables) {
                cmd->bindBuffer(1, 0, uniformBuffer, sizeof(ende::math::Mat4f) * i++, sizeof(ende::math::Mat4f));
                cmd->bindDescriptors();
                cmd->bindVertexBuffer(0, renderable.first.vertex.buffer());
                cmd->bindIndexBuffer(renderable.first.index);
                cmd->draw(renderable.first.index.size() / sizeof(u32), 1, 0, 0);
            }

            imGuiContext.render(*cmd);

            cmd->end(frame.framebuffer);
            cmd->submit(frame.imageAquired, frame.fence);
        }
        driver.endFrame();
        driver._swapchain.present(frame, cmd->signal());

        frameTime = frameClock.reset();
        dt = frameTime.milliseconds() / 1000.f;
        if (frameCount == 0) avgFrameTime = frameTime;
        auto deviation = avgFrameTime - frameTime;
        if (deviation < 0)
            deviation = deviation * -1;
        maxDeviation = std::max(maxDeviation, deviation);
        sumAvgFrameTime += frameTime;
        if (frameCount % 100 == 0) {
            avgFrameTime = sumAvgFrameTime / 100;
            sumAvgFrameTime = 0;
        }

        frameCount++;

        if (cmd->_descriptorSets.size() > 700)
            running = false;
    }

    driver._swapchain.wait();

    std::cout << "\n\nCommand Buffers: " << driver._commands.count();
    std::cout << "\nPipelines: " << (cmd ? cmd->_pipelines.size() : 0);
    std::cout << "\nDescriptors: " << (cmd ? cmd->_descriptorSets.size() : 0);
    std::cout << "\nRenderables: " << scene._renderables.size();
    std::cout << "\nUptime: " << runTime.elapsed().seconds() << "sec";
    std::cout << "\nFrame: " << frameCount;

    return 0;
}