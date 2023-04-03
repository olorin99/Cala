#include "Cala/backend/vulkan/CommandBuffer.h"
#include <Cala/backend/vulkan/primitives.h>
#include <Cala/backend/vulkan/Device.h>
#include <Ende/profile/profile.h>

#include <Ende/log/log.h>

cala::backend::vulkan::CommandBuffer::CommandBuffer(Device& device, VkQueue queue, VkCommandBuffer buffer)
    : _device(&device),
    _buffer(buffer),
    _signal(VK_NULL_HANDLE),
    _queue(queue),
    _active(false),
    _indexBuffer{},
    _currentPipeline(VK_NULL_HANDLE),
    _currentSets{VK_NULL_HANDLE},
    _drawCallCount(0)
{
    memset(&_pipelineKey, 0, sizeof(PipelineKey));
    _pipelineKey.viewPort.maxDepth = 1.f;
}

cala::backend::vulkan::CommandBuffer::~CommandBuffer() {
    vkDestroySemaphore(_device->context().device(), _signal, nullptr);
}

cala::backend::vulkan::CommandBuffer::CommandBuffer(CommandBuffer &&rhs) noexcept
    : _device(nullptr),
    _buffer(VK_NULL_HANDLE),
    _signal(VK_NULL_HANDLE),
    _queue(VK_NULL_HANDLE),
    _active(false),
    _indexBuffer{},
    _boundInterface(nullptr),
    _pipelineKey(),
    _currentPipeline(VK_NULL_HANDLE)
{
    std::swap(_device, rhs._device);
    std::swap(_buffer, rhs._buffer);
    std::swap(_signal, rhs._signal);
    std::swap(_queue, rhs._queue);
    std::swap(_active, rhs._active);
    std::swap(_indexBuffer, rhs._indexBuffer);
    std::swap(_boundInterface, rhs._boundInterface);
    std::swap(_pipelineKey, rhs._pipelineKey);
    std::swap(_currentPipeline, rhs._currentPipeline);
    for (u32 i = 0; i < MAX_SET_COUNT; i++) {
        std::swap(_descriptorKey[i], rhs._descriptorKey[i]);
        std::swap(_currentSets[i], rhs._currentSets[i]);
    }
    std::swap(_drawCallCount, rhs._drawCallCount);
}

cala::backend::vulkan::CommandBuffer &cala::backend::vulkan::CommandBuffer::operator=(CommandBuffer &&rhs) noexcept {
    std::swap(_device, rhs._device);
    std::swap(_buffer, rhs._buffer);
    std::swap(_signal, rhs._signal);
    std::swap(_queue, rhs._queue);
    std::swap(_active, rhs._active);
    std::swap(_indexBuffer, rhs._indexBuffer);
    std::swap(_boundInterface, rhs._boundInterface);
    std::swap(_pipelineKey, rhs._pipelineKey);
    std::swap(_currentPipeline, rhs._currentPipeline);
    for (u32 i = 0; i < MAX_SET_COUNT; i++) {
        std::swap(_descriptorKey[i], rhs._descriptorKey[i]);
        std::swap(_currentSets[i], rhs._currentSets[i]);
    }
    std::swap(_drawCallCount, rhs._drawCallCount);
    return *this;
}

bool cala::backend::vulkan::CommandBuffer::begin() {
//    vkResetCommandBuffer(_buffer, 0);
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    _active = vkBeginCommandBuffer(_buffer, &beginInfo) == VK_SUCCESS;
    vkDestroySemaphore(_device->context().device(), _signal, nullptr);
    VkSemaphoreCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    vkCreateSemaphore(_device->context().device(), &createInfo, nullptr, &_signal);
    _drawCallCount = 0;
    return _active;
}

bool cala::backend::vulkan::CommandBuffer::end() {
    if (!_active)
        return false;

#ifndef NDEBUG
    for (auto& label : _debugLabels)
        popDebugLabel();
#endif

    bool res = vkEndCommandBuffer(_buffer) == VK_SUCCESS;
    if (res) {
        _active = false;
        _currentPipeline = VK_NULL_HANDLE;
//        _pipelineKey = {};
//        for (u32 i = 0; i < SET_COUNT; i++)
//            _descriptorKey[i] = {};
//        _currentPipeline = VK_NULL_HANDLE;
    }
    return res;
}

