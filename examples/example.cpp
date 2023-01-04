#include <Cala/backend/vulkan/SDLPlatform.h>
#include <Cala/backend/vulkan/Driver.h>
#include <Cala/shapes.h>
#include <Cala/Camera.h>
#include <Cala/Light.h>
#include <Ende/filesystem/File.h>
#include <Cala/ImGuiContext.h>
#include <Ende/thread/thread.h>
#include <Cala/backend/vulkan/Timer.h>
#include <Cala/Material.h>
#include <Cala/MaterialInstance.h>
#include <Cala/Scene.h>
#include "../../third_party/stb_image.h"
#include "Ende/math/random.h"
#include <Cala/Mesh.h>
#include <Cala/Engine.h>
#include <Cala/Renderer.h>

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

ImageHandle loadImage(Engine& engine, const ende::fs::Path& path) {
    i32 width, height, channels;
    u8* data = stbi_load((*path).c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!data) throw "unable load image";
    u32 length = width * height * 4;

    ImageHandle handle = engine.createImage({(u32)width, (u32)height, 1, backend::Format::RGBA8_SRGB});

    handle->data(engine.driver(), {0, (u32)width, (u32)height, 1, 4, {data, length}});
    stbi_image_free(data);
    return handle;
}

ImageHandle loadImageHDR(Engine& engine, const ende::fs::Path& path) {
    stbi_set_flip_vertically_on_load(true);
    i32 width, height, channels;
    f32* data = stbi_loadf((*path).c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!data) throw "unable load image";
    u32 length = width * height * 4 * 4;

    ImageHandle handle = engine.createImage({(u32)width, (u32)height, 1, backend::Format::RGBA32_SFLOAT});
    handle->data(engine.driver(), {0, (u32)width, (u32)height, 1, (u32)4 * 4, {data, length}});
    stbi_image_free(data);
    return handle;
}

