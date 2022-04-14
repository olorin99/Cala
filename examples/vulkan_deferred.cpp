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
    Image::View view = image.getView(VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT);

    //Material
    Material gbufferMaterial(driver, std::move(gbufferProgram));
    gbufferMaterial._depthState = {true, true, CompareOp::LESS_EQUAL};
    Material deferredMaterial(driver, std::move(deferredProgram));
    deferredMaterial._rasterState = {.cullMode = CullMode::NONE};

    //Mesh data
    Mesh vertices = cala::shapes::sphereNormalized();
    Buffer vertexBuffer = vertices.vertexBuffer(driver);
    Buffer indexBuffer = vertices.indexBuffer(driver);

    Mesh fullTriangle = cala::shapes::triangle(6, 6);
//    Mesh fullTriangle = cala::shapes::cube();
    Buffer fullTriangleVertexBuffer = fullTriangle.vertexBuffer(driver);
    Buffer fullTriangleIndexBuffer = fullTriangle.indexBuffer(driver);

    Transform model;
    Buffer modelBuffer(driver, sizeof(ende::math::Mat4f), BufferUsage::UNIFORM, MemoryProperties::HOST_VISIBLE | MemoryProperties::HOST_COHERENT);
    Transform model1({0, 0, -1});
    Buffer model1Buffer(driver, sizeof(ende::math::Mat4f), BufferUsage::UNIFORM, MemoryProperties::HOST_VISIBLE | MemoryProperties::HOST_COHERENT);


    Image gAlbedo(driver, {800, 600, 1, Format::RGBA32_SFLOAT, 1, 1, ImageUsage::COLOUR_ATTACHMENT | ImageUsage::SAMPLED});
    Image gNormal(driver, {800, 600, 1, Format::RGBA32_SFLOAT, 1, 1, ImageUsage::COLOUR_ATTACHMENT | ImageUsage::SAMPLED});
    Image gDepth(driver, {800, 600, 1, Format::D32_SFLOAT, 1, 1, ImageUsage::DEPTH_STENCIL_ATTACHMENT | ImageUsage::SAMPLED});

    auto gAlbedoView = gAlbedo.getView();
    auto gNormalView = gNormal.getView();
    auto gDepthView = gDepth.getView(VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_DEPTH_BIT);

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

    VkImageView gFrameAttachments[3] = {
            gAlbedoView.view,
            gNormalView.view,
            gDepthView.view
    };
    Framebuffer gFrameBuffer(driver.context().device(), gbufferPass, gFrameAttachments, 800, 600);

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
//            cmd->begin(frame.framebuffer);

            cmd->bindProgram(gbufferMaterial._program);
            cmd->bindBindings({&binding, 1});
            cmd->bindAttributes(attributes);
            cmd->bindRasterState(gbufferMaterial._rasterState);
            cmd->bindDepthState(gbufferMaterial._depthState);
            cmd->bindPipeline();

            cmd->bindBuffer(0, 0, cameraBuffer);
            cmd->bindBuffer(1, 0, modelBuffer);
            cmd->bindImage(2, 0, view, sampler);
            cmd->bindDescriptors();

            cmd->bindVertexBuffer(0, vertexBuffer.buffer());
            cmd->bindIndexBuffer(indexBuffer);
            cmd->draw(indexBuffer.size() / sizeof(u32), 1, 0, 0);

            cmd->end(gFrameBuffer);

            cmd->clearDescriptors();
            cmd->begin(frame.framebuffer);

            cmd->bindProgram(deferredMaterial._program);
            cmd->bindBindings({&binding, 1});
            cmd->bindAttributes(attributes);
            cmd->bindRasterState(deferredMaterial._rasterState);
            cmd->bindDepthState(deferredMaterial._depthState);
            cmd->bindPipeline();

            cmd->bindImage(0, 0, gAlbedoView, sampler);
            cmd->bindImage(0, 1, gNormalView, sampler);
            cmd->bindImage(0, 2, gDepthView, sampler);
            cmd->bindDescriptors();

            cmd->bindVertexBuffer(0, fullTriangleVertexBuffer.buffer());
            cmd->draw(3, 1, 0, 0);


            imGuiContext.render(*cmd);

            cmd->end(frame.framebuffer);
            cmd->clearDescriptors();
            cmd->submit(frame.imageAquired, frame.fence);
        }
        driver.endFrame();
        driver.swapchain().present(frame, cmd->signal());
    }

    driver.swapchain().wait();
}


