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
    OfflinePlatform offlinePlatform(1920, 1080);

    Engine engine(platform);
    backend::vulkan::Swapchain swapchain(engine.device(), platform);
    Renderer renderer(&engine, {});
    swapchain.setPresentMode(backend::PresentMode::FIFO);

    u32 objectCount = 20;
    Scene scene(&engine, objectCount);

    ui::GuiWindow guiWindow(engine, renderer, scene, swapchain, platform.window());


//    struct Material1Data {
//        i32 albedoIndex = -1;
//        i32 normalIndex = -1;
//        i32 metallicRoughness = -1;
//        f32 metallness = 0;
//        f32 roughness = 0;
//    };
//    Material* material = engine.loadMaterial("../../res/materials/expanded_pbr.mat");
//    if (!material)
//        return -1;

    struct MaterialData {
        i32 albedoIndex = -1;
        i32 normalIndex = -1;
        i32 metallicRoughness = -1;
    };

    Material* material1 = engine.loadMaterial("../../res/materials/pbr.mat");
    if (!material1)
        return -2;

    Transform sponzaTransform;

    Transform helmetTransform({0, 0, 0}, {0, 0, 0, 1}, { 50, 0.1, 50});

    Mesh cube = cala::shapes::cube().mesh(&engine);
//    Mesh sphere = cala::shapes::sphereNormalized(1).mesh(&engine);
    Mesh sphere = loadModel("../../res/models/sphere.obj").mesh(&engine);
    sphere.min = { -1, -1, -1 };
    sphere.max = { 1, 1, 1 };

    auto sponzaAsset = engine.assetManager()->loadModel("sponza", "models/gltf/glTF-Sample-Models/2.0/Sponza/glTF/Sponza.gltf", material1);

//    auto sponza = loadGLTF(&engine, material1, "../../res/models/gltf/glTF-Sample-Models/2.0/Sponza/glTF/Sponza.gltf");
//    auto damagedHelmet = loadGLTF(&engine, &material, "../../res/models/gltf/glTF-Sample-Models/2.0/SciFiHelmet/glTF/SciFiHelmet.gltf");
    Model damagedHelmet;
//    bool addHelmet = false;


    Transform cameraTransform({10, 1.3, 0}, ende::math::Quaternion({0, 1, 0}, ende::math::rad(-90)));
    Camera camera((f32)ende::math::rad(54.4), platform.windowSize().first, platform.windowSize().second, 0.1f, 100.f, cameraTransform);

    Sampler sampler(engine.device(), {});

    auto matInstance = material1->instance();


    matInstance.setData(MaterialData{});

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

//    u32 l1 = scene.addLight(light1);
//    u32 l2 = scene.addLight(light2);
//    u32 l0 = scene.addLight(light);
//    u32 l3 = scene.addLight(light3);

    scene.addLight(light, lightTransform);

//    auto background = engine.assetManager()->loadImage("background", "textures/TropicalRuins_3k.hdr", backend::Format::RGBA32_SFLOAT);
//    auto background = engine.assetManager()->loadImage("background", "textures/Tropical_Beach_3k.hdr", backend::Format::RGBA32_SFLOAT);
//    scene.addSkyLightMap(background, true);

    scene.addModel(*sponzaAsset, sponzaTransform);

    scene.addMesh(sphere, &matInstance, lightTransform);

    f32 sceneSize = std::max(objectCount / 10, 20u);

    f32 sceneBounds = 10;

//    MaterialInstance instances[10] = {
//            material->instance(),
//            material->instance(),
//            material->instance(),
//            material->instance(),
//            material->instance(),
//            material->instance(),
//            material->instance(),
//            material->instance(),
//            material->instance(),
//            material->instance()
//    };
//    ImageHandle roughnessImages[10];
//    for (u32 i = 0; i < 10; i++) {
//        roughnessImages[i] = engine.device().createImage({1, 1, 1, backend::Format::RGBA32_SFLOAT, 1, 1, backend::ImageUsage::SAMPLED | backend::ImageUsage::TRANSFER_DST, backend::ImageType::IMAGE2D});
//        f32 metallicRoughnessData[] = { 0.f, static_cast<f32>(i) / 10.f, 0.f, 1.f };
//        engine.stageData(roughnessImages[i], std::span<f32>(metallicRoughnessData, 4), {0, 1, 1, 1, 4 * 4 });
////        roughnessImages[i]->data(engine.device(), {0, 1, 1, 1, 4 * 4 }, std::span<f32>(metallicRoughnessData, 4));
//
//        Material1Data materialData1 {
//                -1,
//                -1,
//                roughnessImages[i].index(),
//                1.0,
//                0.0
//        };
//
//        instances[i].setData(materialData1);
//    }


    u32 width = 10;
    u32 height = 10;
    u32 depth = 10;

//    Transform transform;
//    std::vector<Transform> transforms;
//    transforms.reserve(width * height * depth * 10);
//    for (u32 i = 0; i < width; i++) {
//        auto xpos = transform.pos();
//        for (u32 j = 0; j < height; j++) {
//            auto ypos = transform.pos();
//            for (u32 k = 0; k < depth; k++) {
//                transform.addPos({0, 0, 3});
//                transforms.push_back(transform);
//                scene.addRenderable(sphere, &instances[k], &transforms.back(), false);
//            }
//            transform.setPos(ypos + ende::math::Vec3f{0, 3, 0});
//        }
//        transform.setPos(xpos + ende::math::Vec3f{3, 0, 0});
//    }


    Transform t1({ 0, 4, 0 });
    Transform t2({ 2, 1, 0 }, {}, { 1, 1, 1 }, &t1);

    scene.addMesh(sphere, &matInstance, t2);



    std::vector<Transform> lightTransforms;
    lightTransforms.reserve(10000);
    for (u32 i = 0; i < 0; i++) {
        lightTransforms.push_back(Transform({ende::math::rand(-sceneSize * 1.5f, sceneSize * 1.5f), ende::math::rand(-sceneSize * 1.5f, sceneSize * 1.5f), ende::math::rand(-sceneSize * 1.5f, sceneSize * 1.5f)}));
        Light l(cala::Light::POINT, false);
        l.setIntensity(ende::math::rand(10.f, 1000.f));
        l.setColour({ende::math::rand(0.f, 1.f), ende::math::rand(0.f, 1.f), ende::math::rand(0.f, 1.f)});
        scene.addLight(l, lightTransforms.back());
    }

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
                    lightTransforms.push_back(Transform({ende::math::rand(-sceneBounds, sceneBounds), ende::math::rand(0.f, sceneBounds), ende::math::rand(-sceneBounds, sceneBounds)}));
                    Light l(Light::LightType::POINT, false);
                    l.setPosition(lightTransforms.back().pos());
//                l.setIntensity(ende::math::rand(0.1f, 5.f));
//                l.setIntensity(0.1f);
                    l.setRange(1);
                    l.setColour({ ende::math::rand(0.f, 1.f), ende::math::rand(0.f, 1.f), ende::math::rand(0.f, 1.f) });
                    scene.addLight(l, lightTransforms.back());
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
//            helmetTransform.rotate(ende::math::Vec3f{0, 1, 0}, ende::math::rad(45) * dt);
            lightTransform.rotate(ende::math::Vec3f{0, 1, 1}, ende::math::rad(45) * dt);
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