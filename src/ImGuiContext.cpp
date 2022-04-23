#include "Cala/ImGuiContext.h"


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


ImGuiContext::ImGuiContext(cala::backend::vulkan::Driver &driver, SDL_Window* window)
    : _device(driver.context().device()),
    _window(window),
    _renderPass(nullptr),
    _descriptorPool(VK_NULL_HANDLE),
    _commandPool(VK_NULL_HANDLE)
{
    std::array<cala::backend::vulkan::RenderPass::Attachment, 2> attachments = {
            cala::backend::vulkan::RenderPass::Attachment{
                    driver.swapchain().format(),
                    VK_SAMPLE_COUNT_1_BIT,
                    VK_ATTACHMENT_LOAD_OP_CLEAR,
                    VK_ATTACHMENT_STORE_OP_STORE,
                    VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                    VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            },
            cala::backend::vulkan::RenderPass::Attachment{
                    driver.context().depthFormat(),
                    VK_SAMPLE_COUNT_1_BIT,
                    VK_ATTACHMENT_LOAD_OP_CLEAR,
                    VK_ATTACHMENT_STORE_OP_STORE,
                    VK_ATTACHMENT_LOAD_OP_CLEAR,
                    VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
            }
    };
    _renderPass = new cala::backend::vulkan::RenderPass(driver, attachments);

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
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 11;
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = driver.swapchain().size();

    vkCreateDescriptorPool(driver.context().device(), &poolInfo, nullptr, &_descriptorPool);

    VkCommandPoolCreateInfo commandPoolInfo{};
    commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolInfo.queueFamilyIndex = driver.context().queueIndex(cala::backend::QueueType::GRAPHICS);
    commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    vkCreateCommandPool(driver.context().device(), &commandPoolInfo, nullptr, &_commandPool);

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
    initInfo.Instance = driver.context().instance();
    initInfo.PhysicalDevice = driver.context().physicalDevice();
    initInfo.Device = driver.context().device();
    initInfo.QueueFamily = driver.context().queueIndex(cala::backend::QueueType::GRAPHICS);
    initInfo.Queue = driver.context().getQueue(cala::backend::QueueType::GRAPHICS);
    initInfo.PipelineCache = VK_NULL_HANDLE;
    initInfo.DescriptorPool = _descriptorPool;
    initInfo.Allocator = nullptr;
    initInfo.MinImageCount = driver.swapchain().size();
    initInfo.ImageCount = driver.swapchain().size();
    initInfo.CheckVkResultFn = nullptr;
    ImGui_ImplVulkan_Init(&initInfo, _renderPass->renderPass());

    VkCommandBuffer a = beginSingleTimeCommands(driver.context().device(), _commandPool);
    ImGui_ImplVulkan_CreateFontsTexture(a);
    endSingleTimeCommands(a, driver.context().device(), driver.context().getQueue(cala::backend::QueueType::GRAPHICS), _commandPool);
}

ImGuiContext::~ImGuiContext() {
    ImGui_ImplVulkan_DestroyFontUploadObjects();
    vkDestroyCommandPool(_device, _commandPool, nullptr);
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();
//    ImGui::DestroyContext();
    vkDestroyDescriptorPool(_device, _descriptorPool, nullptr);
    delete _renderPass;
}


void ImGuiContext::newFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame(_window);
    ImGui::NewFrame();
}

void ImGuiContext::render(cala::backend::vulkan::CommandBuffer &buffer) {
//    buffer.begin(*_renderPass);TODO: use own renderpass/framebuffers
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), buffer.buffer());
}