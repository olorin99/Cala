
#include <iostream>

#include <Ende/platform.h>
#include <Ende/Vector.h>
#include <Ende/filesystem/File.h>
#include <Ende/math/Vec.h>
#include <Ende/time/StopWatch.h>
#include <Ende/time/time.h>
#include <Ende/profile/profile.h>
#include <Ende/profile/ProfileManager.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <SDL2/SDL_syswm.h>

#include <vulkan/vulkan.h>

#include <Cala/backend/vulkan/ShaderProgram.h>
#include <Cala/backend/vulkan/Driver.h>

#include <Cala/backend/vulkan/CommandBuffer.h>
#include <Cala/backend/vulkan/Buffer.h>
#include <Cala/backend/vulkan/Sampler.h>
#include <Cala/backend/vulkan/Timer.h>
#include <Cala/backend/vulkan/SDLPlatform.h>
#include <Cala/Mesh.h>
#include <Cala/ImGuiContext.h>

#include <Cala/Camera.h>
#include <Cala/shapes.h>
#include <Cala/Light.h>
#include <Cala/Scene.h>
#include <Cala/Material.h>

#include "../third_party/stb_image.h"

#include <Stats.h>

using namespace cala;
using namespace cala::backend;
using namespace cala::backend::vulkan;

ShaderProgram loadShader(Driver& driver, const ende::fs::Path& vertex, const ende::fs::Path& fragment) {
    ende::fs::File shaderFile;
    shaderFile.open(vertex, ende::fs::in | ende::fs::binary);

    ende::Vector<u32> vertexData(shaderFile.size() / sizeof(u32));
    shaderFile.read({reinterpret_cast<char*>(vertexData.data()), static_cast<u32>(vertexData.size() * sizeof(u32))});

    shaderFile.open(fragment, ende::fs::in | ende::fs::binary);

    ende::Vector<u32> fragmentData(shaderFile.size() / sizeof(u32));
    shaderFile.read({reinterpret_cast<char*>(fragmentData.data()), static_cast<u32>(fragmentData.size() * sizeof(u32))});

    return ShaderProgram::create()
            .addStage(vertexData, ShaderStage::VERTEX)
            .addStage(fragmentData, ShaderStage::FRAGMENT)
            .compile(driver);
}