int main() {
    SDLPlatform platform("hello_triangle", 800, 600, SDL_WINDOW_RESIZABLE);

    Engine engine(platform);
    Renderer renderer(&engine);

    ImGuiContext imGuiContext(engine.driver(), platform.window());

    //Shaders
    ProgramHandle pointLightProgram = engine.createProgram(loadShader(engine.driver(), "../../res/shaders/direct_shadow.vert.spv"_path, "../../res/shaders/point_shadow.frag.spv"_path));
    ProgramHandle directionalLightProgram = engine.createProgram(loadShader(engine.driver(), "../../res/shaders/direct_shadow.vert.spv"_path, "../../res/shaders/direct_shadow.frag.spv"_path));

    Mesh cube = cala::shapes::cube().mesh(engine.driver());
    Mesh sphere = cala::shapes::sphereNormalized(1).mesh(engine.driver());

    Transform modelTransform({0, 0, 0});
    ende::Vector<Transform> models;

    Transform cameraTransform({0, 0, -10});
    Camera camera((f32)ende::math::rad(54.4), 800.f, 600.f, 0.1f, 1000.f, cameraTransform);

    Transform lightTransform({-3, 3, -1}, {0, 0, 0, 1}, {0.5, 0.5, 0.5});
    Light light(cala::Light::POINT, true, lightTransform);
    Transform light1Transform({10, 2, 4});
    Light light1(cala::Light::POINT, false, light1Transform);
    Transform light2Transform({0, 0, 0}, ende::math::Quaternion({1, 0, 0}, ende::math::rad(-45)));
    Light light2(cala::Light::DIRECTIONAL, false, light2Transform);


//    Camera camera((f32)ende::math::rad(54.4), 800.f, 600.f, 0.1f, 1000.f, lightTransform);

    Sampler sampler(engine.driver(), {});

    ImageHandle brickwall = loadImage(engine, "../../res/textures/brickwall.jpg"_path);
    ImageHandle brickwall_normal = loadImage(engine, "../../res/textures/brickwall_normal.jpg"_path);
    ImageHandle brickwall_specular = loadImage(engine, "../../res/textures/brickwall_specular.jpg"_path);


    Material material(&engine);
    material.setProgram(Material::Variants::POINT, pointLightProgram);
    material.setProgram(Material::Variants::DIRECTIONAL, directionalLightProgram);
    material._depthState = { true, true, CompareOp::LESS_EQUAL };

    auto matInstance = material.instance();
    matInstance.setSampler("diffuseMap", *brickwall, Sampler(engine.driver(), {}));
    matInstance.setSampler("normalMap", *brickwall_normal, Sampler(engine.driver(), {}));
    matInstance.setSampler("specularMap", *brickwall_specular, Sampler(engine.driver(), {}));

    u32 objectCount = 100;
    Scene scene(&engine, objectCount);
    scene.addLight(light);
//    scene.addLight(light1);
    scene.addLight(light2);

    ImageHandle background = loadImageHDR(engine, "../../res/textures/TropicalRuins_3k.hdr"_path);
    scene.addSkyLightMap(background, true);

    scene.addRenderable(cube, &matInstance, &modelTransform);
//    scene.addRenderable(mesh, &matInstance, &lightTransform);

    f32 sceneSize = objectCount / 10;

    Transform floorTransform({0, -sceneSize * 1.5f, 0}, {0, 0, 0, 1}, {sceneSize * 3, 1, sceneSize * 3});
    scene.addRenderable(cube, &matInstance, &floorTransform);

    models.reserve(objectCount);
    for (u32 i = 0; i < objectCount; i++) {
        models.push(Transform({ende::math::rand(-sceneSize, sceneSize), ende::math::rand(-sceneSize, sceneSize), ende::math::rand(-sceneSize, sceneSize)}));
        scene.addRenderable(i % 2 == 0 ? cube : sphere, &matInstance, &models.back());
//        scene.addRenderable(cube, &matInstance, &models.back());
    }

    f64 dt = 1.f / 60.f;
    bool running = true;
    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_WINDOWEVENT:
                    switch (event.window.event) {
                        case SDL_WINDOWEVENT_RESIZED:
                            engine.driver().wait();
                            engine.driver().swapchain().resize(event.window.data1, event.window.data2);
                            camera.resize(event.window.data1, event.window.data2);
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
                cameraTransform.addPos(cameraTransform.rot().invertY().left() * dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_D])
                cameraTransform.addPos(cameraTransform.rot().invertY().right() * dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_LSHIFT])
                cameraTransform.addPos(cameraTransform.rot().invertY().down() * dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_SPACE])
                cameraTransform.addPos(cameraTransform.rot().invertY().up() * dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_LEFT])
                cameraTransform.rotate({0, 1, 0}, ende::math::rad(-45) * dt);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_RIGHT])
                cameraTransform.rotate({0, 1, 0}, ende::math::rad(45) * dt);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_UP])
                cameraTransform.rotate(cameraTransform.rot().right(), ende::math::rad(45) * dt);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_DOWN])
                cameraTransform.rotate(cameraTransform.rot().right(), ende::math::rad(-45) * dt);
        }

        {
            imGuiContext.newFrame();

            ImGui::Begin("Stats");
            ImGui::Text("FPS: %f", engine.driver().fps());
            ImGui::Text("Milliseconds: %f", engine.driver().milliseconds());
            ImGui::Text("Delta Time: %f", dt);

            auto passTimers = renderer.timers();
            for (auto& timer : passTimers) {
                ImGui::Text("%s ms: %f", timer.first, timer.second.result() / 1e6);
            }

            Renderer::Stats rendererStats = renderer.stats();
            ImGui::Text("Descriptors: %d", rendererStats.descriptorCount);
            ImGui::Text("Pipelines: %d", rendererStats.pipelineCount);

            ende::math::Vec3f colour = scene._lights.front().getColour();
            f32 intensity = scene._lights.front().getIntensity();
            if (ImGui::ColorEdit3("Colour", &colour[0]) ||
                ImGui::SliderFloat("Intensity", &intensity, 1, 50)) {
                scene._lights.front().setColour(colour);
                scene._lights.front().setIntensity(intensity);
            }

            ende::math::Quaternion direction = scene._lights.back().transform().rot();
            if (ImGui::DragFloat4("Direction", &direction[0], 0.01, -1, 1)) {
                scene._lights.back().setDirection(direction);
            }


            ImGui::End();
            ImGui::Render();
        }

        {
            modelTransform.rotate(ende::math::Vec3f{0, 1, 0}, ende::math::rad(45) * dt);
            lightTransform.rotate(ende::math::Vec3f{0, 1, 1}, ende::math::rad(45) * dt);
        }
        renderer.beginFrame();

        scene.prepare(renderer.frameIndex(), camera);

        renderer.render(scene, camera, &imGuiContext);

        dt = renderer.endFrame();
    }

    engine.driver().wait();
}