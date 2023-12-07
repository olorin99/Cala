#include <Cala/backend/vulkan/SDLPlatform.h>
#include <Cala/shapes.h>
#include <Cala/Camera.h>
#include <Cala/Light.h>
#include <Ende/filesystem/File.h>
#include <Cala/ImGuiContext.h>
#include <Cala/Material.h>
#include <Cala/MaterialInstance.h>
#include <Cala/Scene.h>
#include <stb_image.h>
#include "Ende/math/random.h"
#include <Cala/Mesh.h>
#include <Cala/Engine.h>
#include <Cala/Renderer.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <Ende/profile/ProfileManager.h>
#include <Cala/backend/vulkan/OfflinePlatform.h>
#include <Cala/Model.h>
#include <Ende/thread/thread.h>
#include <Cala/ui/GuiWindow.h>

using namespace cala;
using namespace cala::backend;
using namespace cala::backend::vulkan;

MeshData loadModel(const std::filesystem::path& path) {
    MeshData data;

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
        return data;

    for (u32 i = 0; i < scene->mNumMeshes; i++) {
        const aiMesh* mesh = scene->mMeshes[i];
//        if (mesh->mMaterialIndex >= 0) {
//            aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
//            aiString name;
//            material->Get(AI_MATKEY_NAME, name);
//
//        }

        for (u32 j = 0; j < mesh->mNumVertices; j++) {
            const aiVector3D pos = mesh->mVertices[j];
            const aiVector3D normal = mesh->mNormals[j];
            const aiVector3D texCoords = mesh->HasTextureCoords(0) ? mesh->mTextureCoords[0][j] : aiVector3D(0, 0, 0);

            Vertex vertex{};
            vertex.position = { pos.x, pos.y, pos.z };
            vertex.normal = { normal.x, normal.y, normal.z };
            vertex.texCoords = { texCoords.x, texCoords.y };

            if (mesh->HasTangentsAndBitangents()) {
                const aiVector3D tangent = mesh->mTangents[j];
//                const aiVector3D bitangent = mesh->mBitangents[j];
                vertex.tangent = { tangent.x, tangent.y, tangent.z, 1 };
            }

            data.addVertex(vertex);
        }

        for (u32 j = 0; j < mesh->mNumFaces; j++) {
            const aiFace& face = mesh->mFaces[j];
            data.addTriangle(face.mIndices[0], face.mIndices[1], face.mIndices[2]);
        }
    }

    return data;
}

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
    swapchain.setPresentMode(backend::PresentMode::FIFO);

    u32 objectCount = 20;
    Scene scene(&engine, objectCount);

    ui::GuiWindow guiWindow(engine, renderer, scene, swapchain, platform.window());

    Material* material1 = engine.loadMaterial("../../res/materials/pbr.mat");
    if (!material1)
        return -2;

    Transform cameraTransform({10, 1.3, 0}, ende::math::Quaternion({0, 1, 0}, ende::math::rad(-90)));
    Camera camera((f32)ende::math::rad(54.4), platform.windowSize().first, platform.windowSize().second, 0.1f, 100.f, cameraTransform);

    Sampler sampler(engine.device(), {});

    Transform lightTransform({0, 1, 0}, {0, 0, 0, 1}, {0.1, 0.1, 0.1});
    Light light(cala::Light::POINT, true);
    light.setPosition({ 0, 1, 0 });
    light.setColour({ 255.f / 255.f, 202.f / 255.f, 136.f / 255.f });
    light.setIntensity(4.649);
    light.setShadowing(false);
    Light light1(cala::Light::POINT, false);
    light1.setPosition({ 10, 2, 4 });
    light1.setColour({0, 1, 0});
    light1.setIntensity(10);
    Light light2(cala::Light::DIRECTIONAL, true);
    light2.setDirection(ende::math::Quaternion(ende::math::rad(-84), 0, ende::math::rad(-11)));
    light2.setColour({ 255.f / 255.f, 202.f / 255.f, 136.f / 255.f });
    light2.setIntensity(2);
    light2.setShadowing(true);
    Light light3(cala::Light::POINT, false);
    light3.setPosition({ -10, 2, 4 });
    light3.setIntensity(1);
    light3.setColour({0.23, 0.46, 0.10});

    scene.addLight(light, lightTransform);