Image loadImage(Driver& driver, const ende::fs::Path& path) {
    i32 width, height, channels;
    u8* data = stbi_load((*path).c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!data) throw "unable load image";
    u32 length = width * height * 4;

    Image image(driver, {(u32)width, (u32)height, 1, backend::Format::RGBA8_SRGB});
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

    Scene scene(driver, 10);

    Transform cameraTransform({0, 0, 10});
    Camera camera(ende::math::perspective((f32)ende::math::rad(54.4), 800.f / -600.f, 0.1f, 1000.f), cameraTransform);
    Buffer cameraBuffer(driver, sizeof(Camera::Data), BufferUsage::UNIFORM, MemoryProperties::HOST_VISIBLE | MemoryProperties::HOST_COHERENT);

    Transform model({0, 0, 0});

    MeshData vertices = cala::shapes::sphereUV();

    Mesh sphereUV = shapes::sphereUV().mesh(driver);
    Mesh sphereNormalized = shapes::sphereNormalized().mesh(driver);

    //scene._renderables.push({Scene::Renderable{std::move(vertices.vertexBuffer(driver)), std::move(vertices.indexBuffer(driver))}, model});



    //vertices = cala::shapes::sphereNormalized();
    /*model.addPos({1, 0, 0});
    //scene._renderables.push({Scene::Renderable{std::move(vertices.vertexBuffer(driver)), std::move(vertices.indexBuffer(driver))}, model});
    model.addPos({-1, 0, 0});

    vertices = cala::shapes::sphereCube();
    model.addPos({-1, 0, 0});
    scene._renderables.push({Scene::Renderable{std::move(vertices.vertexBuffer(driver)), std::move(vertices.indexBuffer(driver))}, model});
    model.addPos({1, 0, 0});

    vertices = cala::shapes::icosahedron();
    model.addPos({-2, 0, 0});
    scene._renderables.push({Scene::Renderable{std::move(vertices.vertexBuffer(driver)), std::move(vertices.indexBuffer(driver))}, model});
    model.addPos({2, 0, 0});

    vertices = cala::shapes::cube(0.5);
    model.addPos({2, 0, 0});
    scene._renderables.push({Scene::Renderable{std::move(vertices.vertexBuffer(driver)), std::move(vertices.indexBuffer(driver))}, model});
    model.addPos({-2, 0, 0});

    vertices = cala::shapes::quad();
    model.addPos({0, 0, 0});
    scene._renderables.push({Scene::Renderable{std::move(vertices.vertexBuffer(driver)), std::move(vertices.indexBuffer(driver))}, model});
    vertices = cala::shapes::quad();
    model.addPos({0, 0, 1});
    scene._renderables.push({Scene::Renderable{std::move(vertices.vertexBuffer(driver)), std::move(vertices.indexBuffer(driver))}, model});
    vertices = cala::shapes::quad();
    model.rotate({1, 0, 0}, ende::math::rad(90.f));
    model.addPos({0.5, 0, 0});
    scene._renderables.push({Scene::Renderable{std::move(vertices.vertexBuffer(driver)), std::move(vertices.indexBuffer(driver))}, model});
    vertices = cala::shapes::quad();
    model.addPos({-1.5, 0, 0});
    scene._renderables.push({Scene::Renderable{std::move(vertices.vertexBuffer(driver)), std::move(vertices.indexBuffer(driver))}, model});

    // light marker
    vertices = cala::shapes::sphereNormalized(0.1);
    model.setPos({0, -1, 1});
    scene._renderables.push({Scene::Renderable{std::move(vertices.vertexBuffer(driver)), std::move(vertices.indexBuffer(driver))}, model});

    f32 quadDistance = 2.f;
    vertices = cala::shapes::quad();
    model.setPos(cameraTransform.pos() + ende::math::Vec3f{0, 0, quadDistance});
    model.setRot({0, 0, 0, 1});
    scene._renderables.push({Scene::Renderable{std::move(vertices.vertexBuffer(driver)), std::move(vertices.indexBuffer(driver))}, model});*/

    //ende::Vector<ende::math::Mat4f> models;
    //for (auto& transform : scene._renderables)
    //    models.push(transform.second.toMat());

    //Buffer uniformBuffer(driver, sizeof(ende::math::Mat4f) * models.size(), BufferUsage::UNIFORM, MemoryProperties::HOST_VISIBLE | MemoryProperties::HOST_COHERENT);
    //uniformBuffer.data({models.data(), static_cast<u32>(models.size() * sizeof(ende::math::Mat4f))});

    PointLight light;
    light.position = {0, -1, 1};
    light.colour = {10, 10, 10};

    Buffer lightBuffer(driver, sizeof(light), BufferUsage::UNIFORM, MemoryProperties::HOST_VISIBLE | MemoryProperties::HOST_COHERENT);
    lightBuffer.data({&light, sizeof(light)});

    // get shader data
    ende::fs::File shaderFile;
    if (!shaderFile.open("../../res/shaders/default.comp.spv"_path, ende::fs::in | ende::fs::binary))
        return -2;
    ende::Vector<u32> computeShaderData(shaderFile.size() / sizeof(u32));
    shaderFile.read({reinterpret_cast<char*>(computeShaderData.data()), static_cast<u32>(computeShaderData.size() * sizeof(u32))});

    ShaderProgram program = loadShader(driver, "../../res/shaders/default.vert.spv"_path, "../../res/shaders/default.frag.spv"_path);
    ShaderProgram triangleProgram = loadShader(driver, "../../res/shaders/triangle.vert.spv"_path, "../../res/shaders/triangle.frag.spv"_path);

    ShaderProgram computeProgram = ShaderProgram::create()
            .addStage(computeShaderData, ShaderStage::COMPUTE)
            .compile(driver);

    //vertex array
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = 14 * sizeof(f32);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; //TODO: enumify

    Attribute attributes[5] = {
            {0, 0, AttribType::Vec3f},
            {1, 0, AttribType::Vec3f},
            {2, 0, AttribType::Vec2f},
            {3, 0, AttribType::Vec3f},
            {4, 0, AttribType::Vec3f}
    };

    Sampler sampler(driver, {});

    Image brickwall = loadImage(driver, "../../res/textures/brickwall.jpg"_path);
    Image brickwall_normal = loadImage(driver, "../../res/textures/brickwall_normal.jpg"_path);
    Image brickwall_specular = loadImage(driver, "../../res/textures/brickwall_specular.jpg"_path);

    Image brickwall_copy(driver, {1024, 1024, 1, Format::RGBA8_UNORM, 1, 1, ImageUsage::STORAGE});
    auto brickwall_copy_view = brickwall_copy.getView();

    Material material(driver, std::move(program));
    material._rasterState = {
            CullMode::BACK,
            FrontFace::CCW,
            PolygonMode::FILL
    };
    material._depthState = {true, true};

    auto matInstance = material.instance();

    matInstance.setSampler("diffuseMap", brickwall.getView(), Sampler(driver, {}));
    matInstance.setSampler("normalMap", brickwall_normal.getView(), Sampler(driver, {}));
    matInstance.setSampler("specularMap", brickwall_specular.getView(), Sampler(driver, {}));
    matInstance.setUniform("mixColour", ende::math::Vec3f{0.5, 1, 1.5});

    scene.addRenderable(sphereUV, &matInstance, &model);

    Transform model1({1, 1, 1});
    scene.addRenderable(sphereNormalized, &matInstance, &model1);



    MeshData triangle = cala::shapes::triangle();
    Buffer triangleVertexBuffer = triangle.vertexBuffer(driver);


    Image frameCopy(driver, {
        driver.swapchain().extent().width,
        driver.swapchain().extent().height,
        1,
        driver.swapchain().format(),
        1,
        1,
        ImageUsage::COLOUR_ATTACHMENT | ImageUsage::TRANSFER_DST
    });

    imgui::Stats statWindow;


    ende::time::StopWatch frameClock;
    f32 dt = 1.f / 60.f;
    f32 fps = 0.f;
    ende::time::Duration frameTime;
    u64 frameCount = 0;

    ende::time::Duration sumAvgFrameTime;
    ende::time::Duration avgFrameTime;
    ende::time::Duration maxDeviation;

    u64 gpuTime = 0;

    CommandBuffer* cmd = nullptr;

    Timer timer(driver);

    u32 prevFrame = 0;
    bool running = true;
    bool lockMouse = false;
    SDL_Event event;
    frameClock.start();
    while (running) {
        ende::profile::ProfileManager::beginFrame(frameCount);
        PROFILE_NAMED("MAIN_LOOP")
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
                cameraTransform.addPos(cameraTransform.rot().invertY().front() * dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_S])
                cameraTransform.addPos(cameraTransform.rot().invertY().back() * dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_A])
                cameraTransform.addPos(cameraTransform.rot().invertY().left() * -dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_D])
                cameraTransform.addPos(cameraTransform.rot().invertY().right() * -dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_LSHIFT])
                cameraTransform.addPos(cameraTransform.rot().invertY().down() * dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_SPACE])
                cameraTransform.addPos(cameraTransform.rot().invertY().up() * dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_LEFT])
                cameraTransform.rotate({0, 1, 0}, ende::math::rad(45) * dt);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_RIGHT])
                cameraTransform.rotate({0, 1, 0}, ende::math::rad(-45) * dt);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_UP])
                cameraTransform.rotate(cameraTransform.rot().right(), ende::math::rad(45) * dt);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_DOWN])
                cameraTransform.rotate(cameraTransform.rot().right(), ende::math::rad(-45) * dt);
        }

        driver.swapchain().wait();
        //driver.swapchain().copyFrameToImage(prevFrame, frameCopy);

        if (renderImGui) {
            imGuiContext.newFrame();
//            ImGui::ShowDemoWindow();

            statWindow.update(frameTime.microseconds() / 1000.f, avgFrameTime.microseconds() / 1000.f, driver.commands().count(), cmd ? cmd->pipelineCount() : 0, cmd ? cmd->descriptorCount() : 0, driver.setLayoutCount(), frameCount);
            statWindow.render();

            ImGui::Begin("Device");
            ImGui::Text("Vendor: %s", driver.context().vendor());
            ImGui::Text("Device: %s", driver.context().deviceName().data());
            ImGui::Text("Type: %s", driver.context().deviceTypeString());
            ImGui::Text("GPU Time: %f", gpuTime / 1e6);

            /*if (ImGui::SliderFloat("QuadDistance", &quadDistance, 0.f, 2.f)) {
                auto q = scene._renderables.back().second;
                q.setPos(cameraTransform.pos() + ende::math::Vec3f{0, 0, quadDistance});
                auto data = q.toMat();
                uniformBuffer.data({&data, sizeof(ende::math::Mat4f)}, (scene._renderables.size() - 1) * sizeof(ende::math::Mat4f));
            }*/

            ImGui::End();

            ImGui::Render();
        }

        {
            auto cameraData = camera.data();
            cameraBuffer.data({&cameraData, sizeof(cameraData)});
            scene.prepare();
        }

        auto frame = driver.swapchain().nextImage();

        cmd = driver.beginFrame();
        timer.start(*cmd);
        {
            VkImageMemoryBarrier barriers[] = { brickwall_copy.barrier(Access::SHADER_READ, Access::SHADER_WRITE, ImageLayout::UNDEFINED, ImageLayout::GENERAL) };
            cmd->pipelineBarrier(PipelineStage::VERTEX_SHADER, PipelineStage::COMPUTE_SHADER, 0, nullptr, barriers);

            cmd->clearDescriptors();
            cmd->bindProgram(computeProgram);
//            cmd->bindImage(0, 0, brickwall_view.view, sampler.sampler());
            cmd->bindImage(0, 0, brickwall_copy_view, sampler, true);
            cmd->bindPipeline();
            cmd->bindDescriptors();
            cmd->dispatchCompute(1024 / 16, 1024 / 16, 1);

            barriers[0] = brickwall_copy.barrier(Access::SHADER_WRITE, Access::SHADER_READ, ImageLayout::GENERAL, ImageLayout::GENERAL);
            cmd->pipelineBarrier(PipelineStage::COMPUTE_SHADER, PipelineStage::VERTEX_SHADER, 0, nullptr, barriers);

            cmd->begin(frame.framebuffer);

            cmd->bindBuffer(0, 0, cameraBuffer);
            scene.render(*cmd);

            imGuiContext.render(*cmd);

            timer.stop();
            cmd->end(frame.framebuffer);
            cmd->submit({&frame.imageAquired, 1}, frame.fence);
        }
        driver.endFrame();
        driver.swapchain().present(frame, cmd->signal());
        prevFrame = frame.index;

        frameTime = frameClock.reset();
        dt = driver.milliseconds() / (f64)1000;
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

        gpuTime = timer.result();

        ende::profile::ProfileManager::endFrame(frameCount);
        frameCount++;

        if (cmd->descriptorCount() > 700)
            running = false;
    }

    driver.swapchain().wait();

    std::cout << "\n\nCommand Buffers: " << driver.commands().count();
    std::cout << "\nPipelines: " << (cmd ? cmd->pipelineCount() : 0);
    std::cout << "\nDescriptors: " << (cmd ? cmd->descriptorCount() : 0);
    std::cout << "\nRenderables: " << scene._renderables.size();
    std::cout << "\nUptime: " << runTime.elapsed().seconds() << "sec";
    std::cout << "\nFrame: " << frameCount << '\n';

    return 0;
}