void cala::backend::vulkan::CommandBuffer::begin(RenderPass &renderPass, VkFramebuffer framebuffer, std::pair<u32, u32> extent) {

    VkRenderPassBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.renderPass = renderPass.renderPass();
    beginInfo.framebuffer = framebuffer;
    beginInfo.renderArea.offset = {0, 0};
    beginInfo.renderArea.extent = {extent.first, extent.second};

    auto clearValues = renderPass.clearValues();
    beginInfo.clearValueCount = clearValues.size();
    beginInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(_buffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void cala::backend::vulkan::CommandBuffer::begin(Framebuffer &framebuffer) {
    begin(framebuffer.renderPass(), framebuffer.framebuffer(), framebuffer.extent());
    _pipelineKey.framebuffer = &framebuffer;
}

void cala::backend::vulkan::CommandBuffer::end(RenderPass &renderPass) {
    vkCmdEndRenderPass(_buffer);
}

void cala::backend::vulkan::CommandBuffer::end(Framebuffer &framebuffer) {
    end(framebuffer.renderPass());
}


void cala::backend::vulkan::CommandBuffer::bindProgram(ProgramHandle program) {
    assert(program);
    if (_pipelineKey.layout != program->_layout)
        _pipelineKey.layout = program->_layout;

    for (u32 i = 0; i < MAX_SET_COUNT; i++) {
        if (_descriptorKey[i].setLayout != program->_setLayout[i])
            _descriptorKey[i].setLayout = program->_setLayout[i];
    }
    for (u32 i = 0; i < program->_stages.size(); i++) {
            _pipelineKey.shaders[i] = program->_stages[i];
    }
    _pipelineKey.shaderCount = program->_stages.size();
    _boundInterface = &program->_interface;
    _pipelineKey.compute = program->stagePresent(ShaderStage::COMPUTE);
}

void cala::backend::vulkan::CommandBuffer::bindAttributes(ende::Span<Attribute> attributes) {
    assert(attributes.size() <= MAX_VERTEX_INPUT_ATTRIBUTES && "number of supplied vertex input attributes is greater than valid count");
    VkVertexInputAttributeDescription attributeDescriptions[MAX_VERTEX_INPUT_ATTRIBUTES]{};
    u32 attribIndex = 0;
    u32 offset = 0;
    for (u32 i = 0; i < attributes.size() && attribIndex < MAX_VERTEX_INPUT_ATTRIBUTES; i++) {
        assert(attribIndex < MAX_VERTEX_INPUT_ATTRIBUTES);
        attributeDescriptions[attribIndex].location = attributes[i].location;
        attributeDescriptions[attribIndex].binding = attributes[i].binding;
        attributeDescriptions[attribIndex].offset = offset;
        switch (attributes[i].type) {
            case AttribType::Vec2f:
                attributeDescriptions[attribIndex].format = VK_FORMAT_R32G32_SFLOAT;
                break;
            case AttribType::Vec3f:
                attributeDescriptions[attribIndex].format = VK_FORMAT_R32G32B32_SFLOAT;
                break;
            case AttribType::Vec4f:
                attributeDescriptions[attribIndex].format = VK_FORMAT_R32G32B32A32_SFLOAT;
                break;
            case AttribType::Mat4f:
                if (i + 4 >= 10) return;
                attributeDescriptions[attribIndex].format = VK_FORMAT_R32G32B32A32_SFLOAT;
                attributeDescriptions[++attribIndex].format = VK_FORMAT_R32G32B32A32_SFLOAT;
                attributeDescriptions[attribIndex].location = attributes[i].location + 1;
                attributeDescriptions[attribIndex].binding = attributes[i].binding;
                attributeDescriptions[attribIndex].offset = offset + (4 * sizeof(f32));
                attributeDescriptions[++attribIndex].format = VK_FORMAT_R32G32B32A32_SFLOAT;
                attributeDescriptions[attribIndex].location = attributes[i].location + 2;
                attributeDescriptions[attribIndex].binding = attributes[i].binding;
                attributeDescriptions[attribIndex].offset = offset + (4 * sizeof(f32)) * 2;
                attributeDescriptions[++attribIndex].format = VK_FORMAT_R32G32B32A32_SFLOAT;
                attributeDescriptions[attribIndex].location = attributes[i].location + 3;
                attributeDescriptions[attribIndex].binding = attributes[i].binding;
                attributeDescriptions[attribIndex].offset = offset + (4 * sizeof(f32)) * 3;
                break;
        }
        offset += static_cast<std::underlying_type<AttribType>::type>(attributes[i].type) * sizeof(f32);
        attribIndex++;
    }
    bindAttributeDescriptions({&attributeDescriptions[0], attribIndex});
}

void cala::backend::vulkan::CommandBuffer::bindBindings(ende::Span<VkVertexInputBindingDescription> bindings) {
    assert(bindings.size() <= MAX_VERTEX_INPUT_BINDINGS && "number of supplied vertex input bindings is greater than valid count");
    memset(_pipelineKey.bindings, 0, sizeof(_pipelineKey.bindings));
    memcpy(_pipelineKey.bindings, bindings.data(), bindings.size() * sizeof(VkVertexInputBindingDescription));
    _pipelineKey.bindingCount = bindings.size();
}

void cala::backend::vulkan::CommandBuffer::bindAttributeDescriptions(ende::Span<VkVertexInputAttributeDescription> attributes) {
    memset(_pipelineKey.attributes, 0, sizeof(_pipelineKey.attributes));
    memcpy(_pipelineKey.attributes, attributes.data(), attributes.size() * sizeof(VkVertexInputAttributeDescription));
    _pipelineKey.attributeCount = attributes.size();
}

void cala::backend::vulkan::CommandBuffer::bindVertexArray(ende::Span<VkVertexInputBindingDescription> bindings,
                                                           ende::Span<VkVertexInputAttributeDescription> attributes) {
    bindBindings(bindings);
    bindAttributeDescriptions(attributes);
}

void cala::backend::vulkan::CommandBuffer::bindViewPort(const ViewPort &viewport) {
    if (_pipelineKey.viewPort.x != viewport.x ||
        _pipelineKey.viewPort.y != viewport.y ||
        _pipelineKey.viewPort.width != viewport.width ||
        _pipelineKey.viewPort.height != viewport.height ||
        _pipelineKey.viewPort.minDepth != viewport.minDepth ||
        _pipelineKey.viewPort.maxDepth != viewport.maxDepth) {
        _pipelineKey.viewPort = viewport;
    }
}

void cala::backend::vulkan::CommandBuffer::bindRasterState(RasterState state) {
    if (_pipelineKey.raster.cullMode != state.cullMode ||
        _pipelineKey.raster.frontFace != state.frontFace ||
        _pipelineKey.raster.polygonMode != state.polygonMode ||
        _pipelineKey.raster.lineWidth != state.lineWidth ||
        _pipelineKey.raster.depthClamp != state.depthClamp ||
        _pipelineKey.raster.rasterDiscard != state.rasterDiscard ||
        _pipelineKey.raster.depthBias != state.depthBias)
    {
        _pipelineKey.raster.cullMode = state.cullMode;
        _pipelineKey.raster.frontFace = state.frontFace;
        _pipelineKey.raster.polygonMode = state.polygonMode;
        _pipelineKey.raster.lineWidth = state.lineWidth;
        _pipelineKey.raster.depthClamp = state.depthClamp;
        _pipelineKey.raster.rasterDiscard = state.rasterDiscard;
        _pipelineKey.raster.depthBias = state.depthBias;
    }
}

void cala::backend::vulkan::CommandBuffer::bindDepthState(DepthState state) {
    if (ende::util::MurmurHash<DepthState>()(_pipelineKey.depth) != ende::util::MurmurHash<DepthState>()(state)) {
        memset(&_pipelineKey.depth, 0, sizeof(DepthState));
        _pipelineKey.depth.test = state.test;
        _pipelineKey.depth.write = state.write;
        _pipelineKey.depth.compareOp = state.compareOp;
    }
}

void cala::backend::vulkan::CommandBuffer::bindBlendState(BlendState state) {
    if (ende::util::MurmurHash<BlendState>()(_pipelineKey.blend) != ende::util::MurmurHash<BlendState>()(state)) {
        _pipelineKey.blend.blend = state.blend;
        _pipelineKey.blend.srcFactor = state.srcFactor;
        _pipelineKey.blend.dstFactor = state.dstFactor;
    }
}

void cala::backend::vulkan::CommandBuffer::bindPipeline() {
    //TODO: add dirty check so dont have to search to check if bound
    if (auto pipeline = _device->getPipeline(_pipelineKey); pipeline != _currentPipeline && pipeline != VK_NULL_HANDLE) {
        vkCmdBindPipeline(_buffer, _pipelineKey.compute ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        _currentPipeline = pipeline;
    }
}

void cala::backend::vulkan::CommandBuffer::bindBuffer(u32 set, u32 binding, BufferHandle buffer, u32 offset, u32 range, bool storage) {
    assert(buffer);
    assert(set < MAX_SET_COUNT && "set is greater than valid number of descriptor sets");
    _descriptorKey[set].buffers[binding] = { buffer, offset, range == 0 ? (buffer->size() - offset) : range , storage };
//    bindBuffer(set, slot, buffer.buffer(), offset, range == 0 ? buffer.size() : range);
}

void cala::backend::vulkan::CommandBuffer::bindBuffer(u32 set, u32 binding, BufferHandle buffer, bool storage) {
    assert(buffer);
    assert(set < MAX_SET_COUNT && "set is greater than valid number of descriptor sets");
    _descriptorKey[set].buffers[binding] = { buffer, 0, buffer->size(), storage };
}

void cala::backend::vulkan::CommandBuffer::bindImage(u32 set, u32 binding, Image::View& image, Sampler& sampler, bool storage) {
    _descriptorKey[set].images[binding] = { image.parent(), image.view, sampler.sampler(), storage };
}

void cala::backend::vulkan::CommandBuffer::pushConstants(ende::Span<const void> data, u32 offset) {
    assert(data.size() <= 128);
    vkCmdPushConstants(_buffer, _pipelineKey.layout, getShaderStage(_boundInterface->pushConstants.stage), offset, data.size(), data.data());
}

void cala::backend::vulkan::CommandBuffer::bindDescriptors() {

    // find descriptors with key
    for (u32 i = 0; i < MAX_SET_COUNT; i++) {
        if (_boundInterface->setPresent(i) && _device->getBindlessIndex() == i)
            _currentSets[i] = _device->bindlessSet();
        else {
            auto descriptor = _device->getDescriptorSet(_descriptorKey[i]);
            _currentSets[i] = descriptor;
        }
    }
    // bind descriptor
    vkCmdBindDescriptorSets(_buffer, _pipelineKey.compute ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS, _pipelineKey.layout, 0, MAX_SET_COUNT, _currentSets, 0, nullptr);

}

void cala::backend::vulkan::CommandBuffer::clearDescriptors() {
    for (auto& setKey : _descriptorKey)
        setKey = DescriptorKey{};
}


void cala::backend::vulkan::CommandBuffer::bindVertexBuffer(u32 first, BufferHandle buffer, u32 offset) {
    assert(buffer);
    VkDeviceSize offsets = offset;
    VkBuffer b = buffer->buffer();
    vkCmdBindVertexBuffers(_buffer, first, 1, &b, &offsets);
    _indexBuffer = {};
}

void cala::backend::vulkan::CommandBuffer::bindVertexBuffers(u32 first, ende::Span<VkBuffer> buffers, ende::Span<VkDeviceSize> offsets) {
    assert(buffers.size() == offsets.size());
    vkCmdBindVertexBuffers(_buffer, first, buffers.size(), buffers.data(), offsets.data());
    _indexBuffer = {};
}

//void cala::backend::vulkan::CommandBuffer::bindVertexBuffer(u32 first, Buffer &buffer, u32 offset) {
//    bindVertexBuffer(first, buffer.buffer(), offset);
//}

void cala::backend::vulkan::CommandBuffer::bindIndexBuffer(BufferHandle buffer, u32 offset) {
    assert(buffer);
    vkCmdBindIndexBuffer(_buffer, buffer->buffer(), offset, VK_INDEX_TYPE_UINT32);
    _indexBuffer = buffer;
}


void cala::backend::vulkan::CommandBuffer::draw(u32 count, u32 instanceCount, u32 first, u32 firstInstance, bool indexed) {
    if (_pipelineKey.compute) throw std::runtime_error("Trying to draw when compute pipeline is bound");
    if (indexed && _indexBuffer)
        vkCmdDrawIndexed(_buffer, count, instanceCount, first, 0, firstInstance);
    else
        vkCmdDraw(_buffer, count, instanceCount, first, firstInstance);
    ++_drawCallCount;
}

void cala::backend::vulkan::CommandBuffer::drawIndirect(BufferHandle buffer, u32 offset, u32 drawCount, u32 stride) {
    if (_pipelineKey.compute) throw std::runtime_error("Trying to draw when compute pipeline is bound");

    if (stride == 0)
        stride = sizeof(u32) * 4;
    vkCmdDrawIndirect(_buffer, buffer->buffer(), offset, drawCount, stride);
    ++_drawCallCount;
}

void cala::backend::vulkan::CommandBuffer::drawIndirectCount(BufferHandle buffer, u32 bufferOffset, BufferHandle countBuffer, u32 countOffset, u32 maxDrawCount, u32 stride) {
    if (_pipelineKey.compute) throw std::runtime_error("Trying to draw when compute pipeline is bound");
    assert(countBuffer->size() > countOffset);

    if (_indexBuffer) {
        if (stride == 0)
            stride = sizeof(VkDrawIndexedIndirectCommand);
        vkCmdDrawIndexedIndirectCount(_buffer, buffer->buffer(), bufferOffset, countBuffer->buffer(), countOffset, maxDrawCount, stride);
    }
    else {
        if (stride == 0)
            stride = sizeof(VkDrawIndirectCommand);
        vkCmdDrawIndirectCount(_buffer, buffer->buffer(), bufferOffset, countBuffer->buffer(), countOffset, maxDrawCount, stride);
    }
    ++_drawCallCount;
}

void cala::backend::vulkan::CommandBuffer::dispatchCompute(u32 x, u32 y, u32 z) {
    if (!_pipelineKey.compute) throw std::runtime_error("Trying to dispatch compute when graphics pipeline is bound");
    vkCmdDispatch(_buffer, x, y, z);
}

void cala::backend::vulkan::CommandBuffer::pipelineBarrier(PipelineStage srcStage, PipelineStage dstStage, VkDependencyFlags dependencyFlags, ende::Span<VkBufferMemoryBarrier> bufferBarriers, ende::Span<VkImageMemoryBarrier> imageBarriers) {
    vkCmdPipelineBarrier(_buffer, getPipelineStage(srcStage), getPipelineStage(dstStage), dependencyFlags, 0, nullptr, bufferBarriers.size(), bufferBarriers.data(), imageBarriers.size(), imageBarriers.data());
}

void cala::backend::vulkan::CommandBuffer::pipelineBarrier(PipelineStage srcStage, PipelineStage dstStage, ende::Span<Image::Barrier> imageBarriers) {
    VkImageMemoryBarrier barriers[imageBarriers.size()];
    for (u32 i = 0; i < imageBarriers.size(); i++) {
        auto& b = barriers[i];
        auto& barrier = imageBarriers[i];
        b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        b.image = barrier.image->image();
        b.subresourceRange = barrier.subresourceRange;
        b.srcAccessMask = getAccessFlags(barrier.srcAccess);
        b.dstAccessMask = getAccessFlags(barrier.dstAccess);
        b.oldLayout = getImageLayout(barrier.srcLayout);
        b.newLayout = getImageLayout(barrier.dstLayout);
        b.srcQueueFamilyIndex = -1;
        b.dstQueueFamilyIndex = -1;
        b.pNext = nullptr;
        barrier.image->setLayout(getImageLayout(barrier.dstLayout));
    }
    vkCmdPipelineBarrier(_buffer, getPipelineStage(srcStage), getPipelineStage(dstStage), 0, 0, nullptr, 0, nullptr, imageBarriers.size(), barriers);
}

void cala::backend::vulkan::CommandBuffer::pipelineBarrier(PipelineStage srcStage, PipelineStage dstStage, ende::Span<Buffer::Barrier> bufferBarriers) {
    VkBufferMemoryBarrier barriers[bufferBarriers.size()];
    for (u32 i = 0; i < bufferBarriers.size(); i++) {
        auto& b = barriers[i];
        auto& barrier = bufferBarriers[i];
        b.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        b.buffer = barrier.buffer->buffer();
        b.size = barrier.buffer->size();
        b.offset = 0;
        b.srcAccessMask = getAccessFlags(barrier.srcAccess);
        b.dstAccessMask = getAccessFlags(barrier.dstAccess);
        b.srcQueueFamilyIndex = -1;
        b.dstQueueFamilyIndex = -1;
        b.pNext = nullptr;
    }
    vkCmdPipelineBarrier(_buffer, getPipelineStage(srcStage), getPipelineStage(dstStage), 0, 0, nullptr, bufferBarriers.size(), barriers, 0, nullptr);
}

void cala::backend::vulkan::CommandBuffer::pushDebugLabel(std::string_view label, std::array<f32, 4> colour) {
#ifndef NDEBUG
    _debugLabels.push(label);
    _device->context().beginDebugLabel(_buffer, label, colour);
#endif
}

void cala::backend::vulkan::CommandBuffer::popDebugLabel() {
#ifndef NDEBUG
    _device->context().endDebugLabel(_buffer);
    _debugLabels.pop();
#endif
}

void cala::backend::vulkan::CommandBuffer::startPipelineStatistics() {
    vkCmdResetQueryPool(_buffer, _device->context().pipelineStatisticsPool(),0, 6);
    vkCmdBeginQuery(_buffer, _device->context().pipelineStatisticsPool(), 0, 0);
}

void cala::backend::vulkan::CommandBuffer::stopPipelineStatistics() {
    vkCmdEndQuery(_buffer, _device->context().pipelineStatisticsPool(), 0);
}





bool cala::backend::vulkan::CommandBuffer::submit(ende::Span<VkSemaphore> wait, VkFence fence) {
    PROFILE_NAMED("CommandBuffer::Submit");

    end();

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    assert(wait.size() <= 2);
    VkPipelineStageFlags waitDstStageMask[2] = { VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT };
//    VkSemaphore waitSemaphore[wait] = { wait };

//    submitInfo.waitSemaphoreCount = wait == VK_NULL_HANDLE ? 0 : 1;
//    submitInfo.pWaitSemaphores = waitSemaphore;
    submitInfo.pWaitDstStageMask = waitDstStageMask;

    u32 waitCount = wait.size();
    for (auto& semaphore : wait) {
        if (semaphore == VK_NULL_HANDLE)
            waitCount = 0;
    }

    submitInfo.waitSemaphoreCount = waitCount;
    submitInfo.pWaitSemaphores = wait.data();

    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &_signal;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &_buffer;

    vkQueueSubmit(_queue, 1, &submitInfo, fence);
    return true;
}

bool
cala::backend::vulkan::CommandBuffer::PipelineEqual::operator()(const PipelineKey &lhs, const PipelineKey &rhs) const {
    return 0 == memcmp((const void*)&lhs, (const void*)&rhs, sizeof(lhs));
}