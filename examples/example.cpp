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
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/cimport.h>
#include <assimp/scene.h>

using namespace cala;
using namespace cala::backend;
using namespace cala::backend::vulkan;

MeshData loadModel(const ende::fs::Path& path) {
    MeshData data;

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(*path, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);

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
                const aiVector3D bitangent = mesh->mBitangents[j];
                vertex.tangent = { tangent.x, tangent.y, tangent.z };
                vertex.bitangent = { bitangent.x, bitangent.y, bitangent.z };
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
    ProgramHandle pointLightProgram = engine.createProgram(loadShader(engine.driver(), "../../res/shaders/direct_shadow.vert.spv"_path, "../../res/shaders/pbr.frag.spv"_path));
    ProgramHandle directionalLightProgram = engine.createProgram(loadShader(engine.driver(), "../../res/shaders/direct_shadow.vert.spv"_path, "../../res/shaders/direct_pbr.frag.spv"_path));

    Mesh cube = cala::shapes::cube().mesh(engine.driver());
//    Mesh sphere = cala::shapes::sphereUV(1).mesh(engine.driver());
    Mesh sphere = loadModel("../../res/models/sphere.obj"_path).mesh(engine.driver());

    Transform modelTransform({0, 0, 0});
    ende::Vector<Transform> models;

    Transform cameraTransform({0, 0, -20});
    Camera camera((f32)ende::math::rad(54.4), 800.f, 600.f, 0.1f, 1000.f, cameraTransform);

    Sampler sampler(engine.driver(), {});

    ImageHandle brickwall_albedo = loadImage(engine, "../../res/textures/pbr_rusted_iron/rustediron2_basecolor.png"_path);
    ImageHandle brickwall_normal = loadImage(engine, "../../res/textures/pbr_rusted_iron/rustediron2_normal.png"_path);
    ImageHandle brickwall_metallic = loadImage(engine, "../../res/textures/pbr_rusted_iron/rustediron2_metallic.png"_path);
    ImageHandle brickwall_roughness = loadImage(engine, "../../res/textures/pbr_rusted_iron/rustediron2_roughness.png"_path);
    ImageHandle brickwall_ao = loadImage(engine, "../../res/textures/pbr_rusted_iron/rustediron2_ao.png"_path);



    Material material(&engine);
    material.setProgram(Material::Variants::POINT, pointLightProgram);
    material.setProgram(Material::Variants::DIRECTIONAL, directionalLightProgram);
    material._depthState = { true, true, CompareOp::LESS_EQUAL };

    auto matInstance = material.instance();

    matInstance.setSampler("albedoMap", brickwall_albedo->newView(), Sampler(engine.driver(), {}));
    matInstance.setSampler("normalMap", brickwall_normal->newView(), Sampler(engine.driver(), {}));
    matInstance.setSampler("metallicMap", brickwall_metallic->newView(), Sampler(engine.driver(), {}));
    matInstance.setSampler("roughnessMap", brickwall_roughness->newView(), Sampler(engine.driver(), {}));
    matInstance.setSampler("aoMap", brickwall_ao->newView(), Sampler(engine.driver(), {}));

    u32 objectCount = 100;
    Scene scene(&engine, objectCount);

    Transform lightTransform({-10, 0, 0}, {0, 0, 0, 1}, {0.5, 0.5, 0.5});
    Light light(cala::Light::POINT, true, lightTransform);
    light.setColour({1, 0, 0});
    light.setIntensity(300);
    Transform light1Transform({10, 2, 4});
    Light light1(cala::Light::POINT, false, light1Transform);
    light1.setColour({0, 1, 0});
    light1.setIntensity(100);
    Transform light2Transform({0, 0, 0}, ende::math::Quaternion({1, 0, 0}, ende::math::rad(-45)));
    Light light2(cala::Light::DIRECTIONAL, false, light2Transform);
    light2.setColour({0, 0, 1});
    light2.setIntensity(2000);
    Transform light3Transform({-10, 2, 4});
    Light light3(cala::Light::POINT, false, light3Transform);
    light3.setIntensity(120);
    light3.setColour({0.23, 0.46, 0.10});

    u32 l0 = scene.addLight(light);
    u32 l1 = scene.addLight(light1);
    u32 l2 = scene.addLight(light2);
    u32 l3 = scene.addLight(light3);

    ImageHandle background = loadImageHDR(engine, "../../res/textures/TropicalRuins_3k.hdr"_path);
    scene.addSkyLightMap(background, true);

//    scene.addRenderable(cube, &matInstance, &modelTransform);
    scene.addRenderable(cube, &matInstance, &lightTransform, false);

    f32 sceneSize = std::max(objectCount / 10, 20u);

    Transform floorTransform({0, -sceneSize * 1.5f, 0}, {0, 0, 0, 1}, {sceneSize * 1.5f, 1, sceneSize * 1.5f});
    scene.addRenderable(cube, &matInstance, &floorTransform, false);

    models.reserve(objectCount);
    for (u32 i = 0; i < objectCount; i++) {
        models.push(Transform({ende::math::rand(-sceneSize, sceneSize), ende::math::rand(-sceneSize, sceneSize), ende::math::rand(-sceneSize, sceneSize)}));
        scene.addRenderable(i % 2 == 0 ? cube : sphere, &matInstance, &models.back());
    }

    ende::Vector<Transform> lightTransforms;
    lightTransforms.reserve(100);
    for (u32 i = 0; i < 0; i++) {
        lightTransforms.push(Transform({ende::math::rand(-sceneSize * 1.5f, sceneSize * 1.5f), ende::math::rand(-sceneSize * 1.5f, sceneSize * 1.5f), ende::math::rand(-sceneSize * 1.5f, sceneSize * 1.5f)}));
        Light l(cala::Light::POINT, false, lightTransforms.back());
        l.setIntensity(ende::math::rand(10.f, 1000.f));
        l.setColour({ende::math::rand(0.f, 1.f), ende::math::rand(0.f, 1.f), ende::math::rand(0.f, 1.f)});
        scene.addLight(l);
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
            ImGui::Text("Draw Calls: %d", rendererStats.drawCallCount);

            auto pipelineStats = engine.driver().context().getPipelineStatistics();
            ImGui::Text("Input Assembly Vertices: %lu", pipelineStats.inputAssemblyVertices);
            ImGui::Text("Input Assembly Primitives: %lu", pipelineStats.inputAssemblyPrimitives);
            ImGui::Text("Vertex Shader Invocations: %lu", pipelineStats.vertexShaderInvocations);
            ImGui::Text("Clipping Invocations: %lu", pipelineStats.clippingInvocations);
            ImGui::Text("Clipping Primitives: %lu", pipelineStats.clippingPrimitives);
            ImGui::Text("Fragment Shader Invocations: %lu", pipelineStats.fragmentShaderInvocations);

            ende::math::Vec3f position = scene._lights[l0].transform().pos();
            ende::math::Vec3f colour = scene._lights[l0].getColour();
            f32 intensity = scene._lights[l0].getIntensity();
            f32 near = scene._lights[l0].getNear();
            f32 far = scene._lights[l0].getFar();
            if (ImGui::DragFloat3("Position", &position[0], 0.1, -sceneSize, sceneSize) ||
                ImGui::ColorEdit3("Colour", &colour[0]) ||
                ImGui::SliderFloat("Intensity", &intensity, 1, 1000) ||
                ImGui::SliderFloat("Near", &near, 0, 5) ||
                ImGui::SliderFloat("Far", &far, 1, 200)) {
                scene._lights[l0].setPosition(position);
                scene._lights[l0].setColour(colour);
                scene._lights[l0].setIntensity(intensity);
                scene._lights[l0].setNear(near);
                scene._lights[l0].setFar(far);
            }

            ImGui::DragFloat("Shadow Bias", &scene.shadowBias, 0.001, -1, 1);

            ende::math::Quaternion direction = scene._lights[l2].transform().rot();
            if (ImGui::DragFloat4("Direction", &direction[0], 0.01, -1, 1)) {
                scene._lights[l2].setDirection(direction);
            }

            ImGui::Text("Lights: %lu", scene._lights.size());


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