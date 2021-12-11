#include "Cala/backend/vulkan/CommandBufferList.h"

cala::backend::vulkan::CommandBufferList::CommandBufferList(VkDevice device, u32 queueIndex)
    : _device(device),
    _pool(VK_NULL_HANDLE),
    _current(nullptr),
    _wait(VK_NULL_HANDLE),
    _renderFinish(VK_NULL_HANDLE)
{

    VkCommandPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    createInfo.queueFamilyIndex = queueIndex;
    createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    vkCreateCommandPool(device, &createInfo, nullptr, &_pool);

//    VkCommandBufferAllocateInfo allocInfo{};
//    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
//    allocInfo.commandPool = _pool;
//    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
//    allocInfo.commandBufferCount = 1;
//
//    VkCommandBuffer buffer;
//    vkAllocateCommandBuffers(_device, &allocInfo, &buffer);

//    VkSemaphore signal;
//    VkSemaphoreCreateInfo semaphoreInfo{};
//    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
//    vkCreateSemaphore(device, &semaphoreInfo, nullptr, &signal);

//    _buffers.push({buffer, true, signal});

    vkGetDeviceQueue(device, queueIndex, 0, &_queue);

//    _buffers.emplace(CommandBuffer(device, _queue, buffer));

}

cala::backend::vulkan::CommandBufferList::~CommandBufferList() {
//    for (auto& buffer : _buffers)
//        vkDestroySemaphore(_device, buffer.signal, nullptr);
    vkDestroyCommandPool(_device, _pool, nullptr);
}


cala::backend::vulkan::CommandBuffer* cala::backend::vulkan::CommandBufferList::get() {
    // if command buffer already started return current
//    if (_current)
//        return _current;

    // find next command buffer that is available
//    for (auto& buffer : _buffers) {
//        if (buffer.free) {
//            _current = &buffer;
//            break;
//        }
//    }
    // if non available create new
    if (!_current) {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = _pool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer buffer;
        vkAllocateCommandBuffers(_device, &allocInfo, &buffer);

        _buffers.emplace(_device, _queue, buffer);
        _current = &_buffers.back();
    }


    // mark as unavailable
//    _current->free = false;

    _current->begin();

    return _current;
}

bool cala::backend::vulkan::CommandBufferList::flush() {
    if (!_current)
        return false;

//    _current->submit(VK_NULL_HANDLE,);


//    vkEndCommandBuffer(_current->buffer);

//    VkPipelineStageFlags waitDstStageMask[2] = { VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT };
//    VkSemaphore waitSemaphores[2] = { VK_NULL_HANDLE, VK_NULL_HANDLE };
//
//    VkSubmitInfo submitInfo{};
//    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
//
//    submitInfo.waitSemaphoreCount = 0;
//    submitInfo.pWaitSemaphores = waitSemaphores;
//    submitInfo.pWaitDstStageMask = waitDstStageMask;
//
//    submitInfo.signalSemaphoreCount = 1;
//    submitInfo.pSignalSemaphores = &_current->signal;
//
//    submitInfo.commandBufferCount = 1;
//    submitInfo.pCommandBuffers = &_current->buffer;
//
//    // wait on last command buffer
//    if (_renderFinish != VK_NULL_HANDLE)
//        waitSemaphores[submitInfo.waitSemaphoreCount++] = _renderFinish;
//
//    // optional wait on external semaphore. most likely swapchain image acquire
//    if (_wait != VK_NULL_HANDLE)
//        waitSemaphores[submitInfo.waitSemaphoreCount++] = _wait;
//
//
//    vkQueueSubmit(_queue, 1, &submitInfo, VK_NULL_HANDLE);
//
//    _renderFinish = _current->signal;
//    _wait = VK_NULL_HANDLE;
//    _current->free = true;
    _current = nullptr;
    return true;
}

void cala::backend::vulkan::CommandBufferList::waitSemaphore(VkSemaphore wait) {
    _wait = wait;
}

VkSemaphore cala::backend::vulkan::CommandBufferList::signal() {
    VkSemaphore tmp = _renderFinish;
    _renderFinish = VK_NULL_HANDLE;
    return tmp;
}