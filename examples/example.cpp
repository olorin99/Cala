#include <Cala/backend/vulkan/SDLPlatform.h>
#include <Cala/backend/vulkan/Driver.h>
#include <Cala/shapes.h>
#include <Cala/Camera.h>
#include <Cala/Light.h>
#include <Ende/filesystem/File.h>
#include <Cala/ImGuiContext.h>
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
#include <Ende/profile/ProfileManager.h>
#include <iostream>
#include <Cala/Model.h>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../third_party/tiny_gltf.h"

using namespace cala;
using namespace cala::backend;
using namespace cala::backend::vulkan;

ImageHandle loadImage(Engine& engine, const ende::fs::Path& path) {
    i32 width, height, channels;
    u8* data = stbi_load((*path).c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!data) throw "unable load image";
    u32 length = width * height * 4;

    ImageHandle handle = engine.createImage({(u32)width, (u32)height, 1, backend::Format::RGBA8_UNORM});

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

Model loadGLTF(Engine* engine, Material* material, const ende::fs::Path& path) {
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string err;
    std::string warn;
    loader.LoadASCIIFromFile(&model, &err, &warn, *path);

    ende::Vector<ImageHandle> images;

    for (u32 i = 0; i < model.images.size(); i++) {
        tinygltf::Image& image = model.images[i];
        u8* buf = nullptr;
        u32 bufferSize = 0;
        bool del = false;
        if (image.component == 3) {
            bufferSize = image.width * image.height * 4;
            buf = new u8[bufferSize];
            u8* rgba = buf;
            u8* rgb = &image.image[0];
            for (u32 j = 0; j < image.width * image.height; j++) {
                memcpy(rgba, rgb, sizeof(u8) * 3);
                rgba += 4;
                rgb += 3;
            }
            del = true;
        } else {
            buf = &image.image[0];
            bufferSize = image.image.size();
        }
        images.push(engine->createImage({
            (u32)image.width, (u32)image.height, 1,
            Format::RGBA8_SRGB,
            1, 1,
            ImageUsage::SAMPLED | ImageUsage::TRANSFER_DST
        }))->data(engine->driver(), {
            0, (u32)image.width, (u32)image.height, 1, 4,
            { buf, bufferSize}
        });
        if (del)
            delete buf;
    }

    ende::Vector<MaterialInstance> materials;
    struct PbrMat {
        u32 albedoIndex = 0;
        u32 normalIndex = 0;
        u32 metallicRoughnessIndex = 0;
        u32 aoIndex = 0;
    };

    for (u32 i = 0; i < model.materials.size(); i++) {
        tinygltf::Material& material1 = model.materials[i];
        PbrMat mat{};
        if (auto it = material1.values.find("baseColorTexture"); it != material1.values.end()) {
            images[model.textures[it->second.TextureIndex()].source].index();
            mat.albedoIndex = images[model.textures[it->second.TextureIndex()].source].index();
        } else
            mat.albedoIndex = engine->defaultAlbedo().index();
        if (auto it = material1.values.find("normalTexture"); it != material1.values.end()) {
            mat.normalIndex = images[model.textures[it->second.TextureIndex()].source].index();
        } else if (auto it1 = material1.additionalValues.find("normalTexture"); it1 != material1.additionalValues.end()) {
            mat.normalIndex = images[model.textures[it1->second.TextureIndex()].source].index();
        } else
            mat.normalIndex = engine->defaultNormal().index();
        if (auto it = material1.values.find("metallicRoughnessTexture"); it != material1.values.end()) {
            mat.metallicRoughnessIndex = images[model.textures[it->second.TextureIndex()].source].index();
        } else {
            mat.metallicRoughnessIndex = engine->defaultMetallic().index();
        }
        mat.aoIndex = engine->defaultAO().index();
        MaterialInstance instance = material->instance();
        instance.setUniform("material", mat);
        materials.push(std::move(instance));
    }

    ende::Vector<Vertex> vertices;
    ende::Vector<u32> indices;

    ende::Vector<Model::Primitive> primitives;

    for (auto& node : model.nodes) {
        if (node.mesh > -1) {
            tinygltf::Mesh mesh = model.meshes[node.mesh];
            for (u32 i = 0; i < mesh.primitives.size(); i++) {
                tinygltf::Primitive& primitive = mesh.primitives[i];
                u32 firstIndex = indices.size();
                u32 vertexStart = vertices.size();
                u32 indexCount = 0;
                ende::math::Vec3f min = {std::numeric_limits<f32>::max(), std::numeric_limits<f32>::max(), std::numeric_limits<f32>::max()};
                ende::math::Vec3f max = {std::numeric_limits<f32>::min(), std::numeric_limits<f32>::min(), std::numeric_limits<f32>::min()};

                {
                    f32* positions = nullptr;
                    f32* normals = nullptr;
                    f32* texCoords = nullptr;
                    u32 vertexCount = 0;
                    if (auto it = primitive.attributes.find("POSITION"); it != primitive.attributes.end()) {
                        tinygltf::Accessor& accessor = model.accessors[it->second];
                        tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
                        positions = (f32*)&model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset];
                        vertexCount = accessor.count;

                        for (u32 v = 0; v < vertexCount; v++) {
                            ende::math::Vec3f pos{positions[v * 3], positions[v * 3 + 1], positions[v * 3 + 2]};
                            if (pos.x() < min.x())
                                min[0] = pos.x();
                            if (pos.y() < min.y())
                                min[1] = pos.y();
                            if (pos.z() < min.z())
                                min[2] = pos.z();
                            if (pos.x() > max.x())
                                max[0] = pos.x();
                            if (pos.y() > max.y())
                                max[1] = pos.y();
                            if (pos.z() > max.z())
                                max[2] = pos.z();
                        }
                    }
                    if (auto it = primitive.attributes.find("NORMAL"); it != primitive.attributes.end()) {
                        tinygltf::Accessor& accessor = model.accessors[it->second];
                        tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
                        normals = (f32*)&model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset];
                    }
                    if (auto it = primitive.attributes.find("TEXCOORD_0"); it != primitive.attributes.end()) {
                        tinygltf::Accessor& accessor = model.accessors[it->second];
                        tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
                        texCoords = (f32*)&model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset];
                    }
                    for (u32 v = 0; v < vertexCount; v++) {
                        Vertex vertex{};
                        if (positions)
                            vertex.position = {positions[v * 3], positions[v * 3 + 1], positions[v * 3 + 2]};
                        if (normals)
                            vertex.normal = {normals[v * 3], normals[v * 3 + 1], normals[v * 3 + 2]};
                        if (texCoords)
                            vertex.texCoords = {texCoords[v * 2], texCoords[v * 2 + 1]};
                        vertices.push(vertex);
                    }
                }
                {
                    tinygltf::Accessor& accessor = model.accessors[primitive.indices];
                    tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
                    tinygltf::Buffer& buffer = model.buffers[view.buffer];
                    indexCount = accessor.count;
                    switch (accessor.componentType) {
                        case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
                            u32* buf = (u32*)&buffer.data[accessor.byteOffset + view.byteOffset];
                            for (u32 index = 0; index < accessor.count; index++)
                                indices.push(buf[index] + vertexStart);
                            break;
                        }
                        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
                            u16* buf = (u16*)&buffer.data[accessor.byteOffset + view.byteOffset];
                            for (u32 index = 0; index < accessor.count; index++)
                                indices.push(buf[index] + vertexStart);
                            break;
                        }
                        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
                            u8* buf = (u8*)&buffer.data[accessor.byteOffset + view.byteOffset];
                            for (u32 index = 0; index < accessor.count; index++)
                                indices.push(buf[index] + vertexStart);
                            break;
                        }
                        default:
                            break;
                    }
                }
                Model::Primitive prim{};
                prim.firstIndex = firstIndex;
                prim.indexCount = indexCount;
                prim.materialIndex = primitive.material;
                prim.aabb.min = min;
                prim.aabb.max = max;
                primitives.push(prim);
            }
        }
    }

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = 14 * sizeof(f32);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<backend::Attribute, 5> attributes = {
            backend::Attribute{0, 0, backend::AttribType::Vec3f},
            backend::Attribute{1, 0, backend::AttribType::Vec3f},
            backend::Attribute{2, 0, backend::AttribType::Vec2f},
            backend::Attribute{3, 0, backend::AttribType::Vec3f},
            backend::Attribute{4, 0, backend::AttribType::Vec3f}
    };

    BufferHandle vertexBuffer = engine->createBuffer(vertices.size() * sizeof(Vertex), BufferUsage::VERTEX);
    BufferHandle indexBuffer = engine->createBuffer(indices.size() * sizeof(u32), BufferUsage::INDEX);
    vertexBuffer->data({ vertices.data(), static_cast<u32>(vertices.size() * sizeof(Vertex)) });
    indexBuffer->data({ indices.data(), static_cast<u32>(indices.size() * sizeof(u32)) });

    Model mesh;
    mesh.vertexBuffer = vertexBuffer;
    mesh.indexBuffer = indexBuffer;
    mesh.primitives = std::move(primitives);
    mesh.images = std::move(images);
    mesh.materials = std::move(materials);
    mesh._binding = binding;
    mesh._attributes = attributes;
    return std::move(mesh);
}

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

