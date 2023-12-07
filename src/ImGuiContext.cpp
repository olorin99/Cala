#include "Cala/ImGuiContext.h"
#include <volk.h>
#include <Cala/vulkan/primitives.h>
#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>
#include <implot/implot.h>


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


ImGuiContext::ImGuiContext(cala::vk::Device &driver, cala::vk::Swapchain* swapchain, SDL_Window* window)
    : _device(driver.context().device()),
    _window(window),
    _renderPass(nullptr),
    _descriptorPool(VK_NULL_HANDLE),
    _commandPool(VK_NULL_HANDLE)
{
    std::array<cala::vk::RenderPass::Attachment, 1> attachments = {
            cala::vk::RenderPass::Attachment{
                    cala::vk::Format::RGBA8_UNORM,
                    VK_SAMPLE_COUNT_1_BIT,
                    cala::vk::LoadOp::CLEAR,
                    cala::vk::StoreOp::STORE,
                    cala::vk::LoadOp::DONT_CARE,
                    cala::vk::StoreOp::DONT_CARE,
                    cala::vk::ImageLayout::UNDEFINED,
                    cala::vk::ImageLayout::PRESENT,
                    cala::vk::ImageLayout::COLOUR_ATTACHMENT
            }
//            cala::backend::vulkan::RenderPass::Attachment{
//                    device.context().depthFormat(),
//                    VK_SAMPLE_COUNT_1_BIT,
//                    VK_ATTACHMENT_LOAD_OP_CLEAR,
//                    VK_ATTACHMENT_STORE_OP_STORE,
//                    VK_ATTACHMENT_LOAD_OP_CLEAR,
//                    VK_ATTACHMENT_STORE_OP_DONT_CARE,
//                    VK_IMAGE_LAYOUT_UNDEFINED,
//                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
//                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
//            }
    };
//    _renderPass = new cala::backend::vulkan::RenderPass(driver, attachments);
    _renderPass = driver.getRenderPass(attachments);

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
    poolInfo.maxSets = 1000;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    vkCreateDescriptorPool(driver.context().device(), &poolInfo, nullptr, &_descriptorPool);

    u32 graphicsIndex = 0;
    driver.context().queueIndex(graphicsIndex, cala::vk::QueueType::GRAPHICS);

    VkCommandPoolCreateInfo commandPoolInfo{};
    commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolInfo.queueFamilyIndex = graphicsIndex;
    commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    vkCreateCommandPool(driver.context().device(), &commandPoolInfo, nullptr, &_commandPool);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::GetStyle().WindowRounding = 0.f;
        ImGui::GetStyle().Colors[ImGuiCol_WindowBg].w = 1.f;
    }

    if (_window)
        ImGui_ImplSDL2_InitForVulkan(window);

    VkInstance inst = driver.context().instance();

    ImGui_ImplVulkan_LoadFunctions([](const char* functionName, void* instance) {
        return vkGetInstanceProcAddr(*(reinterpret_cast<VkInstance*>(instance)), functionName);
    }, &inst);

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = driver.context().instance();
    initInfo.PhysicalDevice = driver.context().physicalDevice();
    initInfo.Device = driver.context().device();
    initInfo.QueueFamily = graphicsIndex;
    initInfo.Queue = driver.context().getQueue(cala::vk::QueueType::GRAPHICS);
    initInfo.PipelineCache = VK_NULL_HANDLE;
    initInfo.DescriptorPool = _descriptorPool;
    initInfo.Allocator = nullptr;
    initInfo.MinImageCount = swapchain->size();
    initInfo.ImageCount = swapchain->size();
    initInfo.CheckVkResultFn = nullptr;
    ImGui_ImplVulkan_Init(&initInfo, _renderPass->renderPass());

    VkCommandBuffer a = beginSingleTimeCommands(driver.context().device(), _commandPool);
    ImGui_ImplVulkan_CreateFontsTexture(a);
    endSingleTimeCommands(a, driver.context().device(), driver.context().getQueue(cala::vk::QueueType::GRAPHICS), _commandPool);
}

ImGuiContext::~ImGuiContext() {
    for (auto& set : _descriptorDestroyQueue)
        ImGui_ImplVulkan_RemoveTexture(set.second);
    ImGui_ImplVulkan_DestroyFontUploadObjects();
    vkDestroyCommandPool(_device, _commandPool, nullptr);
    ImGui_ImplVulkan_Shutdown();
    if (_window)
        ImGui_ImplSDL2_Shutdown();
    ImPlot::DestroyContext();
//    ImGui::DestroyContext();
    vkDestroyDescriptorPool(_device, _descriptorPool, nullptr);
//    delete _renderPass;
}


void ImGuiContext::newFrame() {
    if (_window) {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
    }
}

void ImGuiContext::processEvent(void *event) {
    if (_window)
        ImGui_ImplSDL2_ProcessEvent((SDL_Event*)event);
}

void ImGuiContext::render(cala::vk::CommandHandle buffer) {
    if (_window) {
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), buffer->buffer());

        for (auto it = _descriptorDestroyQueue.begin(); it != _descriptorDestroyQueue.end(); it++) {
            auto& frame = it->first;
            auto& set = it->second;
            if (frame < 0) {
                ImGui_ImplVulkan_RemoveTexture(set);
                _descriptorDestroyQueue.erase(it--);
            } else
                frame--;
        }
    }
}

void ImGuiContext::destroySet(VkDescriptorSet set) {
    _descriptorDestroyQueue.push_back(std::make_pair(5, set));
}