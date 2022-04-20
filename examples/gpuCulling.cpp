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

    //Mesh data
    Mesh vertices = cala::shapes::cube();
    Buffer vertexBuffer = vertices.vertexBuffer(driver);
//    Buffer indexBuffer = vertices.indexBuffer(driver);

    Transform cameraTransform({0, 0, -10});
    Camera camera(ende::math::perspective((f32)ende::math::rad(54.4), 800.f / -600.f, 0.1f, 1000.f), cameraTransform);
    Buffer cameraBuffer(driver, sizeof(Camera::Data) + sizeof(std::array<std::pair<ende::math::Vec3f, f32>, 6>), BufferUsage::UNIFORM, MemoryProperties::HOST_VISIBLE | MemoryProperties::HOST_COHERENT);


    i32 maxSize = 100 * 100 * 100;
    i32 width = 20, height = 10, depth = 10;

    Transform modelTransform({0, 0, 0});
    ende::Vector<Transform> transforms;
    ende::Vector<ende::math::Mat4f> models;
    for (i32 i = 0; i < width; i++) {
        auto xpos = modelTransform.pos();
        for (i32 j = 0; j < height; j++) {
            auto ypos = modelTransform.pos();
            for (i32 k = 0; k < depth; k++) {
                modelTransform.addPos({0, 0, 3});
                transforms.push(modelTransform);
                models.push(modelTransform.toMat());
            }
            modelTransform.setPos(ypos + ende::math::Vec3f{0, 3, 0});
        }
        modelTransform.setPos(xpos + ende::math::Vec3f{3, 0, 0});
    }
    modelTransform.setPos({0, 0, 0});

    ende::Vector<u32> renderList;

    Buffer modelBuffer(driver, sizeof(ende::math::Mat4f) * maxSize, BufferUsage::STORAGE);
    modelBuffer.data({models.data(), static_cast<u32>(models.size() * sizeof(ende::math::Mat4f))});

    // use array if integers to index into transform buffer in shader
    Buffer renderBuffer(driver, sizeof(u32) + sizeof(u32) * maxSize, BufferUsage::STORAGE);

    Image brickwall = loadImage(driver, "../../res/textures/brickwall.jpg"_path);
    Image brickwall_normal = loadImage(driver, "../../res/textures/brickwall_normal.jpg"_path);
    Image brickwall_specular = loadImage(driver, "../../res/textures/brickwall_specular.jpg"_path);

    Material instancedMaterial(driver, std::move(program));
    instancedMaterial._depthState = {true, true, CompareOp::LESS_EQUAL};

    auto brickwallMat = instancedMaterial.instance();
    brickwallMat.setSampler("diffuseMap", brickwall.getView(), Sampler(driver, {}));
    brickwallMat.setSampler("normalMap", brickwall_normal.getView(), Sampler(driver, {}));
    brickwallMat.setSampler("specularMap", brickwall_specular.getView(), Sampler(driver, {}));

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
                cameraTransform.addPos(cameraTransform.rot().front() * dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_S])
                cameraTransform.addPos(cameraTransform.rot().back() * dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_A])
                cameraTransform.addPos(cameraTransform.rot().left() * -dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_D])
                cameraTransform.addPos(cameraTransform.rot().right() * -dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_LSHIFT])
                cameraTransform.addPos(cameraTransform.rot().down() * dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_SPACE])
                cameraTransform.addPos(cameraTransform.rot().up() * dt * 10);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_LEFT])
                cameraTransform.rotate({0, 1, 0}, ende::math::rad(45) * dt);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_RIGHT])
                cameraTransform.rotate({0, 1, 0}, ende::math::rad(-45) * dt);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_UP])
                cameraTransform.rotate(cameraTransform.rot().right(), ende::math::rad(-45) * dt);
            if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_DOWN])
                cameraTransform.rotate(cameraTransform.rot().right(), ende::math::rad(45) * dt);
        }

        {
            imGuiContext.newFrame();

            ImGui::Begin("Stats");
            ImGui::Text("FPS: %f", driver.fps());
            ImGui::Text("Milliseconds: %f", driver.milliseconds());
            ImGui::Text("Instances: %d", width * height * depth);
            ImGui::Text("Drawn: %d", renderList.size());
            ImGui::Text("Culled: %d", width * height * depth - renderList.size());

            if (ImGui::SliderInt("Width", &width, 1, 100) ||
                ImGui::SliderInt("Height", &height, 1, 100) ||
                ImGui::SliderInt("Depth", &depth, 1, 100)) {

                transforms.clear();
                models.clear();
                auto pos = modelTransform.pos();
                for (i32 i = 0; i < width; i++) {
                    auto xpos = modelTransform.pos();
                    for (i32 j = 0; j < height; j++) {
                        auto ypos = modelTransform.pos();
                        for (i32 k = 0; k < depth; k++) {
                            modelTransform.addPos({0, 0, 3});
                            transforms.push(modelTransform);
                            models.push(modelTransform.toMat());
                        }
                        modelTransform.setPos(ypos + ende::math::Vec3f{0, 3, 0});
                    }
                    modelTransform.setPos(xpos + ende::math::Vec3f{3, 0, 0});
                }
                modelTransform.setPos(pos);

                modelBuffer.data({models.data(), static_cast<u32>(models.size() * sizeof(ende::math::Mat4f))});
            }

            ImGui::End();

            ImGui::Render();
        }


        {
            auto cameraData = camera.data();
            cameraBuffer.data({&cameraData, sizeof(cameraData)});
            ende::math::Frustum frustum(camera.viewProjection());
            cameraBuffer.data({&frustum, sizeof(frustum)}, sizeof(cameraData));
        }

        driver.swapchain().wait();
        auto frame = driver.swapchain().nextImage();
        CommandBuffer* cmd = driver.beginFrame();
        {

            VkBufferMemoryBarrier barriers[3] = {};
            barriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            barriers[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barriers[0].buffer = cameraBuffer.buffer();
            barriers[0].offset = 0;
            barriers[0].size = cameraBuffer.size();

            barriers[1] = barriers[0];
            barriers[1].buffer = modelBuffer.buffer();
            barriers[1].size = modelBuffer.size();

            barriers[2] = barriers[0];
            barriers[2].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barriers[2].buffer = renderBuffer.buffer();
            barriers[2].size = renderBuffer.size();


            cmd->pipelineBarrier(PipelineStage::VERTEX_SHADER, PipelineStage::COMPUTE_SHADER, 0, {barriers, 3}, nullptr);

            cmd->clearDescriptors();
            cmd->bindProgram(computeProgram);
            cmd->bindBuffer(0, 0, cameraBuffer);
            cmd->bindBuffer(1, 0, modelBuffer);
            cmd->bindBuffer(1, 1, renderBuffer);

            cmd->bindPipeline();
            cmd->bindDescriptors();
            cmd->dispatchCompute(models.size() / 16, 1, 1);


            barriers[0].dstAccessMask = VK_ACCESS_HOST_READ_BIT;
            barriers[1].dstAccessMask = VK_ACCESS_HOST_READ_BIT;
            barriers[2].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barriers[2].dstAccessMask = VK_ACCESS_HOST_READ_BIT;
            cmd->pipelineBarrier(PipelineStage::COMPUTE_SHADER, PipelineStage::HOST, 0, {barriers, 3}, nullptr);

//            u32 drawCount = 0;
//            {
//                auto mapped = renderBuffer.map(0, sizeof(u32));
//                drawCount = *static_cast<u32*>(mapped.address);
//            }
//            ende::log::info("drawCount: {}", drawCount);

            cmd->begin(frame.framebuffer);

            cmd->bindBindings({binding, 1});
            cmd->bindAttributes(attributes);

            brickwallMat.bind(*cmd);
            cmd->bindBuffer(0, 0, cameraBuffer);

            cmd->bindBuffer(1, 0, modelBuffer);
            cmd->bindBuffer(1, 1, renderBuffer, sizeof(u32));

            cmd->bindPipeline();
            cmd->bindDescriptors();

            cmd->bindVertexBuffer(0, vertexBuffer.buffer());
//            cmd->bindVertexBuffer(1, modelBuffer.buffer());
//            if (drawCount)
                cmd->draw(36, models.size(), 0, 0);

            imGuiContext.render(*cmd);

            cmd->end(frame.framebuffer);
            cmd->submit(frame.imageAquired, frame.fence);
        }
        dt = driver.endFrame().milliseconds() / 1000.f;
        driver.swapchain().present(frame, cmd->signal());
    }

    driver.swapchain().wait();
}