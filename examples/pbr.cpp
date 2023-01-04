#include <Cala/backend/vulkan/SDLPlatform.h>
#include <Cala/backend/vulkan/Driver.h>
#include <Cala/shapes.h>
#include <Cala/Camera.h>
#include <Cala/Light.h>
#include <Ende/filesystem/File.h>
#include <Cala/ImGuiContext.h>
#include <Ende/math/random.h>
#include "../../third_party/stb_image.h"
#include "Cala/Scene.h"
#include "Cala/Material.h"
#include <Cala/Probe.h>
#include <Cala/backend/vulkan/Timer.h>
#include <Cala/Engine.h>
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

Image loadImageHDR(Driver& driver, const ende::fs::Path& path) {
    stbi_set_flip_vertically_on_load(true);
    i32 width, height, channels;
    f32* data = stbi_loadf((*path).c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!data) throw "unable load image";
    u32 length = width * height * 4 * 4;

    Image image(driver, {(u32)width, (u32)height, 1, backend::Format::RGBA32_SFLOAT});
    image.data(driver, {0, (u32)width, (u32)height, 1, (u32)4 * 4, {data, length}});
    stbi_image_free(data);
    return image;
}

ende::math::Vec3f lerpPositions(ende::Span<ende::math::Vec3f> inputs, f32 factor) {
    ende::Vector<ende::math::Vec3f> positions(inputs.size(), inputs.begin());
    ende::Vector<ende::math::Vec3f> tmpPositions;

    while (positions.size() > 1) {
        for (u32 i = 0; i < positions.size(); i++) {
//            ende::math::Vec3f pos;
//            if (i == positions.size() - 1)
//                pos = positions[0];
//            else
//                pos = positions[i + 1];

            if (i + 1 >= positions.size())
                break;
            tmpPositions.push(positions[i].lerp(positions[i+1], factor));
        }
        positions = tmpPositions;
        tmpPositions.clear();
    }
    return positions.front();
}

