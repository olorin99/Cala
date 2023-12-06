#include "Cala/backend/vulkan/CommandBuffer.h"
#include <Cala/backend/vulkan/primitives.h>
#include <Cala/backend/vulkan/Device.h>
#include <Ende/profile/profile.h>
#include <Cala/backend/vulkan/ShaderModule.h>

cala::backend::vulkan::CommandBuffer::CommandBuffer(Device& device, VkQueue queue, VkCommandBuffer buffer)
    : _device(&device),
    _buffer(buffer),
    _queue(queue),
    _active(false),
    _indexBuffer{},
    _currentPipeline(VK_NULL_HANDLE),
    _currentSets{VK_NULL_HANDLE},
    _drawCallCount(0),
    _pipelineDirty(true),
    _descriptorDirty(true)
{
    memset(&_pipelineKey, 0, sizeof(PipelineKey));
    _pipelineKey.viewPort.maxDepth = 1.f;
}

cala::backend::vulkan::CommandBuffer::~CommandBuffer() {}

cala::backend::vulkan::CommandBuffer::CommandBuffer(CommandBuffer &&rhs) noexcept
    : _device(nullptr),
    _buffer(VK_NULL_HANDLE),
    _queue(VK_NULL_HANDLE),
    _active(false),
    _indexBuffer{},
    _boundInterface(nullptr),
    _pipelineKey(),
    _currentPipeline(VK_NULL_HANDLE)
{
    std::swap(_device, rhs._device);
    std::swap(_buffer, rhs._buffer);
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
    std::swap(_pipelineDirty, rhs._pipelineDirty);
    std::swap(_descriptorDirty, rhs._descriptorDirty);
}

cala::backend::vulkan::CommandBuffer &cala::backend::vulkan::CommandBuffer::operator=(CommandBuffer &&rhs) noexcept {
    std::swap(_device, rhs._device);
    std::swap(_buffer, rhs._buffer);
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
    std::swap(_pipelineDirty, rhs._pipelineDirty);
    std::swap(_descriptorDirty, rhs._descriptorDirty);
    return *this;
}

bool cala::backend::vulkan::CommandBuffer::begin() {
//    vkResetCommandBuffer(_buffer, 0);
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    _active = vkBeginCommandBuffer(_buffer, &beginInfo) == VK_SUCCESS;
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


void cala::backend::vulkan::CommandBuffer::bindProgram(const ShaderProgram& program) {
    if (_pipelineKey.layout != program.layout())
        _pipelineKey.layout = program.layout();

    for (u32 i = 0; i < MAX_SET_COUNT; i++) {
        if (_descriptorKey[i].setLayout != program.setLayout(i))
            _descriptorKey[i].setLayout = program.setLayout(i);
    }
    for (u32 i = 0; i < program._modules.size(); i++) {
        auto& module = program._modules[i];

        VkPipelineShaderStageCreateInfo stageCreateInfo{};
        stageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageCreateInfo.stage = static_cast<VkShaderStageFlagBits>(module->stage());
        stageCreateInfo.module = module->module();
        stageCreateInfo.pName = "main";
        _pipelineKey.shaders[i] = stageCreateInfo;
    }
    _pipelineKey.shaderCount = program._modules.size();
    _boundInterface = &program.interface();
    _pipelineKey.compute = program.stagePresent(ShaderStage::COMPUTE);
    _pipelineDirty = true;
}

void cala::backend::vulkan::CommandBuffer::bindAttributes(std::span<Attribute> attributes) {
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
    _pipelineDirty = true;
}

void cala::backend::vulkan::CommandBuffer::bindBindings(std::span<VkVertexInputBindingDescription> bindings) {
    assert(bindings.size() <= MAX_VERTEX_INPUT_BINDINGS && "number of supplied vertex input bindings is greater than valid count");
    memset(_pipelineKey.bindings, 0, sizeof(_pipelineKey.bindings));
    memcpy(_pipelineKey.bindings, bindings.data(), bindings.size() * sizeof(VkVertexInputBindingDescription));
    _pipelineKey.bindingCount = bindings.size();
    _pipelineDirty = true;
}

void cala::backend::vulkan::CommandBuffer::bindAttributeDescriptions(std::span<VkVertexInputAttributeDescription> attributes) {
    memset(_pipelineKey.attributes, 0, sizeof(_pipelineKey.attributes));
    memcpy(_pipelineKey.attributes, attributes.data(), attributes.size() * sizeof(VkVertexInputAttributeDescription));
    _pipelineKey.attributeCount = attributes.size();
    _pipelineDirty = true;
}

void cala::backend::vulkan::CommandBuffer::bindVertexArray(std::span<VkVertexInputBindingDescription> bindings,
                                                           std::span<VkVertexInputAttributeDescription> attributes) {
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
        _pipelineDirty = true;
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
        _pipelineDirty = true;
    }
}

