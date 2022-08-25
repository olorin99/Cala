#include <iostream>

#include <Cala/backend/vulkan/SDLPlatform.h>
#include <Cala/backend/vulkan/Driver.h>
#include <Cala/Scene.h>
#include <Cala/ImGuiContext.h>
#include <Cala/Camera.h>
#include <Cala/shapes.h>
#include <Cala/Material.h>

#include <Ende/filesystem/File.h>

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
    if (!data)
        throw "Unable to load image";
    i32 length = width * height * channels;

    Image image(driver, {(u32)width, (u32)height, 1, Format::RGBA8_SRGB});
    image.data(driver, {0, (u32)width, (u32)height, 1, 4, {data, (u32)length}});
    stbi_image_free(data);
    return image;
}

int main() {

    SDLPlatform platform("Deferred Example", 800, 600);
    Driver driver(platform);
    ImGuiContext imGuiContext(driver, platform.window());

    Scene scene;

    //Camera
    Transform cameraTransform({0, 0, -1});
    Camera camera(ende::math::perspective((f32)ende::math::rad(54.5f), 800.f / -600.f, 0.1f, 1000.f), cameraTransform);
    Buffer cameraBuffer(driver, sizeof(Camera::Data), BufferUsage::UNIFORM, MemoryProperties::HOST_VISIBLE | MemoryProperties::HOST_COHERENT);

    //Shaders
    ShaderProgram gbufferProgram = loadShader(driver, "../../res/shaders/default.vert.spv"_path, "../../res/shaders/gbuffer.frag.spv"_path);
    ShaderProgram deferredProgram = loadShader(driver, "../../res/shaders/deferred.vert.spv"_path, "../../res/shaders/deferred.frag.spv"_path);

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

    //Sampler
    Sampler sampler(driver, {});

    //Image
    Image image = loadImage(driver, "../../res/textures/metal-sheet.jpg"_path);
//    Image::View view = image.getView(VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT);

    //Material
    Material gbufferMaterial(driver, std::move(gbufferProgram));
    gbufferMaterial._depthState = {true, true, CompareOp::LESS_EQUAL};
    Material deferredMaterial(driver, std::move(deferredProgram));
    deferredMaterial._rasterState = {.cullMode = CullMode::NONE};

    //MeshData data
    MeshData vertices = cala::shapes::sphereNormalized();
    Buffer vertexBuffer = vertices.vertexBuffer(driver);
    Buffer indexBuffer = vertices.indexBuffer(driver);

    MeshData fullTriangle = cala::shapes::triangle(6, 6);
//    MeshData fullTriangle = cala::shapes::cube();
    Buffer fullTriangleVertexBuffer = fullTriangle.vertexBuffer(driver);
    Buffer fullTriangleIndexBuffer = fullTriangle.indexBuffer(driver);

    Transform model;
    Buffer modelBuffer(driver, sizeof(ende::math::Mat4f), BufferUsage::UNIFORM, MemoryProperties::HOST_VISIBLE | MemoryProperties::HOST_COHERENT);
    Transform model1({0, 0, -1});
    Buffer model1Buffer(driver, sizeof(ende::math::Mat4f), BufferUsage::UNIFORM, MemoryProperties::HOST_VISIBLE | MemoryProperties::HOST_COHERENT);


    Image gAlbedo(driver, {800, 600, 1, Format::RGBA32_SFLOAT, 1, 1, ImageUsage::COLOUR_ATTACHMENT | ImageUsage::SAMPLED});
    Image gNormal(driver, {800, 600, 1, Format::RGBA32_SFLOAT, 1, 1, ImageUsage::COLOUR_ATTACHMENT | ImageUsage::SAMPLED});
    Image gDepth(driver, {800, 600, 1, Format::D32_SFLOAT, 1, 1, ImageUsage::DEPTH_STENCIL_ATTACHMENT | ImageUsage::SAMPLED});

    auto metalMatInstance = gbufferMaterial.instance();
    metalMatInstance.setSampler("metalMap", image.getView(), Sampler(driver, {}));

    auto deferredInstance = deferredMaterial.instance();
    deferredInstance.setSampler(0, "gAlbedoMap", gAlbedo.getView(), Sampler(driver, {}));
    deferredInstance.setSampler(0, "gNormalMap", gNormal.getView(), Sampler(driver, {}));
    deferredInstance.setSampler(0, "gDepthMap", gDepth.getView(VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_DEPTH_BIT), Sampler(driver, {}));

    std::array<RenderPass::Attachment, 3> attachments = {
            RenderPass::Attachment{
                    Format::RGBA32_SFLOAT,
                    VK_SAMPLE_COUNT_1_BIT,
                    VK_ATTACHMENT_LOAD_OP_CLEAR,
                    VK_ATTACHMENT_STORE_OP_STORE,
                    VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                    VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            },
            RenderPass::Attachment{
                    Format::RGBA32_SFLOAT,
                    VK_SAMPLE_COUNT_1_BIT,
                    VK_ATTACHMENT_LOAD_OP_CLEAR,
                    VK_ATTACHMENT_STORE_OP_STORE,
                    VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                    VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            },
            RenderPass::Attachment{
                    Format::D32_SFLOAT,
                    VK_SAMPLE_COUNT_1_BIT,
                    VK_ATTACHMENT_LOAD_OP_CLEAR,
                    VK_ATTACHMENT_STORE_OP_STORE,
                    VK_ATTACHMENT_LOAD_OP_CLEAR,
                    VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
            }
    };

    RenderPass gbufferPass(driver, attachments);

    Framebuffer gFrameBuffer(driver.context().device(), gbufferPass, &deferredInstance, 800, 600);

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


        driver.swapchain().wait();
        {
            auto cameraData = camera.data();
            cameraBuffer.data({&cameraData, sizeof(cameraData)});

            auto modelData = model.toMat();
            modelBuffer.data({&modelData, sizeof(modelData)});

            auto model1Data = model1.toMat();
            model1Buffer.data({&model1Data, sizeof(model1Data)});
        }

        auto frame = driver.swapchain().nextImage();

        CommandBuffer* cmd = driver.beginFrame();
        {
            imGuiContext.newFrame();
            ImGui::Begin("Device");
            ImGui::Text("Vendor: %s", driver.context().vendor());
            ImGui::Text("Device: %s", driver.context().deviceName().data());
            ImGui::Text("Type: %s", driver.context().deviceTypeString());
            ImGui::End();
            ImGui::Render();



            cmd->begin(gFrameBuffer);

            metalMatInstance.bind(*cmd);
            cmd->bindBindings({&binding, 1});
            cmd->bindAttributes(attributes);

            cmd->bindBuffer(0, 0, cameraBuffer);
            cmd->bindBuffer(1, 0, modelBuffer);
            cmd->bindPipeline();
            cmd->bindDescriptors();

            cmd->bindVertexBuffer(0, vertexBuffer.buffer());
            cmd->bindIndexBuffer(indexBuffer);
            cmd->draw(indexBuffer.size() / sizeof(u32), 1, 0, 0);

            cmd->end(gFrameBuffer);

            cmd->clearDescriptors();
            cmd->begin(frame.framebuffer);

            cmd->bindBindings({&binding, 1});
            cmd->bindAttributes(attributes);

            deferredInstance.bind(*cmd, 0);
            cmd->bindPipeline();
            cmd->bindDescriptors();

            cmd->bindVertexBuffer(0, fullTriangleVertexBuffer.buffer());
            cmd->draw(3, 1, 0, 0);


            imGuiContext.render(*cmd);

            cmd->end(frame.framebuffer);
            cmd->clearDescriptors();
            cmd->submit(frame.imageAquired, frame.fence);
        }
        auto frameTime = driver.endFrame();
        dt = static_cast<f32>(frameTime.milliseconds()) / 1000.f;

        driver.swapchain().present(frame, cmd->signal());
    }

    driver.swapchain().wait();
}