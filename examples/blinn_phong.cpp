#include <Cala/backend/vulkan/SDLPlatform.h>
#include <Cala/backend/vulkan/Driver.h>
#include <Cala/shapes.h>
#include <Cala/Camera.h>
#include <Cala/Light.h>
#include <Ende/filesystem/File.h>
#include <Cala/ImGuiContext.h>
#include <Ende/math/random.h>
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
    ShaderProgram program = loadShader(driver, "../../res/shaders/default.vert.spv"_path, "../../res/shaders/blinn_phong.frag.spv"_path);

    //Vertex Input
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = 14 * sizeof(f32);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    Attribute attributes[5] = {
            {0, 0, AttribType::Vec3f},
            {1, 0, AttribType::Vec3f},
            {2, 0, AttribType::Vec2f},
            {3, 0, AttribType::Vec3f},
            {4, 0, AttribType::Vec3f}
    };

    //Mesh data
    Mesh vertices = cala::shapes::cube();
    Buffer vertexBuffer = vertices.vertexBuffer(driver);
//    Buffer indexBuffer = vertices.indexBuffer(driver);

    Transform modelTransform({0, 0, 0});

    ende::Vector<ende::math::Mat4f> models;
    for (u32 i = 0; i < 100; i++) {
        models.push(Transform({
            ende::math::rand(-10.f, 10.f), ende::math::rand(-10.f, 10.f), ende::math::rand(-10.f, 10.f)
        }).toMat());
    }
    Buffer modelBuffer(driver, models.size() * sizeof(ende::math::Mat4f), BufferUsage::UNIFORM);
    modelBuffer.data({models.data(), static_cast<u32>(models.size() * sizeof(ende::math::Mat4f))});

    Transform cameraTransform({0, 0, -10});
    Camera camera(ende::math::perspective((f32)ende::math::rad(54.4), 800.f / -600.f, 0.1f, 1000.f), cameraTransform);
    Buffer cameraBuffer(driver, sizeof(Camera::Data), BufferUsage::UNIFORM);


    ende::Vector<ende::math::Vec3f> lightPositions = {
            {0, 0, 10},
            {10, 0, 0},
            {0, 0, -10},
            {-10, 0, 0},
            {0, 0, 10}
    };

    PointLight light;
    light.position = {-3, 3, -1};
    light.colour = {1, 1, 1};

    Buffer lightBuffer(driver, sizeof(light), BufferUsage::UNIFORM, MemoryProperties::HOST_VISIBLE | MemoryProperties::HOST_COHERENT);
    lightBuffer.data({&light, sizeof(light)});

    Sampler sampler(driver, {});

    Image brickwall = loadImage(driver, "../../res/textures/brickwall.jpg"_path);
    Image brickwall_normal = loadImage(driver, "../../res/textures/brickwall_normal.jpg"_path);
    Image brickwall_specular = loadImage(driver, "../../res/textures/brickwall_specular.jpg"_path);

    auto diffuseView = brickwall.getView();
    auto normalView = brickwall_normal.getView();
    auto specularView = brickwall_specular.getView();

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

            if (ImGui::ColorEdit3("Colour", &light.colour[0]))
                lightBuffer.data({&light, sizeof(light)});
            if (ImGui::SliderFloat("Intensity", &light.intensity, 1, 50))
                lightBuffer.data({&light, sizeof(light)});


            ImGui::End();

            ImGui::Render();
        }

        {
            f32 factor = (std::sin(systemTime.elapsed().milliseconds() / 1000.f) + 1) / 2.f;
            auto lightPos = lerpPositions(lightPositions, factor);
            light.position = lightPos;
            lightBuffer.data({&light, sizeof(light)});

            auto cameraData = camera.data();
            cameraBuffer.data({&cameraData, sizeof(cameraData)});

//            modelTransform.rotate(ende::math::Vec3f{0, 1, 0}, ende::math::rad(45) * dt);
//            modelTransform.setPos(lightPos);
//            auto model = modelTransform.toMat();
//            modelBuffer.data({&model, sizeof(model)});
        }

        driver.swapchain().wait();
        auto frame = driver.swapchain().nextImage();
        CommandBuffer* cmd = driver.beginFrame();
        {
            cmd->begin(frame.framebuffer);

            cmd->bindProgram(program);
            cmd->bindBindings({&binding, 1});
            cmd->bindAttributes(attributes);
            cmd->bindRasterState({});
            cmd->bindDepthState({});
            cmd->bindPipeline();


            cmd->pushDebugLabel("Test Label");
            cmd->bindBuffer(0, 0, cameraBuffer);
            cmd->bindBuffer(3, 0, lightBuffer);

            cmd->bindImage(2, 0, diffuseView, sampler);
            cmd->bindImage(2, 1, normalView, sampler);
            cmd->bindImage(2, 2, specularView, sampler);


            cmd->bindVertexBuffer(0, vertexBuffer.buffer());

            for (u32 i = 0; i < models.size(); i++) {
                cmd->bindBuffer(1, 0, modelBuffer, i * sizeof(ende::math::Mat4f), sizeof(ende::math::Mat4f));
                cmd->bindDescriptors();

                cmd->draw(36, 1, 0, 0);
            }
            cmd->popDebugLabel();

            imGuiContext.render(*cmd);

            cmd->end(frame.framebuffer);
            cmd->submit({&frame.imageAquired, 1}, frame.fence);
        }
        auto frameTime = driver.endFrame();
        dt = static_cast<f32>(frameTime.milliseconds()) / 1000.f;

        driver.swapchain().present(frame, cmd->signal());
    }

    driver.swapchain().wait();
}