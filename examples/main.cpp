#include <Cala/vulkan/SDLPlatform.h>
#include <Cala/shapes.h>
#include <Cala/Camera.h>
#include <Cala/Light.h>
#include <Cala/ImGuiContext.h>
#include <Cala/Material.h>
#include <Cala/MaterialInstance.h>
#include <Cala/Scene.h>
#include "Ende/math/random.h"
#include <Cala/Engine.h>
#include <Cala/Renderer.h>
#include <Ende/profile/ProfileManager.h>
#include <Cala/ui/GuiWindow.h>

using namespace cala;
using namespace cala::vk;

int main() {
    SDLPlatform platform("hello_triangle", 1920, 1080);

    Engine engine(platform);
    auto swapchainResult = Swapchain::create(&engine.device(), {
        .platform = &platform
    });
    if (!swapchainResult)
        return -10;
    auto swapchain = std::move(swapchainResult.value());
    Renderer renderer(&engine, {});
    swapchain.setPresentMode(vk::PresentMode::FIFO);

    u32 objectCount = 20;
    Scene scene(&engine, objectCount);

    ui::GuiWindow guiWindow(engine, renderer, scene, swapchain, platform.window());

    Material* material1 = engine.loadMaterial("../../res/materials/pbr.mat");
    if (!material1)
        return -2;

    Camera camera((f32)ende::math::rad(54.4), platform.windowSize().first, platform.windowSize().second, 0.1f, 100.f);
    scene.addCamera(camera, Transform({10, 1.3, 0}, ende::math::Quaternion({0, 1, 0}, ende::math::rad(-90))));


    Transform lightTransform({0, 1, 0}, {0, 0, 0, 1}, {0.1, 0.1, 0.1});
    Light light(cala::Light::DIRECTIONAL, true);
    light.setDirection(ende::math::Quaternion(ende::math::rad(-84), 0, ende::math::rad(-11)));
    light.setColour({ 255.f / 255.f, 202.f / 255.f, 136.f / 255.f });
    light.setIntensity(2);
    light.setShadowing(true);
    light.setCascadeCount(4);
    light.setCascadeSplit(0, 4.5);
    light.setCascadeSplit(1, 20);
    light.setCascadeSplit(2, 50);
    light.setCascadeSplit(2, 75);

    auto lightNode = scene.addLight(light, lightTransform);

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
                            engine.device().wait();
                            swapchain.resize(event.window.data1, event.window.data2);
                            scene.getMainCamera()->resize(event.window.data1, event.window.data2);
                            break;
                    }
                    break;
                case SDL_DROPFILE:
                    char* droppedFile = event.drop.file;
                    engine.logger().info("Dropped File: {}", droppedFile);
                    std::filesystem::path assetPath = droppedFile;
                    if (assetPath.extension() == ".gltf" || assetPath.extension() == ".glb") {
                        auto asset = engine.assetManager()->loadModel(droppedFile, assetPath, material1);
                        scene.addModel(droppedFile, *asset, Transform());
                    } else
                        engine.logger().warn("Unrecognised file type: {}", assetPath.extension().string());
                    SDL_free(droppedFile);
                    break;
            }
            guiWindow.context().processEvent(&event);
        }
        {
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_W])
                scene.getMainCamera()->transform().addPos(scene.getMainCamera()->transform().rot().invertY().front() * dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_S])
                scene.getMainCamera()->transform().addPos(scene.getMainCamera()->transform().rot().invertY().back() * dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_A])
                scene.getMainCamera()->transform().addPos(scene.getMainCamera()->transform().rot().invertY().left() * dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_D])
                scene.getMainCamera()->transform().addPos(scene.getMainCamera()->transform().rot().invertY().right() * dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_LSHIFT])
                scene.getMainCamera()->transform().addPos(scene.getMainCamera()->transform().rot().invertY().down() * dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_SPACE])
                scene.getMainCamera()->transform().addPos(scene.getMainCamera()->transform().rot().invertY().up() * dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_LEFT])
                scene.getMainCamera()->transform().rotate({0, 1, 0}, ende::math::rad(-90) * dt);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_RIGHT])
                scene.getMainCamera()->transform().rotate({0, 1, 0}, ende::math::rad(90) * dt);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_UP])
                scene.getMainCamera()->transform().rotate(scene.getMainCamera()->transform().rot().right(), ende::math::rad(45) * dt);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_DOWN])
                scene.getMainCamera()->transform().rotate(scene.getMainCamera()->transform().rot().right(), ende::math::rad(-45) * dt);
        }

        guiWindow.render();

        if (renderer.beginFrame(&swapchain)) {
            scene.prepare();

            renderer.render(scene, &guiWindow.context());

            dt = renderer.endFrame();
        }


        ende::profile::ProfileManager::frame();
    }

    engine.device().wait();
}