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
    SDLPlatform platform("hello_triangle", 800, 600);
    Driver driver(platform);

    ImGuiContext imGuiContext(driver, platform.window());

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

//    Image brickwall = loadImage(driver, "../../res/textures/brickwall.jpg"_path);
//    Image brickwall_normal = loadImage(driver, "../../res/textures/brickwall_normal.jpg"_path);
//    Image brickwall_specular = loadImage(driver, "../../res/textures/brickwall_specular.jpg"_path);
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
            imGuiContext.newFrame();

            ImGui::Begin("Light Data");
            ImGui::Text("FPS: %f", driver.fps());
            ImGui::Text("Milliseconds: %f", driver.milliseconds());

            ende::math::Vec3f colour = scene._lights.front().getColour();
            f32 intensity = scene._lights.front().getIntensity();
            if (ImGui::ColorEdit3("Colour", &colour[0]) ||
                ImGui::SliderFloat("Intensity", &intensity, 1, 5000)) {
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

            frameInfo.cmd->bindBuffer(0, 0, cameraBuffer);
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