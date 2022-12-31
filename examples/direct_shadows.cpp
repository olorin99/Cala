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

    Transform testTransform({0, 0, 0}, ende::math::Quaternion({1, 0, 0}, ende::math::rad(90)));
    testTransform.rotate(ende::math::Quaternion({0, 0, 1}, ende::math::rad(45)));

    auto testDir = testTransform.rot().front();

    auto testDir1 = testTransform.rot().rotate({0, 0, 1});

    auto testMat = testTransform.rot().toMat();

    auto testDir2 = testMat.transform(ende::math::Vec<3, f32>{0.f, 0.f, 1.f});








    SDLPlatform platform("hello_triangle", 800, 600);
    Driver driver(platform);

    ImGuiContext imGuiContext(driver, platform.window());

    //Shaders
    ShaderProgram program = loadShader(driver, "../../res/shaders/direct_shadow.vert.spv"_path, "../../res/shaders/direct_shadow.frag.spv"_path);

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


    ShaderProgram shadowProgram = loadShader(driver, "../../res/shaders/shadow.vert.spv"_path);

    Material shadowMaterial(driver, std::move(shadowProgram));
    shadowMaterial._rasterState = {
            .cullMode = CullMode::FRONT,
            .frontFace = FrontFace::CCW,
            .polygonMode= PolygonMode::FILL,
            //.depthBias = true
    };
    shadowMaterial._depthState = { true, true, cala::backend::CompareOp::LESS_EQUAL };

    auto shadowMatInstance = shadowMaterial.instance();


    Transform cameraTransform({0, 0, 10});
//    Transform cameraTransform({-2, 20, 0}, ende::math::Quaternion({1, 0, 0}, ende::math::rad(-90)));
    Camera camera(ende::math::perspective((f32)ende::math::rad(54.4), 800.f / -600.f, 0.1f, 1000.f), cameraTransform);
//    Camera camera(ende::math::orthographic<f32>(-10, 10, -10, 10, 1, 100), cameraTransform);
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
    Transform centrePos({0, 0, 0}, {0, 0, 0, 1}, {0.2, 0.2, 0.2});
    scene.addRenderable(cube, &matInstance, &centrePos);
    Transform lightPos({1, -1, 1}, {0, 0, 0, 1}, {0.5, 0.5, 0.5});
    scene.addRenderable(cube, &matInstance, &lightPos);

    Transform floorPos({0, -20, 0}, {0, 0, 0, 1}, {100, 1, 100});
    scene.addRenderable(cube, &matInstance, &floorPos);


//    ende::Vector<ende::math::Vec3f> lightPositions = {
//            {0, 0, 1},
//            {1, 0, 0},
//            {0, 0, -1},
//            {-1, 0, 0},
//            {0, 0, 1}
//    };


    Transform lightTransform({-2, 20, 0}, ende::math::Quaternion({1, 0, 0}, ende::math::rad(-90)));

    scene.addRenderable(cube, &matInstance, &lightTransform);
    Light light(Light::DIRECTIONAL, true, lightTransform);

    Buffer lightBuffer(driver, sizeof(light), BufferUsage::UNIFORM, MemoryProperties::HOST_VISIBLE | MemoryProperties::HOST_COHERENT);
    auto lightData = light.data();
    lightBuffer.data({&lightData, sizeof(lightData)});

