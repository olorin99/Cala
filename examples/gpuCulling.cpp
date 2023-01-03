#include <Cala/backend/vulkan/SDLPlatform.h>
#include <Cala/backend/vulkan/Driver.h>
#include <Cala/shapes.h>
#include <Cala/Material.h>
#include <Cala/Camera.h>
#include <Ende/filesystem/File.h>
#include <Ende/math/Frustum.h>
#include <Ende/log/log.h>
#include <Cala/ImGuiContext.h>
#include <Cala/backend/vulkan/CommandBufferList.h>
#include <Cala/backend/vulkan/Timer.h>
#include "../third_party/stb_image.h"

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

int main() {
    SDLPlatform platform("hello_triangle", 800, 600);
    Driver driver(platform);
    ImGuiContext imGuiContext(driver, platform.window());

    //Shaders
    ShaderProgram program = loadShader(driver, "../../res/shaders/instancedCulled.vert.spv"_path, "../../res/shaders/default.frag.spv"_path);
    ende::fs::File shaderFile;
    if (!shaderFile.open("../../res/shaders/cull.comp.spv"_path, ende::fs::in | ende::fs::binary))
        return -2;
    ende::Vector<u32> computeShaderData(shaderFile.size() / sizeof(u32));
    shaderFile.read({reinterpret_cast<char*>(computeShaderData.data()), static_cast<u32>(computeShaderData.size() * sizeof(u32))});
    ShaderProgram computeProgram = ShaderProgram::create()
            .addStage(computeShaderData, ShaderStage::COMPUTE)
            .compile(driver);

    //Vertex Input
    VkVertexInputBindingDescription binding[2]{};
    binding[0].binding = 0;
    binding[0].stride = 14 * sizeof(f32);
    binding[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

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
    Buffer cameraBuffer(driver, sizeof(Camera::Data) + 3 * 4 + sizeof(std::array<std::pair<ende::math::Vec3f, f32>, 6>), BufferUsage::UNIFORM, MemoryProperties::HOST_VISIBLE | MemoryProperties::HOST_COHERENT);


    i32 maxSize = 100 * 100 * 100;
    i32 width = 20, height = 10, depth = 10;

    Transform modelTransform({0, 0, 0});
    ende::Vector<Transform> transforms;
    ende::Vector<ende::math::Mat4f> models;
    ende::Vector<std::pair<ende::math::Vec3f, f32>> positions;
    for (i32 i = 0; i < width; i++) {
        auto xpos = modelTransform.pos();
        for (i32 j = 0; j < height; j++) {
            auto ypos = modelTransform.pos();
            for (i32 k = 0; k < depth; k++) {
                modelTransform.addPos({0, 0, 3});
                transforms.push(modelTransform);
                models.push(modelTransform.toMat());
                positions.push({modelTransform.pos(), 1});
            }
            modelTransform.setPos(ypos + ende::math::Vec3f{0, 3, 0});
        }
        modelTransform.setPos(xpos + ende::math::Vec3f{3, 0, 0});
    }
    modelTransform.setPos({0, 0, 0});

    ende::Vector<u32> renderList;

    Buffer modelBuffer(driver, sizeof(ende::math::Mat4f) * maxSize, BufferUsage::STORAGE);
    modelBuffer.data({models.data(), static_cast<u32>(models.size() * sizeof(ende::math::Mat4f))});

    Buffer positionBuffer(driver, (sizeof(ende::math::Vec3f) + sizeof(f32)) * maxSize, BufferUsage::STORAGE);
    positionBuffer.data({positions.data(), static_cast<u32>(positions.size() * (sizeof(ende::math::Vec3f) + sizeof(f32)))});
    // use array if integers to index into transform buffer in shader
    Buffer renderBuffer(driver, sizeof(u32) * maxSize, BufferUsage::STORAGE);
    Buffer outputBuffer(driver, sizeof(u32), BufferUsage::STORAGE);

    auto mappedOutput = outputBuffer.map(0, sizeof(u32));

    Image brickwall = loadImage(driver, "../../res/textures/brickwall.jpg"_path);
    Image brickwall_normal = loadImage(driver, "../../res/textures/brickwall_normal.jpg"_path);
    Image brickwall_specular = loadImage(driver, "../../res/textures/brickwall_specular.jpg"_path);

    Material instancedMaterial(driver, std::move(program));
    instancedMaterial._depthState = {true, true, CompareOp::LESS_EQUAL};

    auto brickwallMat = instancedMaterial.instance();
    brickwallMat.setSampler("diffuseMap", brickwall.getView(), Sampler(driver, {}));
    brickwallMat.setSampler("normalMap", brickwall_normal.getView(), Sampler(driver, {}));
    brickwallMat.setSampler("specularMap", brickwall_specular.getView(), Sampler(driver, {}));


//    CommandBufferList computeList(driver, QueueType::COMPUTE);

    Timer computeTimer(driver);
    Timer rasterTimer(driver, 1);

    u64 computeTime = 0;
    u64 rasterTime = 0;

    ende::math::Mat4f viewProj = camera.viewProjection();
    bool freezeFrustum = false;

    u32 drawCount = 0;
    f32 dt = 1.f / 60.f;
    ende::time::Duration frameTime;
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

            ImGui::Begin("Stats");
            ImGui::Text("FPS: %f", driver.fps());
            ImGui::Text("Milliseconds: %f", driver.milliseconds());
            ImGui::Text("Compute Milliseconds: %f", computeTime / 1e6);
            ImGui::Text("Raster Milliseconds: %f", rasterTime / 1e6);
            ImGui::Text("Instances: %d", width * height * depth);
            ImGui::Text("Drawn: %d", drawCount);
            ImGui::Text("Culled: %d", width * height * depth - drawCount);
            ImGui::Checkbox("Frustum", &freezeFrustum);

            if (ImGui::SliderInt("Width", &width, 1, 100) ||
                ImGui::SliderInt("Height", &height, 1, 100) ||
                ImGui::SliderInt("Depth", &depth, 1, 100)) {

                transforms.clear();
                models.clear();
                positions.clear();
                auto pos = modelTransform.pos();
                for (i32 i = 0; i < width; i++) {
                    auto xpos = modelTransform.pos();
                    for (i32 j = 0; j < height; j++) {
                        auto ypos = modelTransform.pos();
                        for (i32 k = 0; k < depth; k++) {
                            modelTransform.addPos({0, 0, 3});
                            transforms.push(modelTransform);
                            models.push(modelTransform.toMat());
                            positions.push({modelTransform.pos(), 1});
                        }
                        modelTransform.setPos(ypos + ende::math::Vec3f{0, 3, 0});
                    }
                    modelTransform.setPos(xpos + ende::math::Vec3f{3, 0, 0});
                }
                modelTransform.setPos(pos);

                modelBuffer.data({models.data(), static_cast<u32>(models.size() * sizeof(ende::math::Mat4f))});
                positionBuffer.data({positions.data(), static_cast<u32>(positions.size() * (sizeof(ende::math::Vec3f) + sizeof(f32)))});
            }

            ImGui::End();

            ImGui::Render();
        }


        {
            auto cameraData = camera.data();
            cameraBuffer.data({&cameraData, sizeof(cameraData)});
            if (!freezeFrustum)
                viewProj = camera.viewProjection();
            ende::math::Frustum frustum(viewProj);
            cameraBuffer.data({&frustum, sizeof(frustum)}, sizeof(cameraData) + 4 * 3);
        }


        Driver::FrameInfo frameInfo = driver.beginFrame();
        driver.waitFrame(frameInfo.frame);
        auto frame = driver.swapchain().nextImage();
        {
            frameInfo.cmd->begin();
            computeTimer.start(*frameInfo.cmd);

            VkBufferMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            barrier.buffer = renderBuffer.buffer();
            barrier.offset = 0;
            barrier.size = renderBuffer.size();
            barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;


            frameInfo.cmd->pipelineBarrier(PipelineStage::VERTEX_SHADER, PipelineStage::COMPUTE_SHADER, 0, {&barrier, 1}, nullptr);

            frameInfo.cmd->clearDescriptors();
            frameInfo.cmd->bindProgram(computeProgram);
            frameInfo.cmd->bindBuffer(0, 0, cameraBuffer);
            frameInfo.cmd->bindBuffer(1, 0, modelBuffer);
            frameInfo.cmd->bindBuffer(1, 1, renderBuffer);
            frameInfo.cmd->bindBuffer(1, 2, positionBuffer);
            frameInfo.cmd->bindBuffer(2, 0, outputBuffer);

            frameInfo.cmd->bindPipeline();
            frameInfo.cmd->bindDescriptors();
            frameInfo.cmd->dispatchCompute(models.size() / 16, 1, 1);

            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            frameInfo.cmd->pipelineBarrier(PipelineStage::COMPUTE_SHADER, PipelineStage::VERTEX_SHADER, 0, {&barrier, 1}, nullptr);

            computeTimer.stop();


            frameInfo.cmd->clearDescriptors();
            rasterTimer.start(*frameInfo.cmd);
            frameInfo.cmd->begin(*frame.framebuffer);

            frameInfo.cmd->bindBindings({binding, 1});
            frameInfo.cmd->bindAttributes(attributes);

            brickwallMat.bind(*frameInfo.cmd);
            frameInfo.cmd->bindBuffer(0, 0, cameraBuffer);

            frameInfo.cmd->bindBuffer(1, 0, modelBuffer);
            frameInfo.cmd->bindBuffer(1, 1, renderBuffer);

            frameInfo.cmd->bindPipeline();
            frameInfo.cmd->bindDescriptors();

            frameInfo.cmd->bindVertexBuffer(0, vertexBuffer.buffer());
            if (drawCount)
                frameInfo.cmd->draw(36, drawCount, 0, 0);

            imGuiContext.render(*frameInfo.cmd);

            rasterTimer.stop();
            frameInfo.cmd->end(*frame.framebuffer);

            frameInfo.cmd->end();
            frameInfo.cmd->submit({ &frame.imageAquired, 1 }, frameInfo.fence);

            drawCount = *static_cast<u32*>(mappedOutput.address);
        }
        driver.endFrame();
        dt = driver.milliseconds() / 1000.f;
        driver.swapchain().present(frame, frameInfo.cmd->signal());

        computeTime = computeTimer.result() ? computeTimer.result() : computeTime;
        rasterTime = rasterTimer.result() ? rasterTimer.result() : rasterTime;
    }

    driver.wait();
}