int main() {
    SDLPlatform platform("hello_triangle", 800, 600);
    Engine engine(platform, false);
    auto& driver = engine.driver();

    ImGuiContext imGuiContext(driver, platform.window());

    ShaderProgram skybox = loadShader(driver, "../../res/shaders/skybox.vert.spv"_path, "../../res/shaders/skybox.frag.spv"_path);

    //Shaders
    ProgramHandle program = engine.createProgram(loadShader(driver, "../../res/shaders/default.vert.spv"_path, "../../res/shaders/pbr.frag.spv"_path));

    Material material(&engine);
    material.setProgram(cala::Material::Variants::POINT, program);
    material._depthState = { true, true };

    auto matInstance = material.instance();

//    Image brickwall_albedo = loadImage(driver, "../../res/textures/pbr_gold/lightgold_albedo.png"_path);
//    Image brickwall_normal = loadImage(driver, "../../res/textures/pbr_gold/lightgold_normal-ogl.png"_path);
//    Image brickwall_metallic = loadImage(driver, "../../res/textures/pbr_gold/lightgold_metallic.png"_path);
//    Image brickwall_roughness = loadImage(driver, "../../res/textures/pbr_gold/lightgold_roughness.png"_path);

    Image brickwall_albedo = loadImage(driver, "../../res/textures/pbr_rusted_iron/rustediron2_basecolor.png"_path);
    Image brickwall_normal = loadImage(driver, "../../res/textures/pbr_rusted_iron/rustediron2_normal.png"_path);
    Image brickwall_metallic = loadImage(driver, "../../res/textures/pbr_rusted_iron/rustediron2_metallic.png"_path);
    Image brickwall_roughness = loadImage(driver, "../../res/textures/pbr_rusted_iron/rustediron2_roughness.png"_path);
    Image brickwall_ao = loadImage(driver, "../../res/textures/pbr_rusted_iron/rustediron2_ao.png"_path);
    matInstance.setSampler("albedoMap", brickwall_albedo.getView(), Sampler(driver, {}));
    matInstance.setSampler("normalMap", brickwall_normal.getView(), Sampler(driver, {}));
    matInstance.setSampler("metallicMap", brickwall_metallic.getView(), Sampler(driver, {}));
    matInstance.setSampler("roughnessMap", brickwall_roughness.getView(), Sampler(driver, {}));
    matInstance.setSampler("aoMap", brickwall_ao.getView(), Sampler(driver, {}));

//    Image hdr = loadImageHDR(driver, "../../res/textures/Tropical_Beach_3k.hdr"_path);
    Image hdr = loadImageHDR(driver, "../../res/textures/TropicalRuins_3k.hdr"_path);
    ende::fs::File shaderFile;
    if (!shaderFile.open("../../res/shaders/equirectangularToCubeMap.comp.spv"_path, ende::fs::in | ende::fs::binary))
        return -2;
    ende::Vector<u32> computeShaderData(shaderFile.size() / sizeof(u32));
    shaderFile.read({reinterpret_cast<char*>(computeShaderData.data()), static_cast<u32>(computeShaderData.size() * sizeof(u32))});
    ShaderProgram toCubeCompute = ShaderProgram::create()
            .addStage(computeShaderData, ShaderStage::COMPUTE)
            .compile(driver);
    if (!shaderFile.open("../../res/shaders/irradiance.comp.spv"_path, ende::fs::in | ende::fs::binary))
        return -2;
    ende::Vector<u32> irradianceShaderData(shaderFile.size() / sizeof(u32));
    shaderFile.read({reinterpret_cast<char*>(irradianceShaderData.data()), static_cast<u32>(irradianceShaderData.size() * sizeof(u32))});
    ShaderProgram irradianceCompute = ShaderProgram::create()
            .addStage(irradianceShaderData, ShaderStage::COMPUTE)
            .compile(driver);
    if (!shaderFile.open("../../res/shaders/prefilter.comp.spv"_path, ende::fs::in | ende::fs::binary))
        return -2;
    ende::Vector<u32> prefilterShaderData(shaderFile.size() / sizeof(u32));
    shaderFile.read({reinterpret_cast<char*>(prefilterShaderData.data()), static_cast<u32>(prefilterShaderData.size() * sizeof(u32))});
    ShaderProgram prefilterCompute = ShaderProgram::create()
            .addStage(prefilterShaderData, ShaderStage::COMPUTE)
            .compile(driver);
    if (!shaderFile.open("../../res/shaders/brdf.comp.spv"_path, ende::fs::in | ende::fs::binary))
        return -2;
    ende::Vector<u32> brdfShaderData(shaderFile.size() / sizeof(u32));
    shaderFile.read({reinterpret_cast<char*>(brdfShaderData.data()), static_cast<u32>(brdfShaderData.size() * sizeof(u32))});
    ShaderProgram brdfCompute = ShaderProgram::create()
            .addStage(brdfShaderData, ShaderStage::COMPUTE)
            .compile(driver);



    Timer toCubeTimer(driver);
    Timer irradianceTimer(driver, 1);
    Timer prefilterTimer(driver, 2);
    Timer brdfTimer(driver, 3);



    Sampler s1(driver, {});
    Image::View hdrView = hdr.getView();

    Transform cameraTransform({0, 0, -15});
    Camera camera(ende::math::perspective((f32)ende::math::rad(54.4), 800.f / 600.f, 0.1f, 1000.f), cameraTransform);
    Buffer cameraBuffer(driver, sizeof(Camera::Data), BufferUsage::UNIFORM);

    Scene scene(&engine, 10);
//    Mesh cube = shapes::cube().mesh(driver);
//    Mesh cube = shapes::sphereUV(1).mesh(driver);
    Mesh cube = loadModel("../../res/models/sphere.obj"_path).mesh(driver);
//    Mesh cube = shapes::sphereNormalized(1).mesh(driver);
//    Mesh cube = shapes::sphereCube(1).mesh(driver);

    struct PBRMaterial {
        Image* albedo;
        Image* normal;
        Image* metallic;
        Image* roughness;
    };

    Image albedo(driver, {
        1, 1, 1,
        Format::RGBA32_SFLOAT,
        1, 1,
        ImageUsage::SAMPLED | ImageUsage::TRANSFER_DST,
        ImageLayout::GENERAL,
        ImageType::IMAGE2D
    });
    f32 albedoData[] = { 0, 0, 0, 1 };
    albedo.data(driver, {
        0, 1, 1, 1, 4 * 4,
        { albedoData, sizeof(f32) * 4 }
    });

    Image normal(driver, {
            1, 1, 1,
            Format::RGBA32_SFLOAT,
            1, 1,
            ImageUsage::SAMPLED | ImageUsage::TRANSFER_DST,
            ImageLayout::GENERAL,
            ImageType::IMAGE2D
    });
    f32 normalData[] = { 0.52, 0.52, 1, 1 };
    normal.data(driver, {
            0, 1, 1, 1, 4 * 4,
            { normalData, sizeof(f32) * 4 }
    });


    ende::Vector<Image> images;
    images.reserve(20);

    for (f32 metallic = 0; metallic <= 1.01f; metallic += 0.1f) {
        images.emplace(Image{driver, {
            1, 1, 1,
            Format::R32_SFLOAT,
            1, 1,
            ImageUsage::SAMPLED | ImageUsage::TRANSFER_DST,
            ImageLayout::GENERAL,
            ImageType::IMAGE2D
        }}).data(driver, { 0, 1, 1, 1, 4, { &metallic, sizeof(f32) }});
    }
    for (f32 roughness = 0; roughness <= 1.01f; roughness += 0.1) {
        images.emplace(Image{driver, {
                1, 1, 1,
                Format::R32_SFLOAT,
                1, 1,
                ImageUsage::SAMPLED | ImageUsage::TRANSFER_DST,
                ImageLayout::GENERAL,
                ImageType::IMAGE2D
        }}).data(driver, { 0, 1, 1, 1, 4, { &roughness, sizeof(f32) }});
    }

    ende::Vector<MaterialInstance> instances;
    instances.reserve(200);

    ende::Vector<cala::Transform> transforms;
    transforms.reserve(200);

    for (u32 x = 0; x <= 10; x++) {
        for (u32 y = 0; y <= 10; y++) {

            auto& metallic = images[x];
            auto& roughness = images[y];

            auto& instance = instances.push(material.instance());
            instance.setSampler("albedoMap", albedo.getView(), Sampler(driver, {}));
            instance.setSampler("normalMap", normal.getView(), Sampler(driver, {}));
            instance.setSampler("metallicMap", metallic.getView(), Sampler(driver, {}));
            instance.setSampler("roughnessMap", roughness.getView(), Sampler(driver, {}));
            instance.setSampler("aoMap", brickwall_ao.getView(), Sampler(driver, {}));

            transforms.push(Transform({(f32)x * 3, (f32)y * 3, 0}));

            scene.addRenderable(cube, &instance, &transforms.back());

        }
    }

    ende::Vector<ende::math::Vec3f> lightPositions = {
            {0, 0, 0},
            {0, 10, 0},
            {10, 10, 0},
            {10, 0, 0},
            {0, 0, 0}
    };

    Transform lightTransform({ -3, 3, -1 });
    Light light(Light::POINT, false, lightTransform);

    scene.addLight(light);
//    scene.addRenderable(cube, &matInstance, &lightTransform);

    Sampler sampler(driver, {});
    Sampler prefilterSampler(driver, {
        .maxLod = 10
    });


    Image environmentMap(driver, {
            512,
            512,
            1,
            Format::RGBA32_SFLOAT,
            10,
            6,
            ImageUsage::STORAGE | ImageUsage::SAMPLED | backend::ImageUsage::TRANSFER_DST | backend::ImageUsage::TRANSFER_SRC
    });
    auto environmentView = environmentMap.getView(VK_IMAGE_VIEW_TYPE_CUBE, 0, 10);

    driver.immediate([&](CommandBuffer& cmd) {
        toCubeTimer.start(cmd);
        auto envBarrier = environmentMap.barrier(backend::Access::NONE, backend::Access::SHADER_WRITE, backend::ImageLayout::UNDEFINED, backend::ImageLayout::GENERAL);
        cmd.pipelineBarrier(backend::PipelineStage::TOP, backend::PipelineStage::COMPUTE_SHADER, 0, nullptr, { &envBarrier, 1 });
        auto hdrBarrier = hdr.barrier(backend::Access::NONE, backend::Access::SHADER_READ, backend::ImageLayout::UNDEFINED, backend::ImageLayout::SHADER_READ_ONLY);
        cmd.pipelineBarrier(backend::PipelineStage::TOP, backend::PipelineStage::COMPUTE_SHADER, 0, nullptr, { &hdrBarrier, 1 });


        cmd.bindProgram(toCubeCompute);
        cmd.bindImage(1, 0, hdrView, sampler);
        cmd.bindImage(1, 1, environmentView, sampler, true);

        cmd.bindPipeline();
        cmd.bindDescriptors();

        cmd.dispatchCompute(512 / 32, 512 / 32, 6);


        envBarrier = environmentMap.barrier(backend::Access::SHADER_WRITE, backend::Access::NONE, backend::ImageLayout::GENERAL, backend::ImageLayout::TRANSFER_DST);
        cmd.pipelineBarrier(backend::PipelineStage::COMPUTE_SHADER, backend::PipelineStage::BOTTOM, 0, nullptr, { &envBarrier, 1 });
        toCubeTimer.stop();
    });

    environmentMap.generateMips();

    Image irradianceMap(driver, {
            512,
            512,
            1,
            Format::RGBA32_SFLOAT,
            1,
            6,
            ImageUsage::STORAGE | ImageUsage::SAMPLED | backend::ImageUsage::TRANSFER_DST
    });
    auto irradianceView = irradianceMap.getView(VK_IMAGE_VIEW_TYPE_CUBE);


    driver.immediate([&](CommandBuffer& cmd) {
        irradianceTimer.start(cmd);
        auto irradianceBarrier = irradianceMap.barrier(backend::Access::NONE, backend::Access::SHADER_READ, backend::ImageLayout::UNDEFINED, backend::ImageLayout::GENERAL);
        cmd.pipelineBarrier(backend::PipelineStage::TOP, backend::PipelineStage::COMPUTE_SHADER, 0, nullptr, { &irradianceBarrier, 1 });


        cmd.bindProgram(irradianceCompute);
        cmd.bindImage(1, 0, environmentView, sampler);
        cmd.bindImage(1, 1, irradianceView, sampler, true);

        cmd.bindPipeline();
        cmd.bindDescriptors();

        cmd.dispatchCompute(512 / 32, 512 / 32, 6);

        irradianceBarrier = irradianceMap.barrier(backend::Access::SHADER_WRITE, backend::Access::NONE, backend::ImageLayout::UNDEFINED, backend::ImageLayout::SHADER_READ_ONLY);
        cmd.pipelineBarrier(backend::PipelineStage::COMPUTE_SHADER, backend::PipelineStage::BOTTOM, 0, nullptr, { &irradianceBarrier, 1 });
        irradianceTimer.stop();
    });

    Image prefilterMap(driver, {
            512,
            512,
            1,
            Format::RGBA32_SFLOAT,
            5,
            6,
            ImageUsage::STORAGE | ImageUsage::SAMPLED | backend::ImageUsage::TRANSFER_DST
    });
    Image::View prefilterViews[5] = {
            prefilterMap.getView(VK_IMAGE_VIEW_TYPE_CUBE, 0),
            prefilterMap.getView(VK_IMAGE_VIEW_TYPE_CUBE, 1),
            prefilterMap.getView(VK_IMAGE_VIEW_TYPE_CUBE, 2),
            prefilterMap.getView(VK_IMAGE_VIEW_TYPE_CUBE, 3),
            prefilterMap.getView(VK_IMAGE_VIEW_TYPE_CUBE, 4)
    };

    Buffer roughnessBuf(driver, sizeof(f32) * 6, BufferUsage::UNIFORM);
    f32 roughnessData[] = { 0 / 4.f, 1 / 4.f, 2 / 4.f, 3 / 4.f, 4.f / 4.f };
    roughnessBuf.data({ roughnessData, sizeof(f32) * 5 });
    driver.immediate([&](CommandBuffer& cmd) {
        prefilterTimer.start(cmd);
        auto prefilterBarrier = prefilterMap.barrier(backend::Access::NONE, backend::Access::SHADER_WRITE, backend::ImageLayout::UNDEFINED, backend::ImageLayout::GENERAL);
        cmd.pipelineBarrier(backend::PipelineStage::TOP, backend::PipelineStage::COMPUTE_SHADER, 0, nullptr, { &prefilterBarrier, 1 });

        for (u32 mip = 0; mip < 5; mip++) {

            cmd.bindProgram(prefilterCompute);
            cmd.bindImage(1, 0, environmentView, sampler);
            cmd.bindImage(1, 1, prefilterViews[mip], sampler, true);
            cmd.bindBuffer(2, 2, roughnessBuf, sizeof(f32) * mip, sizeof(f32));

            cmd.bindPipeline();
            cmd.bindDescriptors();
            cmd.dispatchCompute(512 * std::pow(0.5, mip) / 32, 512 * std::pow(0.5, mip) / 32, 6);

        }

        prefilterBarrier = prefilterMap.barrier(backend::Access::SHADER_WRITE, backend::Access::SHADER_READ, backend::ImageLayout::UNDEFINED, backend::ImageLayout::SHADER_READ_ONLY);
        cmd.pipelineBarrier(backend::PipelineStage::COMPUTE_SHADER, backend::PipelineStage::FRAGMENT_SHADER, 0, nullptr, { &prefilterBarrier, 1 });
        prefilterTimer.stop();
    });

    auto prefilterView = prefilterMap.getView(VK_IMAGE_VIEW_TYPE_CUBE, 0, 5);


    Image brdfMap(driver, {
        512,
        512,
        1,
        Format::RG16_SFLOAT,
        1,
        1,
        ImageUsage::SAMPLED | ImageUsage::STORAGE
    });
    auto brdfView = brdfMap.getView();

    driver.immediate([&](CommandBuffer& cmd) {
        brdfTimer.start(cmd);
        auto brdfBarrier = brdfMap.barrier(backend::Access::NONE, backend::Access::SHADER_READ, backend::ImageLayout::UNDEFINED, backend::ImageLayout::GENERAL);
        cmd.pipelineBarrier(backend::PipelineStage::TOP, backend::PipelineStage::COMPUTE_SHADER, 0, nullptr, { &brdfBarrier, 1 });


        cmd.bindProgram(brdfCompute);
        cmd.bindImage(1, 0, brdfView, sampler, true);

        cmd.bindPipeline();
        cmd.bindDescriptors();

        cmd.dispatchCompute(512 / 32, 512 / 32, 1);

        brdfBarrier = brdfMap.barrier(backend::Access::SHADER_WRITE, backend::Access::NONE, backend::ImageLayout::UNDEFINED, backend::ImageLayout::SHADER_READ_ONLY);
        cmd.pipelineBarrier(backend::PipelineStage::COMPUTE_SHADER, backend::PipelineStage::BOTTOM, 0, nullptr, { &brdfBarrier, 1 });
        brdfTimer.stop();
    });

    u64 times[4] = {
            toCubeTimer.result(),
            irradianceTimer.result(),
            prefilterTimer.result(),
            brdfTimer.result()
    };


    RenderPass::Attachment attachments[3] = {
            {
                    driver.swapchain().format(),
                    VK_SAMPLE_COUNT_1_BIT,
                    VK_ATTACHMENT_LOAD_OP_CLEAR,
                    VK_ATTACHMENT_STORE_OP_STORE,
                    VK_ATTACHMENT_LOAD_OP_CLEAR,
                    VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            },
            {
                    Format::RGBA32_SFLOAT,
                    VK_SAMPLE_COUNT_1_BIT,
                    VK_ATTACHMENT_LOAD_OP_CLEAR,
                    VK_ATTACHMENT_STORE_OP_STORE,
                    VK_ATTACHMENT_LOAD_OP_CLEAR,
                    VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            },
            {
                    Format::D32_SFLOAT,
                    VK_SAMPLE_COUNT_1_BIT,
                    VK_ATTACHMENT_LOAD_OP_CLEAR,
                    VK_ATTACHMENT_STORE_OP_STORE,
                    VK_ATTACHMENT_LOAD_OP_CLEAR,
                    VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
            }
    };
    RenderPass renderPass(driver, attachments);

    Image colourBuffer(driver, {
        800, 600, 1,
        driver.swapchain().format(),
        1, 1,
        ImageUsage::COLOUR_ATTACHMENT | ImageUsage::TRANSFER_SRC
    });
    Image normalBuffer(driver, {
        800, 600, 1,
        Format::RGBA32_SFLOAT,
        1, 1,
        ImageUsage::COLOUR_ATTACHMENT
    });
    Image depthBuffer(driver, {
            800, 600, 1,
            Format::D32_SFLOAT,
            1, 1,
            ImageUsage::DEPTH_STENCIL_ATTACHMENT
    });
    Image::View framebufferAttachments[3] = {
            colourBuffer.getView(),
            normalBuffer.getView(),
            depthBuffer.getView()
    };
    VkImageView fbAttachments[3] = {
            framebufferAttachments[0].view,
            framebufferAttachments[1].view,
            framebufferAttachments[2].view
    };
    Framebuffer framebuffer(driver.context().device(), renderPass, fbAttachments, 800, 600);

    auto systemTime = ende::time::SystemTime::now();

    f32 dt = 1.f / 60.f;
    bool running = true;
    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
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

            ImGui::Begin("Light Data");
            ImGui::Text("ToCube: %f", times[0] / 1e6);
            ImGui::Text("Irradiance: %f", times[1] / 1e6);
            ImGui::Text("Prefilter: %f", times[2] / 1e6);
            ImGui::Text("BRDF: %f", times[3] / 1e6);
            ImGui::Text("FPS: %f", driver.fps());
            ImGui::Text("Milliseconds: %f", driver.milliseconds());

            ende::math::Vec3f colour = scene._lights.front().getColour();
            f32 intensity = scene._lights.front().getIntensity();
            if (ImGui::ColorEdit3("Colour", &colour[0]) ||
                ImGui::SliderFloat("Intensity", &intensity, 1, 10000)) {
                scene._lights.front().setColour(colour);
                scene._lights.front().setIntensity(intensity);
            }

            ImGui::End();

            ImGui::Render();
        }

        {
            f32 factor = (std::sin(systemTime.elapsed().milliseconds() / 1000.f) + 1) / 2.f;
            auto lightPos = lerpPositions(lightPositions, factor);
            scene._lights.front().setPosition(lightPos);

            auto cameraData = camera.data();
            cameraBuffer.data({&cameraData, sizeof(cameraData)});

            scene.prepare(0, camera);

        }


        Driver::FrameInfo frameInfo = driver.beginFrame();
        driver.waitFrame(frameInfo.frame);
        auto frame = driver.swapchain().nextImage();
        {
            frameInfo.cmd->begin();
//            frameInfo.cmd->begin(frame.framebuffer);
            frameInfo.cmd->begin(framebuffer);

            frameInfo.cmd->clearDescriptors();

            frameInfo.cmd->bindProgram(skybox);
            frameInfo.cmd->bindRasterState({ CullMode::NONE });
            frameInfo.cmd->bindDepthState({ true, false, CompareOp::LESS });
            frameInfo.cmd->bindBuffer(0, 0, cameraBuffer);
            frameInfo.cmd->bindImage(2, 0, environmentView, sampler);

            frameInfo.cmd->bindBindings({ &cube._binding, 1 });
            frameInfo.cmd->bindAttributes(cube._attributes);
            frameInfo.cmd->bindPipeline();
            frameInfo.cmd->bindDescriptors();
            frameInfo.cmd->bindVertexBuffer(0, cube._vertex.buffer());
            if (cube._index)
                frameInfo.cmd->bindIndexBuffer(*cube._index);

            if (cube._index)
                frameInfo.cmd->draw(cube._index->size() / sizeof(u32), 1, 0, 0);
            else
                frameInfo.cmd->draw(cube._vertex.size() / (4 * 14), 1, 0, 0);

            frameInfo.cmd->clearDescriptors();

            frameInfo.cmd->bindBuffer(0, 0, cameraBuffer);
            frameInfo.cmd->bindImage(2, 5, irradianceView, sampler);
            frameInfo.cmd->bindImage(2, 6, prefilterView, prefilterSampler);
            frameInfo.cmd->bindImage(2, 7, brdfView, sampler);
            scene.render(*frameInfo.cmd);




            frameInfo.cmd->end(framebuffer);
//            frameInfo.cmd->end(frame.framebuffer);

            driver.swapchain().copyImageToFrame(frame.index, *frameInfo.cmd, colourBuffer);

            frameInfo.cmd->begin(*frame.framebuffer);
            imGuiContext.render(*frameInfo.cmd);
            frameInfo.cmd->end(*frame.framebuffer);

            frameInfo.cmd->end();
            frameInfo.cmd->submit({&frame.imageAquired, 1}, frameInfo.fence);
        }
        auto frameTime = driver.endFrame();
        dt = static_cast<f32>(driver.milliseconds()) / 1000.f;

        driver.swapchain().present(frame, frameInfo.cmd->signal());
    }

    driver.wait();

}