//    Camera lightCamera(ende::math::orthographic<f32>(-10, 10, -10, 10, 1, 100), lightTransform);
    Camera lightCamera(ende::math::perspective((f32)ende::math::rad(45.f), 1024.f / 1024.f, 0.1f, 100.f), lightTransform);
    Buffer lightCamBuf(driver, sizeof(Camera::Data), BufferUsage::UNIFORM);
    auto lightCameraData = lightCamera.data();
    lightCamBuf.data({ &lightCameraData, sizeof(lightCameraData) });


    Image shadowMap(driver, { 1024, 1024, 1, Format::D16_UNORM, 1, 1, ImageUsage::SAMPLED | ImageUsage::DEPTH_STENCIL_ATTACHMENT });

    RenderPass::Attachment shadowAttachment {
            Format::D16_UNORM,
        VK_SAMPLE_COUNT_1_BIT,
        VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_STORE,
        VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_DONT_CARE,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    };

    RenderPass shadowPass(driver, { &shadowAttachment, 1});

    Image::View shadowView = shadowMap.getView();
    Framebuffer shadowFramebuffer(driver.context().device(), shadowPass, { &shadowView.view, 1 }, 1024, 1024);


    Sampler sampler(driver, {
        .filter = VK_FILTER_NEAREST,
        .addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        .maxLod = 1.0,
        .borderColour = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE
    });


    auto systemTime = ende::time::SystemTime::now();

    f64 dt = 1.f / 60.f;
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
                cameraTransform.rotate({0, -1, 0}, ende::math::rad(-45) * dt);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_RIGHT])
                cameraTransform.rotate({0, -1, 0}, ende::math::rad(45) * dt);
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

            ende::math::Vec3f colour = light.getColour();
            f32 intensity = light.getIntensity();
            if (ImGui::ColorEdit3("Colour", &colour[0]) ||
                    ImGui::SliderFloat("Intensity", &intensity, 1, 50)) {
                light.setColour(colour);
                light.setIntensity(intensity);
                lightData = light.data();
                lightBuffer.data({&lightData, sizeof(lightData)});
            }

            ImGui::End();

            ImGui::Render();
        }

        {
//            lightTransform.rotate({1, 0, 0}, std::sin(systemTime.elapsed().milliseconds() / 1000.f) * dt);
//            auto lightPos = lightTransform.rot().front() * 20;
//            lightPos = lightPos * -1;
//            lightPos[1] *= -1;
//            lightTransform.setPos(lightPos);
//            cameraTransform.rotate({1, 0, 0}, std::sin(systemTime.elapsed().milliseconds() / 1000.f) * dt);
//            cameraTransform.setPos(cameraTransform.rot().front() * 20);
//            f32 factor = (std::sin(systemTime.elapsed().milliseconds() / 1000.f) + 1) / 2.f;
//            auto lightPos = lerpPositions(lightPositions, factor);
//            light.setDirection({lightPos, 10});
            lightData = light.data();
            lightBuffer.data({&lightData, sizeof(lightData)});

            auto lightCameraData = lightCamera.data();
            lightCamBuf.data({ &lightCameraData, sizeof(lightCameraData) });

            auto cameraData = camera.data();
            cameraBuffer.data({&cameraData, sizeof(cameraData)});

            scene.prepare();

        }

        Driver::FrameInfo frameInfo = driver.beginFrame();
        driver.waitFrame(frameInfo.frame);
        auto frame = driver.swapchain().nextImage();
        {
            frameInfo.cmd->begin();
            //cmd->begin(frame.framebuffer);
            frameInfo.cmd->begin(shadowFramebuffer);
            frameInfo.cmd->clearDescriptors();

            //vkCmdSetDepthBias(cmd->buffer(), 1.25f, 0.f, 1.75f);

            //cmd->bindBuffer(0, 0, cameraBuffer);
//            cmd->bindBuffer(0, 0, lightCamBuf);
//            frameInfo.cmd->bindBuffer(3, 0, lightBuffer);

            shadowMatInstance.bind(*frameInfo.cmd);

            for (u32 i = 0; i < scene._renderables.size(); i++) {
                auto& renderable = scene._renderables[i].second.first;
                auto& transform = scene._renderables[i].second.second;
                frameInfo.cmd->bindBindings(renderable.bindings);
                frameInfo.cmd->bindAttributes(renderable.attributes);

                frameInfo.cmd->bindBuffer(1, 0, scene._modelBuffer, i * sizeof(ende::math::Mat4f), sizeof(ende::math::Mat4f));

                ende::math::Mat4f constants[2] = {
                        lightCamera.projection(),
                        lightCamera.view()
                };
                frameInfo.cmd->pushConstants({constants, sizeof(constants)});

                frameInfo.cmd->bindPipeline();
                frameInfo.cmd->bindDescriptors();

                frameInfo.cmd->bindVertexBuffer(0, renderable.vertex.buffer().buffer());
                if (renderable.index)
                    frameInfo.cmd->bindIndexBuffer(renderable.index.buffer());

                if (renderable.index)
                    frameInfo.cmd->draw(renderable.index.size() / sizeof(u32), 1, 0, 0);
                else
                    frameInfo.cmd->draw(renderable.vertex.size() / (4 * 14), 1, 0, 0);
            }

            frameInfo.cmd->end(shadowFramebuffer);
            //cmd->end(frame.framebuffer);


            frameInfo.cmd->begin(*frame.framebuffer);

            frameInfo.cmd->bindBuffer(0, 0, cameraBuffer);
            frameInfo.cmd->bindBuffer(3, 0, lightBuffer);
            frameInfo.cmd->bindBuffer(0, 1, lightCamBuf);
            frameInfo.cmd->bindImage(2, 3, shadowView, sampler);

            scene.render(*frameInfo.cmd);

            imGuiContext.render(*frameInfo.cmd);

            frameInfo.cmd->end(*frame.framebuffer);
            frameInfo.cmd->submit({&frame.imageAquired, 1}, frameInfo.fence);
        }
        driver.endFrame();
        dt = driver.milliseconds() / (f64)1000;

        driver.swapchain().present(frame, frameInfo.cmd->signal());
    }

    driver.wait();
}