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

int main() {
    SDLPlatform platform("hello_triangle", 800, 600);

    Engine engine(platform);
    Renderer renderer(&engine);

    Driver& driver = engine.driver();

    ImGuiContext imGuiContext(driver, platform.window());

    //Shaders
    ShaderProgram program = loadShader(driver, "../../res/shaders/default.vert.spv"_path, "../../res/shaders/phong_directional.frag.spv"_path);

    Mesh mesh = cala::shapes::cube().mesh(driver);

    Transform modelTransform({0, 0, 0});
    ende::Vector<Transform> models;

    Transform cameraTransform({0, 0, -10});
    Camera camera((f32)ende::math::rad(54.4), 800.f, 600.f, 0.1f, 1000.f, cameraTransform);

    Transform lightTransform({-3, 3, -1});
    Light light(cala::Light::DIRECTIONAL, false, lightTransform);

    Sampler sampler(driver, {});

    ImageHandle brickwall = loadImage(engine, "../../res/textures/brickwall.jpg"_path);
    ImageHandle brickwall_normal = loadImage(engine, "../../res/textures/brickwall_normal.jpg"_path);
    ImageHandle brickwall_specular = loadImage(engine, "../../res/textures/brickwall_specular.jpg"_path);


    Material material(driver, std::move(program));
    material._rasterState = { .cullMode = CullMode::FRONT };
    material._depthState = { true, true, CompareOp::LESS_EQUAL };

    auto matInstance = material.instance();
    matInstance.setSampler("diffuseMap", *brickwall, Sampler(driver, {}));
    matInstance.setSampler("normalMap", *brickwall_normal, Sampler(driver, {}));
    matInstance.setSampler("specularMap", *brickwall_specular, Sampler(driver, {}));


    Scene scene(driver, 11);
    scene.addLight(light);

    scene.addRenderable(mesh, &matInstance, &modelTransform);

    models.reserve(10);
    for (u32 i = 0; i < 10; i++) {
        models.push(Transform({ende::math::rand(-5.f, 5.f), ende::math::rand(-5.f, 5.f), ende::math::rand(-5.f, 5.f)}));
        scene.addRenderable(mesh, &matInstance, &models.back());
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
                            driver.wait();
                            driver.swapchain().resize(event.window.data1, event.window.data2);
                            camera.resize(event.window.data1, event.window.data2);
                            break;
                    }
                    break;
            }
        }

        {
            imGuiContext.newFrame();

            ImGui::Begin("Stats");
            ImGui::Text("FPS: %f", driver.fps());
            ImGui::Text("Milliseconds: %f", driver.milliseconds());
            ImGui::Text("DT: %f", dt);
            ImGui::End();
            ImGui::Render();
        }

        {
            modelTransform.rotate(ende::math::Vec3f{0, 1, 0}, ende::math::rad(45) * dt);
            lightTransform.rotate(ende::math::Vec3f{0, 1, 1}, ende::math::rad(45) * dt);

            scene.prepare();
        }

        renderer.beginFrame();

        auto& cmd = renderer.render(scene, camera);

        imGuiContext.render(cmd);

        dt = renderer.endFrame();
    }

    driver.wait();
}