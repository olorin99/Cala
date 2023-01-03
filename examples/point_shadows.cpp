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

#include <Cala/Probe.h>

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

    Engine engine(platform);
    auto& driver = engine.driver();

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


//    Buffer shadowCameraBuffer(driver, sizeof(Camera::Data) * 6, BufferUsage::UNIFORM);
//    {
//        Transform shadowCameraTransform;
//        Camera shadowCamera(ende::math::perspective((f32)ende::math::rad(90.f), 512.f / 512.f, 0.1f, 100.f), shadowCameraTransform);
//
//        Camera::Data camData[6];
//
//        for (u32 face = 0; face < 6; ++face){
//            switch (face) {
//                case 0:
//                    shadowCameraTransform.rotate({0, 1, 0}, ende::math::rad(90));
//                    break;
//                case 1:
//                    shadowCameraTransform.rotate({0, 1, 0}, ende::math::rad(180));
//                    break;
//                case 2:
//                    shadowCameraTransform.rotate({0, 1, 0}, ende::math::rad(90));
//                    shadowCameraTransform.rotate({1, 0, 0}, ende::math::rad(90));
//                    break;
//                case 3:
//                    shadowCameraTransform.rotate({1, 0, 0}, ende::math::rad(180));
//                    break;
//                case 4:
//                    shadowCameraTransform.rotate({1, 0, 0}, ende::math::rad(90));
//                    break;
//                case 5:
//                    shadowCameraTransform.rotate({0, 1, 0}, ende::math::rad(180));
//                    break;
//            }
//            camData[face] = shadowCamera.data();
//        }
//        shadowCameraBuffer.data({ &camData[0], sizeof(camData)});
//    }
    Probe shadowProbe(&engine, { 1024, 1024, Format::D32_SFLOAT, ImageUsage::SAMPLED | ImageUsage::DEPTH_STENCIL_ATTACHMENT, &shadowPass });
//    auto shadowView = shadowProbe.map().getView(VK_IMAGE_VIEW_TYPE_CUBE, 0, 1, 0, 1);
    auto& shadowView = shadowProbe.view();

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
            lightData = scene._lights.front().data();
            lightBuffer.data({&lightData, sizeof(lightData)});

            lightCameraData = lightCamera.data();
            lightCamBuf.data({ &lightCameraData, sizeof(lightCameraData) });

            auto cameraData = camera.data();
            cameraBuffer.data({&cameraData, sizeof(cameraData)});

            scene.prepare(0, camera);
        }

        Driver::FrameInfo frameInfo = driver.beginFrame();
        driver.waitFrame(frameInfo.frame);
        auto frame = driver.swapchain().nextImage();
        {
            frameInfo.cmd->begin();
            shadowPassTimer.start(*frameInfo.cmd);

            lightTransform.setRot({0, 0, 0, 1});

            shadowProbe.draw(*frameInfo.cmd, [&](CommandBuffer& cmd, u32 face) {
                cmd.clearDescriptors();
                cmd.bindBuffer(3, 0, lightBuffer);
                shadowMatInstance.bind(cmd);
                switch (face) {
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

                    auto lightCamData = lightCamera.data();
                    cmd.pushConstants({ &lightCamData, sizeof(lightCamData) });

                    lightCamera.updateFrustum();

                    for (u32 i = 0; i < scene._renderList.size(); i++) {
                        auto& renderable = scene._renderList[i].second.first;
                        auto& transform = scene._renderList[i].second.second;

                        if (!lightCamera.frustum().intersect(transform->pos(), 2)) //if radius is too small clips edges
                            continue;

                        frameInfo.cmd->bindBindings(renderable.bindings);
                        frameInfo.cmd->bindAttributes(renderable.attributes);

                        frameInfo.cmd->bindBuffer(1, 0, scene._modelBuffer[0], i * sizeof(ende::math::Mat4f), sizeof(ende::math::Mat4f));

                        frameInfo.cmd->bindPipeline();
                        frameInfo.cmd->bindDescriptors();

                        frameInfo.cmd->bindVertexBuffer(0, renderable.vertex.buffer().buffer());
                        if (renderable.index) {
                            frameInfo.cmd->bindIndexBuffer(renderable.index.buffer());
                            frameInfo.cmd->draw(renderable.index.size() / sizeof(u32), 1, 0, 0);
                        } else
                            frameInfo.cmd->draw(renderable.vertex.size() / (4 * 14), 1, 0, 0);
                    }
            });

            shadowPassTimer.stop();


            frameInfo.cmd->clearDescriptors();

            lightPassTimer.start(*frameInfo.cmd);
            frameInfo.cmd->begin(*frame.framebuffer);

            frameInfo.cmd->bindBuffer(0, 0, cameraBuffer);
            frameInfo.cmd->bindBuffer(0, 1, lightCamBuf);
            frameInfo.cmd->bindImage(2, 3, shadowView, sampler);
            scene.render(*frameInfo.cmd);


            imGuiContext.render(*frameInfo.cmd);

            frameInfo.cmd->end(*frame.framebuffer);
            lightPassTimer.stop();
            frameInfo.cmd->submit({&frame.imageAquired, 1}, frameInfo.fence);
        }
        driver.endFrame();
        dt = driver.milliseconds() / (f64)1000;

        driver.swapchain().present(frame, frameInfo.cmd->signal());

        shadowPassTime = shadowPassTimer.result() ? shadowPassTimer.result() : shadowPassTime;
        lightPassTime = lightPassTimer.result() ? lightPassTimer.result() : lightPassTime;
    }

    driver.wait();
}