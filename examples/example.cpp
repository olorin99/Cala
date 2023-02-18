#include <Cala/backend/vulkan/SDLPlatform.h>
#include <Cala/shapes.h>
#include <Cala/Camera.h>
#include <Cala/Light.h>
#include <Ende/filesystem/File.h>
#include <Cala/ImGuiContext.h>
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
            mat.metallicRoughnessIndex = engine->defaultMetallicRoughness().index();
        }
        MaterialInstance instance = material->instance();
        instance.setData(mat);
        materials.push(std::move(instance));
    }

    ende::Vector<Vertex> vertices;
    ende::Vector<u32> indices;

    ende::Vector<Model::Primitive> primitives;

    for (auto& node : model.nodes) {
        if (node.mesh > -1) {
            ende::math::Mat4f transform = ende::math::identity<4, f32>();

            if (node.scale.size() == 3) {
                transform = ende::math::scale<4>(ende::math::Vec3f{ (f32)node.scale[0], (f32)node.scale[1], (f32)node.scale[2] });
            }

            tinygltf::Mesh mesh = model.meshes[node.mesh];
            for (u32 i = 0; i < mesh.primitives.size(); i++) {
                tinygltf::Primitive& primitive = mesh.primitives[i];
                u32 firstIndex = indices.size();
                u32 vertexStart = vertices.size();
                u32 indexCount = 0;
                ende::math::Vec3f min = {std::numeric_limits<f32>::max(), std::numeric_limits<f32>::max(), std::numeric_limits<f32>::max()};
                ende::math::Vec3f max = {-std::numeric_limits<f32>::max(), -std::numeric_limits<f32>::max(), -std::numeric_limits<f32>::max()};

                {
                    f32* positions = nullptr;
                    f32* normals = nullptr;
                    f32* texCoords = nullptr;
                    f32* tangents = nullptr;
                    u32 vertexCount = 0;
                    if (auto it = primitive.attributes.find("POSITION"); it != primitive.attributes.end()) {
                        tinygltf::Accessor& accessor = model.accessors[it->second];
                        tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
                        positions = (f32*)&model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset];
                        vertexCount = accessor.count;

                        for (u32 v = 0; v < vertexCount; v++) {
                            ende::math::Vec3f pos{positions[v * 3], positions[v * 3 + 1], positions[v * 3 + 2]};
                            pos = transform.transform(pos);
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
                            positions[v * 3] = pos.x();
                            positions[v * 3 + 1] = pos.y();
                            positions[v * 3 + 2] = pos.z();
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
                    if (auto it = primitive.attributes.find("TANGENT"); it != primitive.attributes.end()) {
                        tinygltf::Accessor& accessor = model.accessors[it->second];
                        tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
                        tangents = (f32*)&model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset];
                    }
                    for (u32 v = 0; v < vertexCount; v++) {
                        Vertex vertex{};
                        if (positions)
                            vertex.position = {positions[v * 3], positions[v * 3 + 1], positions[v * 3 + 2]};
                        if (normals)
                            vertex.normal = {normals[v * 3], normals[v * 3 + 1], normals[v * 3 + 2]};
                        if (texCoords)
                            vertex.texCoords = {texCoords[v * 2], texCoords[v * 2 + 1]};
                        if (tangents)
                            vertex.tangent = { tangents[v * 4], tangents[v * 4 + 1], tangents[v * 4 + 2], tangents[v * 4 + 3] };
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
    binding.stride = 12 * sizeof(f32);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<backend::Attribute, 4> attributes = {
            backend::Attribute{0, 0, backend::AttribType::Vec3f},
            backend::Attribute{1, 0, backend::AttribType::Vec3f},
            backend::Attribute{2, 0, backend::AttribType::Vec2f},
            backend::Attribute{3, 0, backend::AttribType::Vec4f}
    };

//    BufferHandle vertexBuffer = engine->createBuffer(vertices.size() * sizeof(Vertex), BufferUsage::VERTEX);
//    BufferHandle indexBuffer = engine->createBuffer(indices.size() * sizeof(u32), BufferUsage::INDEX);
//    vertexBuffer->data({ vertices.data(), static_cast<u32>(vertices.size() * sizeof(Vertex)) });
//    indexBuffer->data({ indices.data(), static_cast<u32>(indices.size() * sizeof(u32)) });

    u32 vertexOffset = engine->uploadVertexData({ reinterpret_cast<f32*>(vertices.data()), static_cast<u32>(vertices.size() * sizeof(Vertex)) });
    for (auto& index : indices)
        index += vertexOffset / sizeof(Vertex);
    u32 indexOffset = engine->uploadIndexData({ reinterpret_cast<u32*>(indices.data()), static_cast<u32>(indices.size() * sizeof(u32)) });
    for (auto& primitive : primitives)
        primitive.firstIndex += indexOffset / sizeof(u32);


    Model mesh;
//    mesh.vertexBuffer = engine;
//    mesh.indexBuffer = indexBuffer;
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

ShaderProgram loadShader(Device& driver, const ende::fs::Path& vertex, const ende::fs::Path& fragment) {
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
    Renderer renderer(&engine, {});
    engine.driver().swapchain().setVsync(true);

    ImGuiContext imGuiContext(engine.driver(), platform.window());


    //Shaders
    ProgramHandle pointLightProgram = engine.createProgram(loadShader(engine.driver(), "../../res/shaders/default.vert.spv"_path, "../../res/shaders/pbr.frag.spv"_path));
//    ProgramHandle directionalLightProgram = engine.createProgram(loadShader(engine.driver(), "../../res/shaders/direct_shadow.vert.spv"_path, "../../res/shaders/direct_pbr.frag.spv"_path));

    Material material(&engine, pointLightProgram, sizeof(u32) * 3);
    material._depthState = { true, true, CompareOp::LESS_EQUAL };

    Transform sponzaTransform;

    Transform helmetTransform({0, 1, 0});

    Mesh cube = cala::shapes::cube().mesh(&engine);
//    Mesh sphere = cala::shapes::sphereNormalized(1).mesh(&engine);
    Mesh sphere = loadModel("../../res/models/sphere.obj"_path).mesh(&engine);;
    auto sponza = loadGLTF(&engine, &material, "/home/christian/Downloads/gltf/glTF-Sample-Models/2.0/Sponza/glTF/Sponza.gltf"_path);
//    auto sponza = loadGLTF(&engine, &material, "/home/christian/Downloads/Main.1_Sponza/NewSponza_Main_glTF_002.gltf"_path);
//    auto damagedHelmet = loadGLTF(&engine, &material, "/home/christian/Downloads/gltf/glTF-Sample-Models/2.0/SciFiHelmet/glTF/SciFiHelmet.gltf"_path);
    Model damagedHelmet;
//    bool addHelmet = false;


    Transform cameraTransform({10, 1.3, 0}, ende::math::Quaternion({0, 1, 0}, ende::math::rad(-90)));
    Camera camera((f32)ende::math::rad(54.4), platform.windowSize().first, platform.windowSize().second, 0.1f, 1000.f, cameraTransform);

    Sampler sampler(engine.driver(), {});

    auto matInstance = material.instance();

    struct MaterialData {
        u32 albedoIndex;
        u32 normalIndex;
        u32 metallicRoughness;
    };
    MaterialData materialData {
            static_cast<u32>(engine.defaultAlbedo().index()),
            static_cast<u32>(engine.defaultNormal().index()),
            static_cast<u32>(engine.defaultMetallicRoughness().index())
    };
    matInstance.setData(materialData);

    u32 objectCount = 20;
    Scene scene(&engine, objectCount);

    Transform lightTransform({-10, 1, 0}, {0, 0, 0, 1}, {0.1, 0.1, 0.1});
    Light light(cala::Light::POINT, true, lightTransform);
    light.setColour({1, 1, 1});
    light.setIntensity(20);
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
//    u32 l1 = scene.addLight(light1);
//    u32 l2 = scene.addLight(light2);
//    u32 l3 = scene.addLight(light3);

    ImageHandle background = loadImageHDR(engine, "../../res/textures/TropicalRuins_3k.hdr"_path);
//    ImageHandle background = loadImageHDR(engine, "../../res/textures/brown_photostudio_02_4k.hdr"_path);
//    ImageHandle background = loadImageHDR(engine, "../../res/textures/dresden_station_night_4k.hdr"_path);
    scene.addSkyLightMap(background, true);

    scene.addRenderable(sponza, &sponzaTransform, true);
//    scene.addRenderable(damagedHelmet, &helmetTransform, true);

    scene.addRenderable(sphere, &matInstance, &lightTransform, false);

    f32 sceneSize = std::max(objectCount / 10, 20u);

    MaterialInstance instances[10] = {
            material.instance(),
            material.instance(),
            material.instance(),
            material.instance(),
            material.instance(),
            material.instance(),
            material.instance(),
            material.instance(),
            material.instance(),
            material.instance()
    };
    ImageHandle roughnessImages[10];
    for (u32 i = 0; i < 10; i++) {
        roughnessImages[i] = engine.createImage({1, 1, 1, backend::Format::RGBA32_SFLOAT, 1, 1, backend::ImageUsage::SAMPLED | backend::ImageUsage::TRANSFER_DST, backend::ImageLayout::GENERAL, backend::ImageType::IMAGE2D});
        f32 metallicRoughnessData[] = { 0.f, static_cast<f32>(i) / 10.f, 0.f, 1.f };
        roughnessImages[i]->data(engine.driver(), { 0, 1, 1, 1, 4 * 4, { metallicRoughnessData, sizeof(f32) * 4 } });

        MaterialData materialData1 {
                static_cast<u32>(engine.defaultAlbedo().index()),
                static_cast<u32>(engine.defaultNormal().index()),
                static_cast<u32>(roughnessImages[i].index())
        };

        instances[i].setData(materialData1);
    }


    u32 width = 10;
    u32 height = 10;
    u32 depth = 10;

    Transform transform;
    ende::Vector<Transform> transforms;
    transforms.reserve(width * height * depth * 10);
    for (u32 i = 0; i < width; i++) {
        auto xpos = transform.pos();
        for (u32 j = 0; j < height; j++) {
            auto ypos = transform.pos();
            for (u32 k = 0; k < depth; k++) {
                transform.addPos({0, 0, 3});
                auto& t = transforms.push(transform);
                scene.addRenderable(sphere, &instances[k], &t, false);
            }
            transform.setPos(ypos + ende::math::Vec3f{0, 3, 0});
        }
        transform.setPos(xpos + ende::math::Vec3f{3, 0, 0});
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

    i32 lightIndex = 0;

    std::array<f32, 60> frameTimes{};

    tsl::robin_map<const char*, std::array<f32, 60>> funcFrameTimes;

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

//        if (addHelmet) {
//            scene.addRenderable(damagedHelmet, &helmetTransform, true);
//            addHelmet = false;
//        }

        {
            imGuiContext.newFrame();

            ImGui::Begin("Stats");
            ImGui::Text("FPS: %f", engine.driver().fps());


            ImGui::BeginTable("Times", 2);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);

            ImGui::Text("Milliseconds: %f", engine.driver().milliseconds());

            ImGui::TableNextColumn();

            std::rotate(frameTimes.begin(), frameTimes.begin() + 1, frameTimes.end());
            frameTimes.back() = engine.driver().milliseconds();
            ImGui::PlotLines("Milliseconds", &frameTimes[0], frameTimes.size());

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);

            ImGui::Text("CPU Times:");
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);

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
                    ImGui::Text("\t%s ms", func.first);
                    auto it = funcFrameTimes.find(func.first);
                    if (it == funcFrameTimes.end()) {
                        funcFrameTimes.insert(std::make_pair(func.first, std::array<f32, 60>{}));
                        it = funcFrameTimes.find(func.first);
                    }
                    std::rotate(it.value().begin(), it.value().begin() + 1, it.value().end());
                    it.value().back() = func.second.first;
                    ImGui::TableNextColumn();
                    ImGui::PlotLines(std::to_string(func.second.first).c_str(), &it.value()[0], it.value().size());
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                }
            }
#endif

            ImGui::Text("GPU Times:");
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            auto passTimers = renderer.timers();
            u64 totalGPUTime = 0;
            for (auto& timer : passTimers) {
                u64 time = timer.second.result();
                totalGPUTime += time;
                ImGui::Text("\t%s ms", timer.first);
                auto it = funcFrameTimes.find(timer.first);
                if (it == funcFrameTimes.end()) {
                    funcFrameTimes.insert(std::make_pair(timer.first, std::array<f32, 60>{}));
                    it = funcFrameTimes.find(timer.first);
                }
                std::rotate(it.value().begin(), it.value().begin() + 1, it.value().end());
                it.value().back() = time / 1e6;
                ImGui::TableNextColumn();
                ImGui::PlotLines(std::to_string(time / 1e6).c_str(), &it.value()[0], it.value().size());
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
            }
            ImGui::EndTable();

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

            auto engineStats = engine.stats();
            ImGui::Text("Allocated Buffers: %d", engineStats.allocatedBuffers);
            ImGui::Text("Buffers In Use: %d", engineStats.buffersInUse);
            ImGui::Text("Allocated Images: %d", engineStats.allocatedImages);
            ImGui::Text("Images In Use: %d", engineStats.imagesInUse);

            ImGui::End();

            ImGui::Begin("Render Settings");
            auto& renderSettings = renderer.settings();
            ImGui::Checkbox("Forward Pass", &renderSettings.forward);
            ImGui::Checkbox("Depth Pre Pass", &renderSettings.depthPre);
            ImGui::Checkbox("Skybox Pass", &renderSettings.skybox);
            ImGui::Checkbox("Tonemap Pass", &renderSettings.tonemap);
            ImGui::Checkbox("Freeze Frustum,", &renderSettings.freezeFrustum);
            ImGui::Checkbox("IBL,", &renderSettings.ibl);
            bool vsync = engine.driver().swapchain().getVsync();
            if (ImGui::Checkbox("Vsync", &vsync)) {
                engine.driver().wait();
                engine.driver().swapchain().setVsync(vsync);
            }

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

            if (ImGui::Button("Add Light")) {
                auto& t = lightTransforms.push(Transform({ende::math::rand(-sceneSize * 1.5f, sceneSize * 1.5f), ende::math::rand(-sceneSize * 1.5f, sceneSize * 1.5f), ende::math::rand(-sceneSize * 1.5f, sceneSize * 1.5f)}));
                Light l(Light::LightType::POINT, false, t);
                l.setIntensity(ende::math::rand(1.f, 20.f));
                l.setColour({ ende::math::rand(0.f, 1.f), ende::math::rand(0.f, 1.f), ende::math::rand(0.f, 1.f) });
                scene.addLight(l);
            }

            auto pos = camera.transform().pos();
            ImGui::Text("Position: { %f, %f, %f }", pos.x(), pos.y(), pos.z());

            if (ImGui::Button("Add 10")) {
                transform.setPos({ (f32)width * 3, 0, 0 });
                for (u32 i = 0; i < depth; i++) {
                    auto& t = transforms.push(transform);
                    scene.addRenderable(cube, &matInstance, &t, false);
                    transform.addPos({0, 0, 3});
                }
                width += 3;
            }

            if (ImGui::Button("Load")) {
                damagedHelmet = std::move(loadGLTF(&engine, &material, "/home/christian/Downloads/gltf/glTF-Sample-Models/2.0/SciFiHelmet/glTF/SciFiHelmet.gltf"_path));
//                addHelmet = true;
                scene.addRenderable(damagedHelmet, &helmetTransform, true);
            }

            ImGui::End();
            ImGui::Render();
        }

        {
            helmetTransform.rotate(ende::math::Vec3f{0, 1, 0}, ende::math::rad(45) * dt);
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