//    auto background = engine.assetManager()->loadImage("background", "textures/TropicalRuins_3k.hdr", backend::Format::RGBA32_SFLOAT);
//    auto background = engine.assetManager()->loadImage("background", "textures/Tropical_Beach_3k.hdr", backend::Format::RGBA32_SFLOAT);
//    scene.addSkyLightMap(background, true);

//    Mesh cube = cala::shapes::cube().mesh(&engine);

    auto sphere = engine.assetManager()->loadModel("sphere", "models/sphere.glb", material1);
    auto sponzaAsset = engine.assetManager()->loadModel("sponza", "models/gltf/glTF-Sample-Models/2.0/Sponza/glTF/Sponza.gltf", material1);
//    auto bistro = engine.assetManager()->loadModel("bistro", "models/bistro/gltf/Bistro_Exterior.gltf", material1);
//    auto damagedHelmet = engine.assetManager()->loadModel("damagedHelmet", "models/gltf/glTF-Sample-Models/2.0/DamagedHelmet/glTF/DamagedHelmet.gltf", material1);


    Transform defaultTransform;
    scene.addModel(*sponzaAsset, defaultTransform);
//    scene.addModel(*bistro, defaultTransform);
//    scene.addModel(*damagedHelmet, sponzaTransform);
    auto sphereNode = scene.addModel(*sphere, lightTransform);

    f32 sceneBounds = 10;

    Transform t1({ 0, 4, 0 });
    Transform t2({ 2, 1, 0 }, {}, { 1, 1, 1 }, &t1);

    scene.addModel(*sphere, t2);

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
                            camera.resize(event.window.data1, event.window.data2);
                            break;
                    }
                    break;
            }
            guiWindow.context().processEvent(&event);
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
                cameraTransform.rotate({0, 1, 0}, ende::math::rad(-90) * dt);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_RIGHT])
                cameraTransform.rotate({0, 1, 0}, ende::math::rad(90) * dt);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_UP])
                cameraTransform.rotate(cameraTransform.rot().right(), ende::math::rad(45) * dt);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_DOWN])
                cameraTransform.rotate(cameraTransform.rot().right(), ende::math::rad(-45) * dt);
        }

        {
            guiWindow.render();


            f32 exposure = camera.getExposure();
            if (ImGui::SliderFloat("Exposure", &exposure, 0, 10))
                camera.setExposure(exposure);

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
                    scene.addLight(l, transform);
                }
            }

            auto pos = camera.transform().pos();
            ImGui::Text("Position: { %f, %f, %f }", pos.x(), pos.y(), pos.z());

            {
                auto parentPos = t2.parent()->pos();
                if (ImGui::DragFloat3("parent", &parentPos[0], 0.1, -10, 10))
                    t2.parent()->setPos(parentPos);
            }

            ImGui::End();

            ImGui::Render();
        }

        {
            f32 seconds = engine.getRunningTime().milliseconds() / 1000.f;
            f32 factor = std::clamp(seconds / 100, 0.f, 1.f);
            auto currentPos = ende::math::Vec3f{-10, 1, 0}.lerp(ende::math::Vec3f{10, 1, 0}, factor);
            sphereNode->transform.setPos(currentPos);
        }

        if (renderer.beginFrame(&swapchain)) {
            scene.prepare(camera);

            renderer.render(scene, camera, &guiWindow.context());

            dt = renderer.endFrame();
        }


        ende::profile::ProfileManager::frame();
    }

    engine.device().wait();
}