/*

Image loadImage(Driver& driver, const ende::fs::Path& path) {
    i32 width, height, channels;
    u8* data = stbi_load((*path).c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!data) throw "unable load image";
    u32 length = width * height * 4;

    Image image(driver, {(u32)width, (u32)height, 1, VK_FORMAT_R8G8B8A8_SRGB});
    image.data(driver, {0, (u32)width, (u32)height, 1, 4, {data, length}});
    stbi_image_free(data);
    return image;
}

int main() {

    SDLPlatform platform("Deferred", 800, 600);

    Driver driver(platform);

    ImGuiContext imGuiContext(driver, platform.window());

    // get shader data
    ende::fs::File shaderFile;
    if (!shaderFile.open("../../res/shaders/default.vert.spv"_path, ende::fs::in | ende::fs::binary))
        return -1;

    ende::Vector<u32> vertexShaderData(shaderFile.size() / sizeof(u32));
    shaderFile.read({reinterpret_cast<char*>(vertexShaderData.data()), static_cast<u32>(vertexShaderData.size() * sizeof(u32))});

    if (!shaderFile.open("../../res/shaders/gbuffer.frag.spv"_path, ende::fs::in | ende::fs::binary))
        return -2;
    ende::Vector<u32> fragmentShaderData(shaderFile.size() / sizeof(u32));
    shaderFile.read({reinterpret_cast<char*>(fragmentShaderData.data()), static_cast<u32>(fragmentShaderData.size() * sizeof(u32))});

    if (!shaderFile.open("../../res/shaders/deferred.vert.spv"_path, ende::fs::in | ende::fs::binary))
        return -2;
    ende::Vector<u32> deferredVertexShaderData(shaderFile.size() / sizeof(u32));
    shaderFile.read({reinterpret_cast<char*>(deferredVertexShaderData.data()), static_cast<u32>(deferredVertexShaderData.size() * sizeof(u32))});

    if (!shaderFile.open("../../res/shaders/deferred.frag.spv"_path, ende::fs::in | ende::fs::binary))
        return -2;
    ende::Vector<u32> deferredFragmentShaderData(shaderFile.size() / sizeof(u32));
    shaderFile.read({reinterpret_cast<char*>(deferredFragmentShaderData.data()), static_cast<u32>(deferredFragmentShaderData.size() * sizeof(u32))});

    ShaderProgram gbufferProgram = ShaderProgram::create()
            .addStage(vertexShaderData, VK_SHADER_STAGE_VERTEX_BIT)
            .addStage(fragmentShaderData, VK_SHADER_STAGE_FRAGMENT_BIT)
            .compile(driver);

    ShaderProgram deferredProgram = ShaderProgram::create()
            .addStage(deferredVertexShaderData, VK_SHADER_STAGE_VERTEX_BIT)
            .addStage(deferredFragmentShaderData, VK_SHADER_STAGE_FRAGMENT_BIT)
            .compile(driver);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 0;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.f;
    samplerInfo.minLod = 0.f;
    samplerInfo.maxLod = 0.f;

    VkSampler sampler;
    vkCreateSampler(driver._context._device, &samplerInfo, nullptr, &sampler);

    Image image = loadImage(driver, "../../res/textures/metal-sheet.jpg"_path);
    Image::View view = image.getView(VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT);


    //vertex array
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = 14 * sizeof(f32);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attributes[5] = {
            { .location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 0 },
            { .location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = sizeof(f32) * 3 },
            { .location = 2, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = sizeof(f32) * 6 },
            { .location = 3, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = sizeof(f32) * 8 },
            { .location = 4, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = sizeof(f32) * 11 }
    };


    Transform cameraTransform({0, 0, -1});
    Camera camera(ende::math::perspective(45.f, 800.f / -600.f, 0.f, 1000.f), cameraTransform);
    Buffer cameraBuffer(driver._context, sizeof(Camera::Data), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);


    Mesh vertices = cala::shapes::sphere();
    Buffer vertexBuffer = vertices.vertexBuffer(driver._context);
    Buffer indexBuffer = vertices.indexBuffer(driver._context);

    Mesh fullTriangle = cala::shapes::triangle(6, 6);
    Buffer fullTriangleVertexBuffer = fullTriangle.vertexBuffer(driver._context);

    Transform model;
    Buffer modelBuffer(driver._context, sizeof(ende::math::Mat4f), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);


    Image gAlbedo(driver._context, {800, 600, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 1, 1, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT});
    auto gAlbedoView = gAlbedo.getView(VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT);
    Image gNormal(driver._context, {800, 600, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 1, 1, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT});
    auto gNormalView = gNormal.getView(VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT);
    Image gDepth(driver._context, {800, 600, 1, VK_FORMAT_D32_SFLOAT, 1, 1, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT});
    auto gDepthView = gDepth.getView(VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_DEPTH_BIT);

    u32 currentImage = 0;

    std::array<RenderPass::Attachment, 3> attachments = {
            RenderPass::Attachment{
                    VK_FORMAT_R32G32B32A32_SFLOAT,
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
                    VK_FORMAT_R32G32B32A32_SFLOAT,
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
                    VK_FORMAT_D32_SFLOAT,
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
    RenderPass gbufferPass(driver._context._device, attachments);
    VkImageView gframeAttachments[3] = { gAlbedoView.view, gNormalView.view, gDepthView.view };
    Framebuffer gframeBuffer(driver._context._device, gbufferPass, gframeAttachments, 800, 600);

    bool running = true;
    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_KEYDOWN:
                    switch (event.key.keysym.scancode) {
                        case SDL_SCANCODE_L:
                            currentImage++;
                            break;
                        default:
                            break;
                    }
                    break;
            }
        }


        driver._swapchain.wait();

        {
            auto cameraData = camera.data();
            cameraBuffer.data({&cameraData, sizeof(cameraData)});

            auto modelData = model.toMat();
            modelBuffer.data({&modelData, sizeof(modelData)});
        }

        auto frame = driver._swapchain.nextImage();

        CommandBuffer* cmd = driver.beginFrame();
        {
            cmd->begin(gframeBuffer);

            cmd->bindProgram(gbufferProgram);
            cmd->bindVertexArray({&binding, 1}, {attributes, 5});
            cmd->bindRasterState({});
            cmd->bindDepthState({true, true, VK_COMPARE_OP_LESS_OR_EQUAL});
            cmd->bindPipeline();

            cmd->bindBuffer(0, 0, cameraBuffer);
            cmd->bindBuffer(1, 0, modelBuffer);
            cmd->bindImage(2, 0, view.view, sampler);
            cmd->bindDescriptors();

            cmd->bindVertexBuffer(0, vertexBuffer.buffer());
            cmd->bindIndexBuffer(indexBuffer);
            cmd->draw(indexBuffer.size() / sizeof(u32), 1, 0, 0);


            cmd->end(gframeBuffer);
            cmd->clearDescriptors();
            cmd->begin(frame.framebuffer);

            cmd->bindProgram(deferredProgram);
            cmd->bindVertexArray({&binding, 1}, {attributes, 5});
            cmd->bindRasterState({.cullMode=VK_CULL_MODE_NONE});
            cmd->bindDepthState({});
            cmd->bindPipeline();

            cmd->bindImage(0, currentImage % 3, gAlbedoView.view, sampler);
            cmd->bindImage(0, (currentImage + 1) % 3, gNormalView.view, sampler);
            cmd->bindImage(0, (currentImage + 2) % 3, gDepthView.view, sampler);
            cmd->bindDescriptors();

            cmd->bindVertexBuffer(0, fullTriangleVertexBuffer.buffer());
            cmd->draw(3, 1, 0, 0);

            cmd->end(frame.framebuffer);
            cmd->clearDescriptors();
            cmd->submit(frame.imageAquired, frame.fence);
        }
        driver.endFrame();
        driver._swapchain.present(frame, cmd->signal());

    }
    driver._swapchain.wait();

    vkDestroySampler(driver._context._device, sampler, nullptr);

}*/