void cala::backend::vulkan::CommandBuffer::bindDepthState(DepthState state) {
    if (ende::util::MurmurHash<DepthState>()(_pipelineKey.depth) != ende::util::MurmurHash<DepthState>()(state)) {
        memset(&_pipelineKey.depth, 0, sizeof(DepthState));
        _pipelineKey.depth.test = state.test;
        _pipelineKey.depth.write = state.write;
        _pipelineKey.depth.compareOp = state.compareOp;
        _pipelineDirty = true;
    }
}

void cala::backend::vulkan::CommandBuffer::bindBlendState(BlendState state) {
    if (ende::util::MurmurHash<BlendState>()(_pipelineKey.blend) != ende::util::MurmurHash<BlendState>()(state)) {
        _pipelineKey.blend.blend = state.blend;
        _pipelineKey.blend.srcFactor = state.srcFactor;
        _pipelineKey.blend.dstFactor = state.dstFactor;
        _pipelineDirty = true;
    }
}

void cala::backend::vulkan::CommandBuffer::bindPipeline() {
    //TODO: add dirty check so dont have to search to check if bound
    if (auto pipeline = _device->getPipeline(_pipelineKey); pipeline.has_value() && *pipeline != _currentPipeline && *pipeline != VK_NULL_HANDLE) {
        vkCmdBindPipeline(_buffer, _pipelineKey.compute ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
        _currentPipeline = *pipeline;
    }
    _pipelineDirty = false;
}

void cala::backend::vulkan::CommandBuffer::bindBuffer(u32 set, u32 binding, BufferHandle buffer, u32 offset, u32 range, bool storage) {
    assert(buffer);
    assert(set < MAX_SET_COUNT && "set is greater than valid number of descriptor sets");
    _descriptorKey[set].buffers[binding] = { &*buffer, offset, range == 0 ? (buffer->size() - offset) : range , storage };
    _descriptorKey[set].type = storage ? ShaderModuleInterface::BindingType::STORAGE_BUFFER : ShaderModuleInterface::BindingType::UNIFORM_BUFFER;
//    bindBuffer(set, slot, buffer.buffer(), offset, range == 0 ? buffer.size() : range);
    _descriptorDirty = true;
}

void cala::backend::vulkan::CommandBuffer::bindBuffer(u32 set, u32 binding, BufferHandle buffer, bool storage) {
    assert(buffer);
    assert(set < MAX_SET_COUNT && "set is greater than valid number of descriptor sets");
    _descriptorKey[set].buffers[binding] = { &*buffer, 0, buffer->size(), storage };
    _descriptorKey[set].type = storage ? ShaderModuleInterface::BindingType::STORAGE_BUFFER : ShaderModuleInterface::BindingType::UNIFORM_BUFFER;
    _descriptorDirty = true;
}

void cala::backend::vulkan::CommandBuffer::bindImage(u32 set, u32 binding, Image::View& image, SamplerHandle sampler) {
    bool isStorage = !sampler;
    _descriptorKey[set].images[binding] = { image.parent(), image.view, sampler ? sampler->sampler() : VK_NULL_HANDLE, isStorage };
    _descriptorKey[set].type = isStorage ? ShaderModuleInterface::BindingType::STORAGE_IMAGE : ShaderModuleInterface::BindingType::SAMPLED_IMAGE;
    _descriptorDirty = true;
}

void cala::backend::vulkan::CommandBuffer::bindImage(u32 set, u32 binding, cala::backend::vulkan::ImageHandle image, cala::backend::vulkan::SamplerHandle sampler) {
    bindImage(set, binding, image->defaultView(), sampler);
}

void cala::backend::vulkan::CommandBuffer::pushConstants(ShaderStage stage, std::span<const u8> data, u32 offset) {
    assert(offset + data.size() <= 128);
    vkCmdPushConstants(_buffer, _pipelineKey.layout, getShaderStage(stage), offset, data.size(), data.data());
}

void cala::backend::vulkan::CommandBuffer::bindDescriptors() {

    // find descriptors with key
    for (u32 i = 0; i < MAX_SET_COUNT; i++) {
        if (_device->getBindlessIndex() == i)
            _currentSets[i] = _device->bindlessSet();
        else {
            auto descriptor = _device->getDescriptorSet(_descriptorKey[i]);
            _currentSets[i] = descriptor;
        }
    }
    // bind descriptor
    vkCmdBindDescriptorSets(_buffer, _pipelineKey.compute ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS, _pipelineKey.layout, 0, MAX_SET_COUNT, _currentSets, 0, nullptr);
    _descriptorDirty = false;
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

void cala::backend::vulkan::CommandBuffer::bindVertexBuffers(u32 first, std::span<VkBuffer> buffers, std::span<VkDeviceSize> offsets) {
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
    assert(!_pipelineDirty);
    assert(!_descriptorDirty);
    if (_pipelineKey.compute) throw std::runtime_error("Trying to draw when compute pipeline is bound");
    if (indexed && _indexBuffer)
        vkCmdDrawIndexed(_buffer, count, instanceCount, first, 0, firstInstance);
    else
        vkCmdDraw(_buffer, count, instanceCount, first, firstInstance);
    ++_drawCallCount;
    writeBufferMarker(PipelineStage::VERTEX_SHADER, "vkCmdDrawIndirectCount::VERTEX");
    writeBufferMarker(PipelineStage::GEOMETRY_SHADER, "vkCmdDrawIndirectCount::GEOMETRY");
    writeBufferMarker(PipelineStage::FRAGMENT_SHADER, "vkCmdDrawIndirectCount::FRAGMENT");
}

void cala::backend::vulkan::CommandBuffer::drawIndirect(BufferHandle buffer, u32 offset, u32 drawCount, u32 stride) {
    assert(!_pipelineDirty);
    assert(!_descriptorDirty);
    if (_pipelineKey.compute) throw std::runtime_error("Trying to draw when compute pipeline is bound");

    if (stride == 0)
        stride = sizeof(u32) * 4;
    vkCmdDrawIndirect(_buffer, buffer->buffer(), offset, drawCount, stride);
    ++_drawCallCount;
    writeBufferMarker(PipelineStage::VERTEX_SHADER, "vkCmdDrawIndirectCount::VERTEX");
    writeBufferMarker(PipelineStage::GEOMETRY_SHADER, "vkCmdDrawIndirectCount::GEOMETRY");
    writeBufferMarker(PipelineStage::FRAGMENT_SHADER, "vkCmdDrawIndirectCount::FRAGMENT");
}

void cala::backend::vulkan::CommandBuffer::drawIndirectCount(BufferHandle buffer, u32 bufferOffset, BufferHandle countBuffer, u32 countOffset, u32 stride) {
    assert(!_pipelineDirty);
    assert(!_descriptorDirty);
    if (_pipelineKey.compute) throw std::runtime_error("Trying to draw when compute pipeline is bound");
    assert(countBuffer->size() > countOffset);

    if (_indexBuffer) {
        if (stride == 0)
            stride = sizeof(VkDrawIndexedIndirectCommand);
        u32 maxDrawCount = (buffer->size() - bufferOffset) / stride;
        vkCmdDrawIndexedIndirectCount(_buffer, buffer->buffer(), bufferOffset, countBuffer->buffer(), countOffset, maxDrawCount, stride);
    }
    else {
        if (stride == 0)
            stride = sizeof(VkDrawIndirectCommand);
        u32 maxDrawCount = (buffer->size() - bufferOffset) / stride;
        vkCmdDrawIndirectCount(_buffer, buffer->buffer(), bufferOffset, countBuffer->buffer(), countOffset, maxDrawCount, stride);
    }
    ++_drawCallCount;
    writeBufferMarker(PipelineStage::VERTEX_SHADER, "vkCmdDrawIndirectCount::VERTEX");
    writeBufferMarker(PipelineStage::GEOMETRY_SHADER, "vkCmdDrawIndirectCount::GEOMETRY");
    writeBufferMarker(PipelineStage::FRAGMENT_SHADER, "vkCmdDrawIndirectCount::FRAGMENT");
}

void cala::backend::vulkan::CommandBuffer::dispatchCompute(u32 x, u32 y, u32 z) {
    assert(!_pipelineDirty);
    assert(!_descriptorDirty);
    if (!_pipelineKey.compute) throw std::runtime_error("Trying to dispatch compute when graphics pipeline is bound");
    vkCmdDispatch(_buffer, x, y, z);
    writeBufferMarker(PipelineStage::COMPUTE_SHADER, "vkCmdDispatch");
}

void cala::backend::vulkan::CommandBuffer::clearImage(cala::backend::vulkan::ImageHandle image, const ende::math::Vec4f& clearValue) {
    VkClearColorValue clearColour = { clearValue.x(), clearValue.y(), clearValue.z(), clearValue.w() };
    VkImageSubresourceRange range{};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = 0;
    range.baseArrayLayer = 0;
    range.levelCount = VK_REMAINING_MIP_LEVELS;
    range.layerCount = VK_REMAINING_ARRAY_LAYERS;

    vkCmdClearColorImage(_buffer, image->image(), getImageLayout(image->layout()), &clearColour, 1, &range);
}

void cala::backend::vulkan::CommandBuffer::clearBuffer(cala::backend::vulkan::BufferHandle buffer, u32 clearValue) {
    vkCmdFillBuffer(_buffer, buffer->buffer(), 0, buffer->size(), clearValue);
}

void cala::backend::vulkan::CommandBuffer::pipelineBarrier(std::span<VkBufferMemoryBarrier2> bufferBarriers, std::span<VkImageMemoryBarrier2> imageBarriers) {
    VkDependencyInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    info.bufferMemoryBarrierCount = bufferBarriers.size();
    info.pBufferMemoryBarriers = bufferBarriers.data();
    info.imageMemoryBarrierCount = imageBarriers.size();
    info.pImageMemoryBarriers = imageBarriers.data();

    vkCmdPipelineBarrier2(_buffer, &info);
}

void cala::backend::vulkan::CommandBuffer::pipelineBarrier(std::span<Image::Barrier> imageBarriers) {
    VkImageMemoryBarrier2 barriers[imageBarriers.size()];
    for (u32 i = 0; i < imageBarriers.size(); i++) {
        auto& b = barriers[i];
        auto& barrier = imageBarriers[i];
        b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        b.image = barrier.image->image();
        b.subresourceRange = barrier.subresourceRange;
        b.srcStageMask = getPipelineStage(barrier.srcStage);
        b.dstStageMask = getPipelineStage(barrier.dstStage);
        b.srcAccessMask = getAccessFlags(barrier.srcAccess);
        b.dstAccessMask = getAccessFlags(barrier.dstAccess);
        b.oldLayout = getImageLayout(barrier.srcLayout);
        b.newLayout = getImageLayout(barrier.dstLayout);
        b.srcQueueFamilyIndex = -1;
        b.dstQueueFamilyIndex = -1;
        b.pNext = nullptr;
        barrier.image->setLayout(getImageLayout(barrier.dstLayout));
    }
    VkDependencyInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    info.imageMemoryBarrierCount = imageBarriers.size();
    info.pImageMemoryBarriers = barriers;

    vkCmdPipelineBarrier2(_buffer, &info);
}

void cala::backend::vulkan::CommandBuffer::pipelineBarrier(std::span<Buffer::Barrier> bufferBarriers) {
    VkBufferMemoryBarrier2 barriers[bufferBarriers.size()];
    for (u32 i = 0; i < bufferBarriers.size(); i++) {
        auto& b = barriers[i];
        auto& barrier = bufferBarriers[i];
        b.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
        b.buffer = barrier.buffer->buffer();
        b.size = barrier.buffer->size();
        b.offset = 0;
        b.srcStageMask = getPipelineStage(barrier.srcStage);
        b.dstStageMask = getPipelineStage(barrier.dstStage);
        b.srcAccessMask = getAccessFlags(barrier.srcAccess);
        b.dstAccessMask = getAccessFlags(barrier.dstAccess);
        b.srcQueueFamilyIndex = -1;
        b.dstQueueFamilyIndex = -1;
        b.pNext = nullptr;
    }
    VkDependencyInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    info.bufferMemoryBarrierCount = bufferBarriers.size();
    info.pBufferMemoryBarriers = barriers;

    vkCmdPipelineBarrier2(_buffer, &info);
}

void cala::backend::vulkan::CommandBuffer::pipelineBarrier(std::span<MemoryBarrier> memoryBarriers) {
    VkMemoryBarrier2 barriers[memoryBarriers.size()];
    for (u32 i = 0; i < memoryBarriers.size(); i++) {
        auto& b = barriers[i];
        auto& barrier = memoryBarriers[i];
        b.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
        b.pNext = nullptr;
        b.srcStageMask = getPipelineStage(barrier.srcStage);
        b.dstStageMask = getPipelineStage(barrier.dstStage);
        b.srcAccessMask = getAccessFlags(barrier.srcAccess);
        b.dstAccessMask = getAccessFlags(barrier.dstAccess);
    }
    VkDependencyInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    info.memoryBarrierCount = memoryBarriers.size();
    info.pMemoryBarriers = barriers;

    vkCmdPipelineBarrier2(_buffer, &info);
}

void cala::backend::vulkan::CommandBuffer::pushDebugLabel(std::string_view label, std::array<f32, 4> colour) {
#ifndef NDEBUG
    _debugLabels.push_back(label);
    _device->context().beginDebugLabel(_buffer, label, colour);
#endif
}

void cala::backend::vulkan::CommandBuffer::popDebugLabel() {
#ifndef NDEBUG
    _device->context().endDebugLabel(_buffer);
    _debugLabels.pop_back();
#endif
}

void cala::backend::vulkan::CommandBuffer::startPipelineStatistics() {
    vkCmdResetQueryPool(_buffer, _device->context().pipelineStatisticsPool(), 0, 6);
    vkCmdBeginQuery(_buffer, _device->context().pipelineStatisticsPool(), 0, 0);
}

void cala::backend::vulkan::CommandBuffer::stopPipelineStatistics() {
    vkCmdEndQuery(_buffer, _device->context().pipelineStatisticsPool(), 0);
}




std::expected<bool, cala::backend::Error> cala::backend::vulkan::CommandBuffer::submit(std::span<SemaphoreSubmit> waitSemaphores, std::span<SemaphoreSubmit> signalSemaphores, VkFence fence) {
    PROFILE_NAMED("CommandBuffer::Submit");
    end();

    VkSemaphoreSubmitInfo waits[waitSemaphores.size()];
    VkSemaphoreSubmitInfo signals[signalSemaphores.size()];

    for (u32 i = 0; i < waitSemaphores.size(); i++) {
        auto& wait = waitSemaphores[i];
        waits[i].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        waits[i].pNext = nullptr;
        waits[i].semaphore = wait.semaphore->semaphore();
        waits[i].value = wait.value;
        waits[i].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        waits[i].deviceIndex = 0;
    }

    for (u32 i = 0; i < signalSemaphores.size(); i++) {
        auto& signal = signalSemaphores[i];
        signals[i].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        signals[i].pNext = nullptr;
        signals[i].semaphore = signal.semaphore->semaphore();
        signals[i].value = signal.value;
        signals[i].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        signals[i].deviceIndex = 0;
    }

    VkCommandBufferSubmitInfo commandSubmitInfo{};
    commandSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    commandSubmitInfo.commandBuffer = _buffer;
    commandSubmitInfo.deviceMask = 0;

    VkSubmitInfo2 submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.waitSemaphoreInfoCount = waitSemaphores.size();
    submitInfo.pWaitSemaphoreInfos = waits;
    submitInfo.signalSemaphoreInfoCount = signalSemaphores.size();
    submitInfo.pSignalSemaphoreInfos = signals;
    submitInfo.commandBufferInfoCount = 1;
    submitInfo.pCommandBufferInfos = &commandSubmitInfo;

    auto res = vkQueueSubmit2(_queue, 1, &submitInfo, fence);
    if (res != VK_SUCCESS) {
        return std::unexpected(static_cast<Error>(res));
    }
    return res == VK_SUCCESS;
}

bool cala::backend::vulkan::CommandBuffer::PipelineEqual::operator()(const PipelineKey &lhs, const PipelineKey &rhs) const {
    return 0 == memcmp((const void*)&lhs, (const void*)&rhs, sizeof(lhs));
}

void cala::backend::vulkan::CommandBuffer::writeBufferMarker(cala::backend::PipelineStage stage, std::string_view cmd) {
#ifndef NDEBUG
    if (_device->context().getSupportedExtensions().AMD_buffer_marker && _device->_markerBuffer[_device->frameIndex()]) {
        vkCmdWriteBufferMarkerAMD(_buffer, (VkPipelineStageFlagBits)getPipelineStage(stage), _device->_markerBuffer[_device->frameIndex()]->buffer(), _device->_offset, _device->_marker);

        if (_device->_markedCmds[_device->frameIndex()].size() >= _device->_marker) {
            _device->_markedCmds[_device->_offset / sizeof(u32)][_device->frameIndex()] = std::make_pair(cmd, _device->_marker);
        } else {
            _device->_markedCmds[_device->frameIndex()].push_back(std::make_pair(cmd, _device->_marker));
        }
        _device->_offset += sizeof(u32);
        _device->_marker++;
    }
#endif
}