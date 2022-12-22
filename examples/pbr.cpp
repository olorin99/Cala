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
    Driver driver(platform);

    ImGuiContext imGuiContext(driver, platform.window());

    ShaderProgram skybox = loadShader(driver, "../../res/shaders/skybox.vert.spv"_path, "../../res/shaders/skybox.frag.spv"_path);

    //Shaders
    ShaderProgram program = loadShader(driver, "../../res/shaders/default.vert.spv"_path, "../../res/shaders/pbr.frag.spv"_path);

    Material material(driver, std::move(program));
    material._rasterState = {
            CullMode::BACK,
            FrontFace::CCW,
            PolygonMode::FILL
    };
    material._depthState = { true, true };

    auto matInstance = material.instance();

    Image brickwall_albedo = loadImage(driver, "../../res/textures/pbr_gold/lightgold_albedo.png"_path);
    Image brickwall_normal = loadImage(driver, "../../res/textures/pbr_gold/lightgold_normal-ogl.png"_path);
    Image brickwall_metallic = loadImage(driver, "../../res/textures/pbr_gold/lightgold_metallic.png"_path);
    Image brickwall_roughness = loadImage(driver, "../../res/textures/pbr_gold/lightgold_roughness.png"_path);

//    Image brickwall_albedo = loadImage(driver, "../../res/textures/pbr_rusted_iron/rustediron2_basecolor.png"_path);
//    Image brickwall_normal = loadImage(driver, "../../res/textures/pbr_rusted_iron/rustediron2_normal.png"_path);
//    Image brickwall_metallic = loadImage(driver, "../../res/textures/pbr_rusted_iron/rustediron2_metallic.png"_path);
//    Image brickwall_roughness = loadImage(driver, "../../res/textures/pbr_rusted_iron/rustediron2_roughness.png"_path);
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
    Camera camera(ende::math::perspective((f32)ende::math::rad(54.4), 800.f / -600.f, 0.1f, 1000.f), cameraTransform);
    Buffer cameraBuffer(driver, sizeof(Camera::Data), BufferUsage::UNIFORM);

    Scene scene(driver, 10);
    Mesh cube = shapes::cube().mesh(driver);

    ende::Vector<cala::Transform> transforms;
    transforms.reserve(100);
    for (int i = 0; i < 10; i++) {
        transforms.push(Transform({
            ende::math::rand(-5.f, 5.f), ende::math::rand(-5.f, 5.f), ende::math::rand(-5.f, 5.f)
        }));
        scene.addRenderable(cube, &matInstance, &transforms.back());
    }


    ende::Vector<ende::math::Vec3f> lightPositions = {
            {0, 0, 10},
            {10, 0, 0},
            {0, 0, -10},
            {-10, 0, 0},
            {0, 0, 10}
    };

    Transform lightTransform({ -3, 3, -1 });
    Light light(Light::POINT, false, lightTransform);

    scene.addLight(light);

    scene.addRenderable(cube, &matInstance, &lightTransform);

    Sampler sampler(driver, {});

    Buffer probeCameraBuffer(driver, sizeof(Camera::Data) * 6, BufferUsage::UNIFORM);

    {
        Transform probeCameraTransform;
        Camera probeCamera(ende::math::perspective((f32)ende::math::rad(90.f), 512.f / 512.f, 0.1f, 100.f), probeCameraTransform);

        Camera::Data camData[6];

        for (u32 face = 0; face < 6; ++face){
            switch (face) {
                case 0:
                    probeCameraTransform.rotate({0, 1, 0}, ende::math::rad(90));
                    break;
                case 1:
                    probeCameraTransform.rotate({0, 1, 0}, ende::math::rad(180));
                    break;
                case 2:
                    probeCameraTransform.rotate({0, 1, 0}, ende::math::rad(90));
                    probeCameraTransform.rotate({1, 0, 0}, ende::math::rad(90));
                    break;
                case 3:
                    probeCameraTransform.rotate({1, 0, 0}, ende::math::rad(180));
                    break;
                case 4:
                    probeCameraTransform.rotate({1, 0, 0}, ende::math::rad(90));
                    break;
                case 5:
                    probeCameraTransform.rotate({0, 1, 0}, ende::math::rad(180));
                    break;
            }
            camData[face] = probeCamera.data();
        }
        probeCameraBuffer.data({ &camData[0], sizeof(camData)});
    }


    Image environmentMap(driver, {
            512,
            512,
            1,
            Format::RGBA32_SFLOAT,
            4,
            6,
            ImageUsage::STORAGE | ImageUsage::SAMPLED | backend::ImageUsage::TRANSFER_DST | backend::ImageUsage::TRANSFER_SRC
    });
    auto environmentView = environmentMap.getView(VK_IMAGE_VIEW_TYPE_CUBE);

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
            4,
            6,
            ImageUsage::STORAGE | ImageUsage::SAMPLED | backend::ImageUsage::TRANSFER_DST
    });
    Image::View prefilterViews[4] = {
            prefilterMap.getView(VK_IMAGE_VIEW_TYPE_CUBE, 0),
            prefilterMap.getView(VK_IMAGE_VIEW_TYPE_CUBE, 1),
            prefilterMap.getView(VK_IMAGE_VIEW_TYPE_CUBE, 2),
            prefilterMap.getView(VK_IMAGE_VIEW_TYPE_CUBE, 3)
    };

    Buffer roughnessBuf(driver, sizeof(f32) * 4, BufferUsage::UNIFORM);
    f32 roughnessData[] = { 0 / 4.f, 1 / 4.f, 2 / 4.f, 3 / 4.f };
    roughnessBuf.data({ roughnessData, sizeof(f32) * 4 });
    driver.immediate([&](CommandBuffer& cmd) {
        prefilterTimer.start(cmd);
        auto prefilterBarrier = prefilterMap.barrier(backend::Access::NONE, backend::Access::SHADER_WRITE, backend::ImageLayout::UNDEFINED, backend::ImageLayout::GENERAL);
        cmd.pipelineBarrier(backend::PipelineStage::TOP, backend::PipelineStage::COMPUTE_SHADER, 0, nullptr, { &prefilterBarrier, 1 });

        for (u32 mip = 0; mip < 4; mip++) {

            cmd.bindProgram(prefilterCompute);
            cmd.bindImage(1, 0, environmentView, sampler);
            cmd.bindImage(1, 1, prefilterViews[mip], sampler, true);
            cmd.bindBuffer(2, 2, roughnessBuf, sizeof(f32) * mip, sizeof(f32));

            cmd.bindPipeline();
            cmd.bindDescriptors();

            cmd.dispatchCompute(512 / 32, 512 / 32, 6);

        }

        prefilterBarrier = prefilterMap.barrier(backend::Access::SHADER_WRITE, backend::Access::NONE, backend::ImageLayout::UNDEFINED, backend::ImageLayout::SHADER_READ_ONLY);
        cmd.pipelineBarrier(backend::PipelineStage::COMPUTE_SHADER, backend::PipelineStage::BOTTOM, 0, nullptr, { &prefilterBarrier, 1 });
        prefilterTimer.stop();
    });

    auto prefilterView = prefilterMap.getView(VK_IMAGE_VIEW_TYPE_CUBE, 0, 4);


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
                cameraTransform.addPos(cameraTransform.rot().invertY().left() * -dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_D])
                cameraTransform.addPos(cameraTransform.rot().invertY().right() * -dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_LSHIFT])
                cameraTransform.addPos(cameraTransform.rot().invertY().down() * dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_SPACE])
                cameraTransform.addPos(cameraTransform.rot().invertY().up() * dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_LEFT])
                cameraTransform.rotate({0, 1, 0}, ende::math::rad(45) * dt);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_RIGHT])
                cameraTransform.rotate({0, 1, 0}, ende::math::rad(-45) * dt);
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

            scene.prepare();

        }


        Driver::FrameInfo frameInfo = driver.beginFrame();
        driver.waitFrame(frameInfo.frame);
        auto frame = driver.swapchain().nextImage();
        {
            frameInfo.cmd->begin();
            frameInfo.cmd->begin(frame.framebuffer);

            frameInfo.cmd->clearDescriptors();

            frameInfo.cmd->bindProgram(skybox);
            frameInfo.cmd->bindRasterState({ CullMode::NONE });
            frameInfo.cmd->bindDepthState({ true, false, CompareOp::LESS });
            frameInfo.cmd->bindBuffer(0, 0, cameraBuffer);
            frameInfo.cmd->bindImage(2, 0, environmentView, sampler);
//            frameInfo.cmd->bindImage(2, 0, irradianceView, sampler);
//            frameInfo.cmd->bindImage(2, 0, prefilterView, sampler);
//            frameInfo.cmd->bindImage(2, 0, prefilterViews[3], sampler);

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
            frameInfo.cmd->bindImage(2, 6, prefilterView, sampler);
            frameInfo.cmd->bindImage(2, 7, brdfView, sampler);
            scene.render(*frameInfo.cmd);



            imGuiContext.render(*frameInfo.cmd);

            frameInfo.cmd->end(frame.framebuffer);
            frameInfo.cmd->end();
            frameInfo.cmd->submit({&frame.imageAquired, 1}, frameInfo.fence);
        }
        auto frameTime = driver.endFrame();
        dt = static_cast<f32>(driver.milliseconds()) / 1000.f;

        driver.swapchain().present(frame, frameInfo.cmd->signal());
    }

    driver.wait();
}