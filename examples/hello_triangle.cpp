#include <Cala/backend/vulkan/SDLPlatform.h>
#include <Cala/backend/vulkan/Device.h>
#include <Cala/shapes.h>
#include <Cala/Camera.h>
#include <Ende/filesystem/File.h>

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

int main() {
    SDLPlatform platform("hello_triangle", 800, 600);
    Device driver(platform);

    //Shaders
    ShaderProgram program = loadShader(driver, "../../res/shaders/triangle.vert.spv"_path, "../../res/shaders/triangle.frag.spv"_path);

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
    MeshData vertices = cala::shapes::triangle();
    Buffer vertexBuffer = vertices.vertexBuffer(driver);
//    Buffer indexBuffer = vertices.indexBuffer(device);

    Transform cameraTransform({0, 0, -10});
    Camera camera(ende::math::perspective((f32)ende::math::rad(54.4), 800.f / -600.f, 0.1f, 1000.f), cameraTransform);
    Buffer cameraBuffer(driver, sizeof(Camera::Data), BufferUsage::UNIFORM, MemoryProperties::HOST_VISIBLE | MemoryProperties::HOST_COHERENT);

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

            frameInfo.cmd->bindProgram(program);
            frameInfo.cmd->bindBindings({&binding, 1});
            frameInfo.cmd->bindAttributes(attributes);
            frameInfo.cmd->bindRasterState({.cullMode=CullMode::NONE});
            frameInfo.cmd->bindDepthState({});
            frameInfo.cmd->bindPipeline();

            frameInfo.cmd->bindVertexBuffer(0, vertexBuffer.buffer());
            frameInfo.cmd->draw(3, 1, 0, 0);

            frameInfo.cmd->end(frame.framebuffer);
            frameInfo.cmd->submit({&frame.imageAquired, 1}, frameInfo.fence);
        }
        driver.endFrame();
        driver.swapchain().present(frame, frameInfo.cmd->signal());
    }

    driver.wait();
}