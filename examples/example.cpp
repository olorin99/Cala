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
//    scene.addCamera(camera, Transform({-10, 1.3, 0}, ende::math::Quaternion({0, 1, 0}, ende::math::rad(90))));


    Sampler sampler(engine.device(), {});

    Transform lightTransform({0, 1, 0}, {0, 0, 0, 1}, {0.1, 0.1, 0.1});
    Light light(cala::Light::POINT, true);
    light.setPosition({ 0, 1, 0 });
    light.setColour({ 255.f / 255.f, 202.f / 255.f, 136.f / 255.f });
    light.setIntensity(4.649);
    light.setShadowing(true);
    Light light1(cala::Light::POINT, false);
    light1.setPosition({ 10, 2, 4 });
    light1.setColour({0, 1, 0});
    light1.setIntensity(10);
    Light light2(cala::Light::DIRECTIONAL, true);
    light2.setDirection(ende::math::Quaternion(ende::math::rad(-84), 0, ende::math::rad(-11)));
    light2.setColour({ 255.f / 255.f, 202.f / 255.f, 136.f / 255.f });
    light2.setIntensity(2);
    light2.setShadowing(true);
    light2.setCascadeCount(4);
    light2.setCascadeSplit(0, 4.5);
    light2.setCascadeSplit(1, 20);
    light2.setCascadeSplit(2, 50);
    light2.setCascadeSplit(2, 75);
    Light light3(cala::Light::POINT, false);
    light3.setPosition({ -10, 2, 4 });
    light3.setIntensity(1);
    light3.setColour({0.23, 0.46, 0.10});
    light3.setShadowing(true);

//    scene.addLight(light, lightTransform);
    scene.addLight(light2, lightTransform);
//    scene.addLight(light3, lightTransform);

    auto background = engine.assetManager()->loadImage("background", "textures/TropicalRuins_3k.hdr", vk::Format::RGBA32_SFLOAT);
//    auto background = engine.assetManager()->loadImage("background", "textures/Tropical_Beach_3k.hdr", backend::Format::RGBA32_SFLOAT);
    scene.addSkyLightMap(background, true);

//    Mesh cube = cala::shapes::cube().mesh(&engine);
//    auto matInstance = material1->instance();
//    scene.addMesh(cube, Transform({0, 3, 0}, {}, {1, 3, 1}), &matInstance);

    auto sponzaAsset = engine.assetManager()->loadModel("sponza", "models/gltf/glTF-Sample-Models/2.0/Sponza/glTF/Sponza.gltf", material1);
//    auto bistro = engine.assetManager()->loadModel("bistro", "models/bistro/gltf/Bistro_Exterior.gltf", material1);
//    auto damagedHelmet = engine.assetManager()->loadModel("damagedHelmet", "models/gltf/glTF-Sample-Models/2.0/DamagedHelmet/glTF/DamagedHelmet.gltf", material1);
    auto suzanne = engine.assetManager()->loadModel("suzanne", "models/gltf/glTF-Sample-Models/2.0/Suzanne/glTF/Suzanne.gltf", material1);
    auto sphere = engine.assetManager()->loadModel("sphere", "models/sphere.glb", material1);

    scene.addModel("suzanne", *suzanne, Transform());
//    auto plane = engine.assetManager()->loadModel("plane", "models/plane.glb", material1);
//    scene.addModel("plane", *plane, Transform({}, {}, {100, 1, 100}));

    Transform defaultTransform;
    scene.addModel("sponza", *sponzaAsset, defaultTransform);
//    scene.addModel(*bistro, defaultTransform);
//    scene.addModel(*damagedHelmet, sponzaTransform);
    auto sphereNode = scene.addModel("smallSphere", *sphere, lightTransform);

    f32 sceneBounds = 10;

    Transform t2({ 2, 4, 0 });

//    scene.addModel("sphere2", *sphere, t2);

    i32 newLights = 10;

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
                        scene.addModel(droppedFile, *asset, defaultTransform);
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

        {
            guiWindow.render();


            f32 exposure = scene.getMainCamera()->getExposure();
            if (ImGui::SliderFloat("Exposure", &exposure, 0, 10))
                scene.getMainCamera()->setExposure(exposure);

            ImGui::SliderInt("New Lights", &newLights, 0, 100);
            if (ImGui::Button("Add Lights")) {
                for (u32 i = 0; i < newLights; i++) {
                    Transform transform({ende::math::rand(-sceneBounds, sceneBounds), ende::math::rand(0.f, sceneBounds), ende::math::rand(-sceneBounds, sceneBounds)});
                    Light l(Light::LightType::POINT, false);
                    l.setPosition(transform.pos());
//                l.setIntensity(ende::math::rand(0.1f, 5.f));
//                l.setIntensity(0.1f);
                    l.setRange(1);
                    l.setColour({ ende::math::rand(0.f, 1.f), ende::math::rand(0.f, 1.f), ende::math::rand(0.f, 1.f) });
                    l.setShadowing(true);
                    scene.addLight(l, transform);
                }
            }

            auto pos = scene.getMainCamera()->transform().pos();
            ImGui::Text("Position: { %f, %f, %f }", pos.x(), pos.y(), pos.z());

//            {
//                auto parentPos = t2.parent()->pos();
//                if (ImGui::DragFloat3("parent", &parentPos[0], 0.1, -10, 10))
//                    t2.parent()->setPos(parentPos);
//            }

            ImGui::End();

            ImGui::Render();
        }

//        {
//            f32 seconds = engine.getRunningTime().milliseconds() / 1000.f;
//            f32 factor = std::clamp(seconds / 100, 0.f, 1.f);
//            auto currentPos = ende::math::Vec3f{-10, 1, 0}.lerp(ende::math::Vec3f{10, 1, 0}, factor);
//            sphereNode->transform.setPos(currentPos);
//        }

        if (renderer.beginFrame(&swapchain)) {
            scene.prepare();

            renderer.render(scene, &guiWindow.context());

            dt = renderer.endFrame();
        }


        ende::profile::ProfileManager::frame();
    }

    engine.device().wait();
}