#include <Cala/backend/vulkan/SDLPlatform.h>
#include <Cala/backend/vulkan/Device.h>
#include <Cala/shapes.h>
#include <Cala/Camera.h>
#include <Ende/filesystem/File.h>
#include <Cala/Material.h>

#include "../third_party/stb_image.h"

using namespace cala;
using namespace cala::backend;
using namespace cala::backend::vulkan;

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

Image loadImage(Device& driver, const ende::fs::Path& path) {
    i32 width, height, channels;
    u8* data = stbi_load((*path).c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!data) throw "unable load image";
    u32 length = width * height * 4;

    Image image(driver, {(u32)width, (u32)height, 1, backend::Format::RGBA8_SRGB});
    image.data(driver, {0, (u32)width, (u32)height, 1, 4, {data, length}});
    stbi_image_free(data);
    return image;
}

struct IndirectDrawData {
    u32 vertexCount;
    u32 instanceCount;
    u32 firstVertex;
    u32 firstInstance;
};

int main() {
    SDLPlatform platform("hello_triangle", 800, 600);
    Device driver(platform);

    //Shaders
    ShaderProgram program = loadShader(driver, "../../res/shaders/defaultIndirect.vert.spv"_path, "../../res/shaders/default.frag.spv"_path);

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

    //MeshData data
    MeshData vertices = cala::shapes::cube();
    Buffer vertexBuffer = vertices.vertexBuffer(driver);
//    Buffer indexBuffer = vertices.indexBuffer(driver);

    Transform cameraTransform({0, 0, -10});
    Camera camera(ende::math::perspective((f32)ende::math::rad(54.4), 800.f / -600.f, 0.1f, 1000.f), cameraTransform);
    Buffer cameraBuffer(driver, sizeof(Camera::Data), BufferUsage::UNIFORM, MemoryProperties::HOST_VISIBLE | MemoryProperties::HOST_COHERENT);

    Buffer drawCommands(driver, sizeof(IndirectDrawData) * 10, BufferUsage::INDIRECT | BufferUsage::TRANSFER_DST | BufferUsage::STORAGE);
    {
        ende::Vector<IndirectDrawData> drawData;
        for (u32 i = 0; i < 10; i++) {
            drawData.push({ 36, 1, 0, i });
        }
        drawCommands.data({drawData.data(), static_cast<u32>(drawData.size()* sizeof(IndirectDrawData))});
    }


    Transform modelTransform({0, 0, 0});
    Buffer modelBuffer(driver, 10 * sizeof(ende::math::Mat4f), BufferUsage::STORAGE);
    ende::Vector<ende::math::Mat4f> models;
    for (u32 i = 0; i < 10; i++) {
        models.push(modelTransform.toMat());
        modelTransform.addPos({3, 0, 0});
    }
    modelBuffer.data({models.data(), static_cast<u32>(models.size() * sizeof(ende::math::Mat4f))});


    Image brickwall = loadImage(driver, "../../res/textures/brickwall.jpg"_path);
    Image brickwall_normal = loadImage(driver, "../../res/textures/brickwall_normal.jpg"_path);
    Image brickwall_specular = loadImage(driver, "../../res/textures/brickwall_specular.jpg"_path);

    Material instancedMaterial(driver, std::move(program));
    instancedMaterial._depthState = {true, true, CompareOp::LESS_EQUAL};

    auto brickwallMat = instancedMaterial.instance();
    brickwallMat.setSampler("diffuseMap", brickwall.newView(), Sampler(driver, {}));
    brickwallMat.setSampler("normalMap", brickwall_normal.newView(), Sampler(driver, {}));
    brickwallMat.setSampler("specularMap", brickwall_specular.newView(), Sampler(driver, {}));

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
            auto cameraData = camera.data();
            cameraBuffer.data({&cameraData, sizeof(cameraData)});
        }

        Device::FrameInfo frameInfo = driver.beginFrame();
        driver.waitFrame(frameInfo.frame);
        auto frame = driver.swapchain().nextImage();
        {
            frameInfo.cmd->begin();
            frameInfo.cmd->begin(frame.framebuffer);

            frameInfo.cmd->bindBindings({&binding, 1});
            frameInfo.cmd->bindAttributes(attributes);
            brickwallMat.bind(*frameInfo.cmd);

            frameInfo.cmd->bindBuffer(0, 0, cameraBuffer);
            frameInfo.cmd->bindBuffer(1, 0, modelBuffer);

            frameInfo.cmd->bindPipeline();
            frameInfo.cmd->bindDescriptors();

            frameInfo.cmd->bindVertexBuffer(0, vertexBuffer.buffer());
            frameInfo.cmd->drawIndirect(drawCommands, 0, 10);
//            cmd->draw(36, 1, 0, 0);

            frameInfo.cmd->end(frame.framebuffer);
            frameInfo.cmd->submit({&frame.imageAquired, 1}, frameInfo.fence);
        }
        driver.endFrame();
        driver.swapchain().present(frame, frameInfo.cmd->signal());
    }

    driver.wait();
}