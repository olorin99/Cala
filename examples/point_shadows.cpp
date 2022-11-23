#include <Cala/backend/vulkan/SDLPlatform.h>
#include <Cala/backend/vulkan/Driver.h>
#include <Cala/shapes.h>
#include <Cala/Camera.h>
#include <Cala/Light.h>
#include <Ende/filesystem/File.h>
#include <Cala/ImGuiContext.h>
#include <Ende/math/random.h>
#include <Cala/Material.h>
#include <Cala/MaterialInstance.h>
#include <Cala/Scene.h>
#include <Ende/math/Frustum.h>
#include <Cala/backend/vulkan/Timer.h>
#include "../../third_party/stb_image.h"

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

ShaderProgram loadShader(Driver& driver, const ende::fs::Path& vertex) {
    ende::fs::File shaderFile;
    shaderFile.open(vertex, ende::fs::in | ende::fs::binary);

    ende::Vector<u32> vertexData(shaderFile.size() / sizeof(u32));
    shaderFile.read({reinterpret_cast<char*>(vertexData.data()), static_cast<u32>(vertexData.size() * sizeof(u32))});

    return ShaderProgram::create()
            .addStage(vertexData, ShaderStage::VERTEX)
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

ende::math::Vec3f lerpPositions(ende::Span<ende::math::Vec3f> inputs, f32 factor) {
    ende::Vector<ende::math::Vec3f> positions(inputs.size(), inputs.begin());
    ende::Vector<ende::math::Vec3f> tmpPositions;

    while (positions.size() > 1) {
        for (u32 i = 0; i < positions.size(); i++) {
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

    //Shaders
    ShaderProgram program = loadShader(driver, "../../res/shaders/direct_shadow.vert.spv"_path, "../../res/shaders/point_shadow.frag.spv"_path);

    Material material(driver, std::move(program));
    material._rasterState = {
            CullMode::BACK,
            FrontFace::CCW,
            PolygonMode::FILL
    };
    material._depthState = { true, true, cala::backend::CompareOp::LESS_EQUAL };

    auto matInstance = material.instance();

    Image brickwall = loadImage(driver, "../../res/textures/brickwall.jpg"_path);
    Image brickwall_normal = loadImage(driver, "../../res/textures/brickwall_normal.jpg"_path);
    Image brickwall_specular = loadImage(driver, "../../res/textures/brickwall_specular.jpg"_path);
    matInstance.setSampler("diffuseMap", brickwall.getView(), Sampler(driver, {}));
    matInstance.setSampler("normalMap", brickwall_normal.getView(), Sampler(driver, {}));
    matInstance.setSampler("specularMap", brickwall_specular.getView(), Sampler(driver, {}));


    ShaderProgram shadowProgram = loadShader(driver, "../../res/shaders/shadow_point.vert.spv"_path, "../../res/shaders/shadow_point.frag.spv"_path);
//    ShaderProgram shadowProgram = loadShader(driver, "../../res/shaders/shadow.vert.spv"_path);

    Material shadowMaterial(driver, std::move(shadowProgram));
    shadowMaterial._rasterState = {
            .cullMode = CullMode::FRONT,
            .frontFace = FrontFace::CCW,
            .polygonMode= PolygonMode::FILL,
            //.depthBias = true
    };
    shadowMaterial._depthState = { true, true, cala::backend::CompareOp::LESS_EQUAL };

    auto shadowMatInstance = shadowMaterial.instance();


    Transform cameraTransform({0, 0, 0});
    Camera camera(ende::math::perspective((f32)ende::math::rad(90), 800.f / -600.f, 0.1f, 1000.f), cameraTransform);
    Buffer cameraBuffer(driver, sizeof(Camera::Data), BufferUsage::UNIFORM);

    Scene scene(driver, 10);
    Mesh cube = shapes::cube().mesh(driver);

    ende::Vector<cala::Transform> transforms;
    transforms.reserve(100);

    f32 volume = 20;
    u32 count = 20;

    for (int i = 0; i < count; i++) {
        transforms.push(Transform({
            ende::math::rand(-volume, volume), ende::math::rand(-volume, volume), ende::math::rand(-volume, volume)
        }));
        scene.addRenderable(cube, &matInstance, &transforms.back());
    }
//    Transform centrePos({0, 0, 0}, {0, 0, 0, 1}, {0.2, 0.2, 0.2});
//    scene.addRenderable(cube, &matInstance, &centrePos);
//    Transform lightPos({1, -1, 1}, {0, 0, 0, 1}, {0.5, 0.5, 0.5});
//    scene.addRenderable(cube, &matInstance, &lightPos);

    f32 width = volume * 2;
    Transform floorPos({0, -width, 0}, {0, 0, 0, 1}, {width, 1, width});
    scene.addRenderable(cube, &matInstance, &floorPos);
    Transform roofPos({0, width, 0}, {0, 0, 0, 1}, {width, 1, width});
    scene.addRenderable(cube, &matInstance, &roofPos);
    Transform leftPos({-width, 0, 0}, {0, 0, 0, 1}, {1, width, width});
    scene.addRenderable(cube, &matInstance, &leftPos);
    Transform rightPos({width, 0, 0}, {0, 0, 0, 1}, {1, width, width});
    scene.addRenderable(cube, &matInstance, &rightPos);
    Transform frontPos({0, 0, -width}, {0, 0, 0, 1}, {width, width, 1});
    scene.addRenderable(cube, &matInstance, &frontPos);
    Transform backPos({0, 0, width}, {0, 0, 0, 1}, {width, width, 1});
    scene.addRenderable(cube, &matInstance, &backPos);


    f32 lightVolume = 1;
    ende::Vector<ende::math::Vec3f> lightPositions = {
            {0, 0, lightVolume},
            {lightVolume, 0, 0},
            {0, 0, -lightVolume},
            {-lightVolume, 0, 0},
            {0, 0, lightVolume}
    };


    Transform lightTransform({0, 0, 0});

    scene.addRenderable(cube, &matInstance, &lightTransform);
    Light light(Light::POINT, true, lightTransform);

    Buffer lightBuffer(driver, sizeof(light), BufferUsage::UNIFORM, MemoryProperties::HOST_VISIBLE | MemoryProperties::HOST_COHERENT);
    auto lightData = light.data();
    lightBuffer.data({&lightData, sizeof(lightData)});

    scene.addLight(light);

//    Camera lightCamera(ende::math::orthographic<f32>(-10, 10, -10, 10, 1, 100), lightTransform);
    Camera lightCamera((f32)ende::math::rad(90.f), 1024.f, 1024.f, 0.1f, 100.f, lightTransform);
    Buffer lightCamBuf(driver, sizeof(Camera::Data), BufferUsage::UNIFORM);
    auto lightCameraData = lightCamera.data();
    lightCamBuf.data({ &lightCameraData, sizeof(lightCameraData) });

    RenderPass::Attachment shadowPassAttachment {
            Format::D32_SFLOAT,
            VK_SAMPLE_COUNT_1_BIT,
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    };
    RenderPass shadowPass(driver, {&shadowPassAttachment, 1});
    Image depthMap(driver, {1024, 1024, 1, Format::D32_SFLOAT, 1, 1, ImageUsage::SAMPLED | ImageUsage::DEPTH_STENCIL_ATTACHMENT | ImageUsage::TRANSFER_SRC});
    Image::View depthMapView = depthMap.getView();
    VkImageView shadowAttachment = depthMapView.view;
    Framebuffer shadowFramebuffer(driver.context().device(), shadowPass, {&shadowAttachment, 1}, 1024, 1024);

    Image shadowMap(driver, {1024, 1024, 1, Format::D32_SFLOAT, 1, 6, ImageUsage::SAMPLED | ImageUsage::TRANSFER_DST});
    auto shadowView = shadowMap.getView(VK_IMAGE_VIEW_TYPE_CUBE, 0, 1, 0, 1);

    Sampler sampler(driver, {
            .filter = VK_FILTER_NEAREST,
            .addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
            .maxLod = 1.0,
            .borderColour = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE
    });

    auto systemTime = ende::time::SystemTime::now();

    Timer shadowPassTimer(driver);
    Timer lightPassTimer(driver, 1);

    u64 shadowPassTime = 0;
    u64 lightPassTime = 0;

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
            ImGui::Text("FPS: %f", driver.fps());
            ImGui::Text("Milliseconds: %f", driver.milliseconds());
            ImGui::Text("Shadow Milliseconds: %f", shadowPassTime / 1e6);
            ImGui::Text("Light Milliseconds: %f", lightPassTime / 1e6);

            ende::math::Vec3f colour = scene._lights.front().getColour();
            f32 intensity = scene._lights.front().getIntensity();
            if (ImGui::ColorEdit3("Colour", &colour[0]) ||
                ImGui::SliderFloat("Intensity", &intensity, 1, 50)) {
                scene._lights.front().setColour(colour);
                scene._lights.front().setIntensity(intensity);
                lightData = scene._lights.front().data();
                lightBuffer.data({&lightData, sizeof(lightData)});
            }

            ImGui::End();

            ImGui::Render();
        }

        {
            f32 factor = (std::sin(systemTime.elapsed().milliseconds() / 1000.f) + 1) / 2.f;
            auto lPos = lerpPositions(lightPositions, factor);
            lightTransform.setPos(lPos);
//            lightTransform.rotate({1, 0, 0}, std::sin(systemTime.elapsed().milliseconds() / 1000.f) * dt);
//            auto lightPos = lightTransform.rot().front() * 20;
            lightData = scene._lights.front().data();
            lightBuffer.data({&lightData, sizeof(lightData)});

            lightCameraData = lightCamera.data();
            lightCamBuf.data({ &lightCameraData, sizeof(lightCameraData) });

            auto cameraData = camera.data();
            cameraBuffer.data({&cameraData, sizeof(cameraData)});

            scene.prepare();
        }

        driver.swapchain().wait();
        auto frame = driver.swapchain().nextImage();
        CommandBuffer* cmd = driver.beginFrame();
        {
            shadowPassTimer.start(*cmd);

            lightTransform.setRot({0, 0, 0, 1});
            for (u32 j = 0; j < 6; j++) {
                cmd->begin(shadowFramebuffer);
                cmd->clearDescriptors();
//                cmd->bindBuffer(0, 0, lightCamBuf);
                cmd->bindBuffer(3, 0, lightBuffer);

                shadowMatInstance.bind(*cmd);
                {
                    switch (j) {
                        case 0:
                            lightTransform.rotate({0, 1, 0}, ende::math::rad(90));
                            break;
                        case 1:
                            lightTransform.rotate({0, 1, 0}, ende::math::rad(180));
                            break;
                        case 2:
                            lightTransform.rotate({0, 1, 0}, ende::math::rad(90));
                            lightTransform.rotate({1, 0, 0}, ende::math::rad(90));
                            break;
                        case 3:
                            lightTransform.rotate({1, 0, 0}, ende::math::rad(180));
                            break;
                        case 4:
                            lightTransform.rotate({1, 0, 0}, ende::math::rad(90));
                            break;
                        case 5:
                            lightTransform.rotate({0, 1, 0}, ende::math::rad(180));
                            break;
                    }


                    lightCameraData = lightCamera.data();
                    cmd->pushConstants({&lightCameraData, sizeof(lightCameraData)});
                }

                lightCamera.updateFrustum();


                for (u32 i = 0; i < scene._renderables.size(); i++) {
                    auto& renderable = scene._renderables[i].second.first;
                    auto& transform = scene._renderables[i].second.second;

                    if (!lightCamera.frustum().intersect(transform->pos(), 2)) //if radius is too small clips edges
                        continue;

                    cmd->bindBindings(renderable.bindings);
                    cmd->bindAttributes(renderable.attributes);

                    cmd->bindBuffer(1, 0, scene._modelBuffer, i * sizeof(ende::math::Mat4f), sizeof(ende::math::Mat4f));

                    cmd->bindPipeline();
                    cmd->bindDescriptors();

                    cmd->bindVertexBuffer(0, renderable.vertex.buffer().buffer());
                    if (renderable.index) {
                        cmd->bindIndexBuffer(renderable.index.buffer());
                        cmd->draw(renderable.index.size() / sizeof(u32), 1, 0, 0);
                    } else
                        cmd->draw(renderable.vertex.size() / (4 * 14), 1, 0, 0);

                }

                cmd->end(shadowFramebuffer);


                VkImageMemoryBarrier barriers[2];
                barriers[0] = depthMap.barrier(Access::DEPTH_STENCIL_WRITE, Access::TRANSFER_READ, ImageLayout::DEPTH_STENCIL_ATTACHMENT, ImageLayout::TRANSFER_SRC);
                barriers[1] = shadowMap.barrier(Access::SHADER_READ, Access::TRANSFER_WRITE, ImageLayout::UNDEFINED, ImageLayout::TRANSFER_DST);

                VkImageSubresourceRange cubeFaceRange{};
                cubeFaceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                cubeFaceRange.baseMipLevel = 0;
                cubeFaceRange.levelCount = 1;
                cubeFaceRange.baseArrayLayer = j;
                cubeFaceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

                barriers[1].subresourceRange = cubeFaceRange;
                cubeFaceRange.baseArrayLayer = 0;
                barriers[0].subresourceRange = cubeFaceRange;

                cmd->pipelineBarrier(PipelineStage::LATE_FRAGMENT, PipelineStage::TRANSFER, 0, nullptr, {&barriers[0], 1});
                cmd->pipelineBarrier(PipelineStage::FRAGMENT_SHADER, PipelineStage::TRANSFER, 0, nullptr, {&barriers[1], 1});

                VkImageCopy copyRegion{};
                copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                copyRegion.srcSubresource.baseArrayLayer = 0;
                copyRegion.srcSubresource.mipLevel = 0;
                copyRegion.srcSubresource.layerCount = 1;
                copyRegion.srcOffset = {0, 0};

                copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                copyRegion.dstSubresource.baseArrayLayer = j;
                copyRegion.dstSubresource.mipLevel = 0;
                copyRegion.dstSubresource.layerCount = 1;
                copyRegion.dstOffset = {0, 0, 0};

                copyRegion.extent.width = shadowMap.width();
                copyRegion.extent.height = shadowMap.height();
                copyRegion.extent.depth = shadowMap.depth();

                vkCmdCopyImage(cmd->buffer(), depthMap.image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, shadowMap.image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

                barriers[0] = depthMap.barrier(Access::TRANSFER_READ, Access::DEPTH_STENCIL_WRITE, ImageLayout::TRANSFER_SRC, ImageLayout::DEPTH_STENCIL_ATTACHMENT);
                barriers[1] = shadowMap.barrier(Access::TRANSFER_WRITE, Access::SHADER_READ, ImageLayout::TRANSFER_DST, ImageLayout::SHADER_READ_ONLY);

                cubeFaceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                cubeFaceRange.baseMipLevel = 0;
                cubeFaceRange.levelCount = 1;
                cubeFaceRange.baseArrayLayer = j;
                cubeFaceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

                barriers[1].subresourceRange = cubeFaceRange;

                cmd->pipelineBarrier(PipelineStage::TRANSFER, PipelineStage::EARLY_FRAGMENT, 0, nullptr, {&barriers[0], 1});
                cmd->pipelineBarrier(PipelineStage::TRANSFER, PipelineStage::FRAGMENT_SHADER, 0, nullptr, {&barriers[1], 1});
            }
            shadowPassTimer.stop();


            cmd->clearDescriptors();

            lightPassTimer.start(*cmd);
            cmd->begin(frame.framebuffer);

            cmd->bindBuffer(0, 0, cameraBuffer);
//            cmd->bindBuffer(3, 0, lightBuffer);
            cmd->bindBuffer(0, 1, lightCamBuf);
            cmd->bindImage(2, 3, shadowView, sampler);
            scene.render(*cmd);


            imGuiContext.render(*cmd);

            cmd->end(frame.framebuffer);
            lightPassTimer.stop();
            cmd->submit({&frame.imageAquired, 1}, frame.fence);
        }
        driver.endFrame();
        dt = driver.milliseconds() / (f64)1000;

        driver.swapchain().present(frame, cmd->signal());

        shadowPassTime = shadowPassTimer.result() ? shadowPassTimer.result() : shadowPassTime;
        lightPassTime = lightPassTimer.result() ? lightPassTimer.result() : lightPassTime;
    }

    driver.swapchain().wait();
}