int main() {
    SDLPlatform platform("hello_triangle", 1920, 1080);

    Engine engine(platform);
    Renderer renderer(&engine, {
//        .tonemap = false,
//        .skybox = false
    });

    ImGuiContext imGuiContext(engine.driver(), platform.window());


    //Shaders
    ProgramHandle pointLightProgram = engine.createProgram(loadShader(engine.driver(), "../../res/shaders/default.vert.spv"_path, "../../res/shaders/pbr.frag.spv"_path));
//    ProgramHandle directionalLightProgram = engine.createProgram(loadShader(engine.driver(), "../../res/shaders/direct_shadow.vert.spv"_path, "../../res/shaders/direct_pbr.frag.spv"_path));

    Material material(&engine, pointLightProgram);
    material._depthState = { true, true, CompareOp::LESS_EQUAL };

    Transform sponzaTransform({0, 0, 0}, {0, 0, 0, 1}, {0.008, 0.008, 0.008});
    auto sponza = loadGLTF(&engine, &material, "/home/christian/Downloads/gltf/glTF-Sample-Models/2.0/Sponza/glTF/Sponza.gltf"_path);


    Mesh cube = cala::shapes::cube().mesh(&engine);

    Transform cameraTransform({0, 0, -20});
    Camera camera((f32)ende::math::rad(54.4), platform.windowSize().first, platform.windowSize().second, 0.1f, 1000.f, cameraTransform);

    Sampler sampler(engine.driver(), {});

    auto matInstance = material.instance();

    struct MaterialData {
        u32 albedoIndex;
        u32 normalIndex;
        u32 metallicRoughness;
        u32 aoIndex;
    };
    MaterialData materialData {
            static_cast<u32>(engine.defaultAlbedo().index()),
            static_cast<u32>(engine.defaultNormal().index()),
            static_cast<u32>(engine.defaultMetallic().index()),
            static_cast<u32>(engine.defaultAO().index())
    };
    matInstance.setUniform("material", materialData);

    u32 objectCount = 100;
    Scene scene(&engine, objectCount);

    Transform lightTransform({-10, 0, 0}, {0, 0, 0, 1}, {0.1, 0.1, 0.1});
    Light light(cala::Light::POINT, true, lightTransform);
    light.setColour({1, 0, 0});
    light.setIntensity(50);
    Transform light1Transform({10, 2, 4});
    Light light1(cala::Light::POINT, false, light1Transform);
    light1.setColour({0, 1, 0});
    light1.setIntensity(10);
    Transform light2Transform({0, 0, 0}, ende::math::Quaternion({1, 0, 0}, ende::math::rad(-45)));
    Light light2(cala::Light::DIRECTIONAL, false, light2Transform);
    light2.setColour({0, 0, 1});
    light2.setIntensity(20);
    Transform light3Transform({-10, 2, 4});
    Light light3(cala::Light::POINT, false, light3Transform);
    light3.setIntensity(1);
    light3.setColour({0.23, 0.46, 0.10});

    u32 l0 = scene.addLight(light);
    u32 l1 = scene.addLight(light1);
    u32 l2 = scene.addLight(light2);
    u32 l3 = scene.addLight(light3);

    ImageHandle background = loadImageHDR(engine, "../../res/textures/TropicalRuins_3k.hdr"_path);
    scene.addSkyLightMap(background, true);

    scene.addRenderable(sponza, &sponzaTransform, true);

//    scene.addRenderable(cube, &matInstance, &modelTransform);
    scene.addRenderable(cube, &matInstance, &lightTransform, false);

    f32 sceneSize = std::max(objectCount / 10, 20u);

    ende::Vector<Transform> lightTransforms;
    lightTransforms.reserve(100);
    for (u32 i = 0; i < 0; i++) {
        lightTransforms.push(Transform({ende::math::rand(-sceneSize * 1.5f, sceneSize * 1.5f), ende::math::rand(-sceneSize * 1.5f, sceneSize * 1.5f), ende::math::rand(-sceneSize * 1.5f, sceneSize * 1.5f)}));
        Light l(cala::Light::POINT, false, lightTransforms.back());
        l.setIntensity(ende::math::rand(10.f, 1000.f));
        l.setColour({ende::math::rand(0.f, 1.f), ende::math::rand(0.f, 1.f), ende::math::rand(0.f, 1.f)});
        scene.addLight(l);
    }

    i32 lightIndex = 0;

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
            imGuiContext.processEvent(&event);
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

#ifdef ENDE_PROFILE
            {
                PROFILE_NAMED("show_profile");
                u32 currentProfileFrame = ende::profile::ProfileManager::getCurrentFrame();
                auto f = ende::profile::ProfileManager::getFrameData(currentProfileFrame);
                tsl::robin_map<const char*, std::pair<f64, u32>> data;
                for (auto& profileData : f) {
                    f64 diff = (profileData.end.nanoseconds() - profileData.start.nanoseconds()) / 1e6;
                    auto it = data.find(profileData.label);
                    if (it == data.end())
                        data.emplace(std::make_pair(profileData.label, std::make_pair(diff, 1)));
                    else {
                        it.value().first += diff;
                        it.value().second++;
                    }
                }

                ende::Vector<std::pair<const char*, std::pair<f64, u32>>> dataVec;

                for (auto& func : data) {
                    dataVec.push(std::make_pair(func.first, std::make_pair(func.second.first, func.second.second)));
                }
                std::sort(dataVec.begin(), dataVec.end(), [](std::pair<const char*, std::pair<f64, u32>> lhs, std::pair<const char*, std::pair<f64, u32>> rhs) -> bool {
                    return lhs.first > rhs.first;
                });
                for (auto& func : dataVec) {
                    ImGui::Text("\t%s ms: %f, avg: %f", func.first, func.second.first, func.second.first / func.second.second);
                }
            }
#endif

            ImGui::Text("GPU Times:");
            auto passTimers = renderer.timers();
            u64 totalGPUTime = 0;
            for (auto& timer : passTimers) {
                u64 time = timer.second.result();
                totalGPUTime += time;
                ImGui::Text("\t%s ms: %f", timer.first, time / 1e6);
            }
            ImGui::Text("Total GPU: %f", totalGPUTime / 1e6);

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

            ImGui::Text("Memory Usage: ");
            VmaBudget budgets[10]{};
            vmaGetHeapBudgets(engine.driver().context().allocator(), budgets);
            for (auto & budget : budgets) {
                if (budget.usage == 0)
                    break;
                ImGui::Text("\tUsed: %lu mb", budget.usage / 1000000);
                ImGui::Text("\tAvailable: %lu mb", budget.budget / 1000000);
            }

            ImGui::End();

            ImGui::Begin("Render Settings");
            auto& renderSettings = renderer.settings();
            ImGui::Checkbox("Forward Pass", &renderSettings.forward);
            ImGui::Checkbox("Depth Pre Pass", &renderSettings.depthPre);
            ImGui::Checkbox("Skybox Pass", &renderSettings.skybox);
            ImGui::Checkbox("Tonemap Pass", &renderSettings.tonemap);

            f32 gamma = renderer.getGamma();
            if (ImGui::SliderFloat("Gamma", &gamma, 0, 5))
                renderer.setGamma(gamma);

            ImGui::End();

            ImGui::Begin("Lights");
            ImGui::Text("Lights: %lu", scene._lights.size());
            ImGui::SliderInt("Light", &lightIndex, 0, scene._lights.size() - 1);

            auto& lightRef = scene._lights[lightIndex];

            ende::math::Vec3f position = lightRef.transform().pos();
            ende::math::Vec3f colour = lightRef.getColour();
            f32 intensity = lightRef.getIntensity();
            f32 near = lightRef.getNear();
            f32 far = lightRef.getFar();
            bool shadowing = lightRef.shadowing();

            if (lightRef.type() == cala::Light::POINT) {
                if (ImGui::DragFloat3("Position", &position[0], 0.1, -sceneSize, sceneSize))
                    lightRef.setPosition(position);
            } else {
                ende::math::Quaternion direction = lightRef.transform().rot();
                if (ImGui::DragFloat4("Direction", &direction[0], 0.01, -1, 1))
                    lightRef.setDirection(direction);
            }
            if (ImGui::ColorEdit3("Colour", &colour[0]))
                lightRef.setColour(colour);
            if (ImGui::SliderFloat("Intensity", &intensity, 1, 1000))
                lightRef.setIntensity(intensity);
            if (ImGui::SliderFloat("Near", &near, 0, 5))
                lightRef.setNear(near);
            if (ImGui::SliderFloat("Far", &far, 0, 1000))
                lightRef.setFar(far);
            ImGui::DragFloat("Shadow Bias", &scene.shadowBias, 0.001, -1, 1);
            if (ImGui::Checkbox("Shadowing", &shadowing))
                lightRef.setShadowing(shadowing);

            f32 exposure = camera.getExposure();
            if (ImGui::SliderFloat("Exposure", &exposure, 0, 10))
                camera.setExposure(exposure);

            ImGui::End();
            ImGui::Render();
        }

        {
//            modelTransform.rotate(ende::math::Vec3f{0, 1, 0}, ende::math::rad(45) * dt);
            lightTransform.rotate(ende::math::Vec3f{0, 1, 1}, ende::math::rad(45) * dt);
        }
        engine.gc();
        renderer.beginFrame();

        scene.prepare(renderer.frameIndex(), camera);

        renderer.render(scene, camera, &imGuiContext);

        dt = renderer.endFrame();

        ende::profile::ProfileManager::frame();
    }

    engine.driver().wait();
}