
#include <iostream>

#include <Ende/platform.h>
#include <Ende/Vector.h>
#include <Ende/filesystem/File.h>
#include <Ende/math/Vec.h>
#include <Ende/time/StopWatch.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <SDL2/SDL_syswm.h>

#include <vulkan/vulkan.h>

#include <Cala/backend/vulkan/ShaderProgram.h>
#include <Cala/backend/vulkan/Driver.h>

#include <Cala/backend/vulkan/CommandBuffer.h>

#include "../third_party/imgui/imgui.h"
#include "../third_party/imgui/imgui_impl_vulkan.h"
#include "../third_party/imgui/imgui_impl_sdl.h"

using namespace cala::backend::vulkan;

struct Vertex {
    ende::math::Vec<2, f32> position;
    ende::math::Vec3f colour;
};

VkCommandBuffer beginSingleTimeCommands(VkDevice device, VkCommandPool pool) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = pool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    return commandBuffer;
}

void endSingleTimeCommands(VkCommandBuffer commandBuffer, VkDevice device, VkQueue queue, VkCommandPool pool) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(device, pool, 1, &commandBuffer);
}

int main() {

    bool renderImGui = true;

    auto window = SDL_CreateWindow("hello", 0, 0, 800, 600, SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN);

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(window, &wmInfo);

    u32 extensionCount = 0;
    SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, nullptr);
    ende::Vector<const char*> extensionNames(extensionCount);
    SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, extensionNames.data());

    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    ende::Vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
    for (const auto& extension : extensions)
        std::cout << '\t' << extension.extensionName << '\n';

    Driver driver(extensionNames, &wmInfo.info.x11.window, wmInfo.info.x11.display);

    VkCommandPool commandPool;
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = driver._context.queueIndex(VK_QUEUE_GRAPHICS_BIT);
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(driver._context._device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
        return -65;


    VkRenderPass ImGuiRenderPass;
    {
        VkAttachmentDescription colourAttachment{};
        colourAttachment.format = driver._swapchain.format();
        colourAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colourAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colourAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colourAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colourAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colourAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colourAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colourAttachmentRef{};
        colourAttachmentRef.attachment = 0;
        colourAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colourAttachmentRef;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colourAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;
        if (vkCreateRenderPass(driver._context._device, &renderPassInfo, nullptr, &ImGuiRenderPass) != VK_SUCCESS)
            return -15;
    }

    VkDescriptorPool ImGuiDescriptorPool;
    {
        VkDescriptorPoolSize poolSizes[] = {
                { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
                { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
                { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
                { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
                { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
                { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
                { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
                { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
                { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
                { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
                { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 },
        };

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 11;
        poolInfo.pPoolSizes = poolSizes;
        poolInfo.maxSets = driver._swapchain.size();

        if (vkCreateDescriptorPool(driver._context._device, &poolInfo, nullptr, &ImGuiDescriptorPool) != VK_SUCCESS)
            return -17;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::GetStyle().WindowRounding = 0.f;
        ImGui::GetStyle().Colors[ImGuiCol_WindowBg].w = 1.f;
    }

    ImGui_ImplSDL2_InitForVulkan(window);
    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = driver._context._instance;
    initInfo.PhysicalDevice = driver._context._physicalDevice;
    initInfo.Device = driver._context._device;
    initInfo.QueueFamily = driver._context.queueIndex(VK_QUEUE_GRAPHICS_BIT);
    initInfo.Queue = driver._context.getQueue(VK_QUEUE_GRAPHICS_BIT);
    initInfo.PipelineCache = VK_NULL_HANDLE;
    initInfo.DescriptorPool = ImGuiDescriptorPool;
    initInfo.Allocator = nullptr;
    initInfo.MinImageCount = driver._swapchain.size();
    initInfo.ImageCount = driver._swapchain.size();
    initInfo.CheckVkResultFn = nullptr;
    ImGui_ImplVulkan_Init(&initInfo, ImGuiRenderPass);
    VkCommandBuffer a = beginSingleTimeCommands(driver._context._device, commandPool);
    ImGui_ImplVulkan_CreateFontsTexture(a);
    endSingleTimeCommands(a, driver._context._device, driver._context.getQueue(VK_QUEUE_GRAPHICS_BIT), commandPool);



    // buffer

    const Vertex vertices[] = {
            {{0.f, -0.5f}, {1.f, 0.f, 0.f}},
            {{0.5f, 0.5f}, {0.f, 1.f, 0.f}},
            {{-0.5f, 0.5f}, {0.f, 0.f, 1.f}}
    };

    auto [vertexBuffer, vertexBufferMemory] = driver._context.createBuffer(sizeof(vertices[0]) * 3, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void* data;
    vkMapMemory(driver._context._device, vertexBufferMemory, 0, sizeof(vertices[0]) * 3, 0, &data);
    memcpy(data, vertices, sizeof(vertices[0]) * 3);
    vkUnmapMemory(driver._context._device, vertexBufferMemory);


    auto [uniformBuffer, uniformBufferMemory] = driver._context.createBuffer(sizeof(ende::math::Vec3f), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    void* uniformData;
    vkMapMemory(driver._context._device, uniformBufferMemory, 0, sizeof(ende::math::Vec3f), 0, &uniformData);
    *static_cast<ende::math::Vec3f*>(uniformData) = {0.5, 0.5, 0.5};
    vkUnmapMemory(driver._context._device, uniformBufferMemory);

    // get shader data
    ende::fs::File shaderFile;
    if (!shaderFile.open("../../res/shaders/triangle.vert.spv"_path, ende::fs::in | ende::fs::binary))
        return -1;

    ende::Vector<u32> vertexShaderData(shaderFile.size());
    shaderFile.read({reinterpret_cast<char*>(vertexShaderData.data()), static_cast<u32>(vertexShaderData.size() * sizeof(u32))});

    if (!shaderFile.open("../../res/shaders/triangle.frag.spv"_path, ende::fs::in | ende::fs::binary))
        return -2;
    ende::Vector<u32> fragmentShaderData(shaderFile.size());
    shaderFile.read({reinterpret_cast<char*>(fragmentShaderData.data()), static_cast<u32>(fragmentShaderData.size() * sizeof(u32))});


    ShaderProgram program(driver._context._device);
    if (!program.addStage(vertexShaderData, VK_SHADER_STAGE_VERTEX_BIT))
        return -10;
    if (!program.addStage(fragmentShaderData, VK_SHADER_STAGE_FRAGMENT_BIT))
        return -11;

    VkDescriptorSetLayoutBinding uboBinding{};
    uboBinding.binding = 0;
    uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboBinding.descriptorCount = 1;
    uboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    uboBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uboBinding;

    VkDescriptorSetLayout descriptorSetLayout;
    vkCreateDescriptorSetLayout(driver._context._device, &layoutInfo, nullptr, &descriptorSetLayout);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;

    VkPipelineLayout pipelineLayout;
    VkResult result = vkCreatePipelineLayout(driver._context._device, &pipelineLayoutInfo, nullptr, &pipelineLayout);

    program._setLayout[0] = descriptorSetLayout;
    program._layout = pipelineLayout;


    //vertex array
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = 5 * sizeof(f32);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attributes[2] = {
            { .location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = 0 },
            { .location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = sizeof(f32) * 2 }
    };

    //renderpass
    VkAttachmentDescription colourAttachment{};
    colourAttachment.format = driver._swapchain.format();
    colourAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colourAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colourAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colourAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colourAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colourAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colourAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colourAttachmentRef{};
    colourAttachmentRef.attachment = 0;
    colourAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colourAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPass renderPass;
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colourAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;
    if (vkCreateRenderPass(driver._context._device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
        return -15;


    ende::Vector<VkFramebuffer> swapchainFramebuffers;
    swapchainFramebuffers.resize(driver._swapchain.size());

    for (u32 i = 0; i < swapchainFramebuffers.size(); i++) {
        VkImageView attachments[] = { driver._swapchain.view(i) };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = driver._swapchain.extent().width;
        framebufferInfo.height = driver._swapchain.extent().height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(driver._context._device, &framebufferInfo, nullptr, &swapchainFramebuffers[i]) != VK_SUCCESS)
            return -34;
    }


    VkQueue graphicsQueue = driver._context.getQueue(VK_QUEUE_GRAPHICS_BIT);


    ende::time::StopWatch frameClock;
    frameClock.start();
    f32 dt = 1.f / 60.f;
    f32 fps = 0.f;
    ende::time::Duration frameTime;

    CommandBuffer* buffer = nullptr;


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

        driver._swapchain.wait();

        if (renderImGui) {
            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplSDL2_NewFrame(window);
            ImGui::NewFrame();
            ImGui::ShowDemoWindow();

            ImGui::Begin("Stats");

            ImGui::Text("FrameTime: %f", frameTime.microseconds() / 1000.f);
            ImGui::Text("FPS: %f", 1000.f / (frameTime.microseconds() / 1000.f));
            ImGui::Text("Pipelines: %ld", buffer ? buffer->_pipelines.size() : 0);
            ImGui::Text("Descriptors: %ld", buffer ? buffer->_descriptorSets.size() : 0);

            ImGui::End();


            ImGui::Render();
        }

        auto frame = driver._swapchain.nextImage();

        buffer = driver._context._commands->get();
        {
            u32 i = frame.index;

            VkRenderPassBeginInfo renderPassBeginInfo{};
            renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassBeginInfo.renderPass = renderPass;
            renderPassBeginInfo.framebuffer = swapchainFramebuffers[i];
            renderPassBeginInfo.renderArea.offset = {0, 0};
            renderPassBeginInfo.renderArea.extent = driver._swapchain.extent();

            VkClearValue clearColour = {{{0.f, 0.f, 0.f, 1.f}}};
            renderPassBeginInfo.clearValueCount = 1;
            renderPassBeginInfo.pClearValues = &clearColour;

            vkCmdBeginRenderPass(buffer->buffer(), &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);


            buffer->bindProgram(program);
            buffer->bindVertexArray({&binding, 1}, {attributes, 2});
            buffer->bindRenderPass(renderPass);
            buffer->bindRasterState({});
            buffer->bindPipeline();

            buffer->bindBuffer(0, 0, uniformBuffer, 0, sizeof(ende::math::Vec3f));
            buffer->bindDescriptors();

            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(buffer->buffer(), 0, 1, &vertexBuffer, offsets);

            vkCmdDraw(buffer->buffer(), 3, 1, 0, 0);

//            driver.draw({}, {vertexBuffer, 3});

            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), buffer->buffer());

            vkCmdEndRenderPass(buffer->buffer());


            buffer->submit(frame.imageAquired, driver._swapchain.fence());
            driver._context._commands->flush();
        }

        driver._swapchain.present(frame, buffer->signal());


        frameTime = frameClock.reset();
        dt = frameTime.milliseconds() / 1000.f;
    }

    driver._swapchain.wait();


    vkDestroyBuffer(driver._context._device, uniformBuffer, nullptr);
    vkFreeMemory(driver._context._device, uniformBufferMemory, nullptr);

    vkDestroyBuffer(driver._context._device, vertexBuffer, nullptr);
    vkFreeMemory(driver._context._device, vertexBufferMemory, nullptr);

    vkDestroyCommandPool(driver._context._device, commandPool, nullptr);

    for (auto& framebuffer : swapchainFramebuffers)
        vkDestroyFramebuffer(driver._context._device, framebuffer, nullptr);

    vkDestroyRenderPass(driver._context._device, renderPass, nullptr);

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    vkDestroyDescriptorPool(driver._context._device, ImGuiDescriptorPool, nullptr);
    vkDestroyRenderPass(driver._context._device, ImGuiRenderPass, nullptr);

    SDL_DestroyWindow(window);

    return 0;
}