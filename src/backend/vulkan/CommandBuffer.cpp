#include "Cala/backend/vulkan/CommandBuffer.h"
#include <Cala/backend/vulkan/primitives.h>
#include <Cala/backend/vulkan/Driver.h>
#include <Ende/profile/profile.h>

#include <Ende/log/log.h>

cala::backend::vulkan::CommandBuffer::CommandBuffer(Driver& driver, VkQueue queue, VkCommandBuffer buffer)
    : _driver(driver),
    _buffer(buffer),
    _signal(VK_NULL_HANDLE),
    _queue(queue),
    _active(false),
    _renderPass(nullptr),
    _framebuffer(nullptr),
    _indexBuffer(nullptr),
    _computeBound(false),
    _currentPipeline(VK_NULL_HANDLE),
    _currentSets{VK_NULL_HANDLE},
    _descriptorPool(VK_NULL_HANDLE)
{
    VkSemaphoreCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    vkCreateSemaphore(_driver.context().device(), &createInfo, nullptr, &_signal);

    VkDescriptorPoolSize poolSizes[] = {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000}
    };

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{};
    descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCreateInfo.flags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
    descriptorPoolCreateInfo.poolSizeCount = 4;
    descriptorPoolCreateInfo.pPoolSizes = poolSizes;
    descriptorPoolCreateInfo.maxSets = 1000;
    vkCreateDescriptorPool(_driver.context().device(), &descriptorPoolCreateInfo, nullptr, &_descriptorPool);
}

cala::backend::vulkan::CommandBuffer::~CommandBuffer() {
    vkDestroySemaphore(_driver.context().device(), _signal, nullptr);

    for (auto& pipeline : _pipelines) {
        vkDestroyPipeline(_driver.context().device(), pipeline.second, nullptr);
    }

    vkDestroyDescriptorPool(_driver.context().device(), _descriptorPool, nullptr);
}

bool cala::backend::vulkan::CommandBuffer::begin() {
    vkResetCommandBuffer(_buffer, 0);
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    _active = vkBeginCommandBuffer(_buffer, &beginInfo) == VK_SUCCESS;
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

//    VkClearValue clear[2] = {{0.f, 0.f, 0.f, 1.f}, {1.f, 0.f}};
    auto clearValues = renderPass.clearValues();
    beginInfo.clearValueCount = clearValues.size();
    beginInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(_buffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
    bindRenderPass(renderPass);
}

void cala::backend::vulkan::CommandBuffer::begin(Framebuffer &framebuffer) {
    begin(framebuffer.renderPass(), framebuffer.framebuffer(), framebuffer.extent());
    _framebuffer = &framebuffer;
}

void cala::backend::vulkan::CommandBuffer::end(RenderPass &renderPass) {
    vkCmdEndRenderPass(_buffer);
}

void cala::backend::vulkan::CommandBuffer::end(Framebuffer &framebuffer) {
    end(framebuffer.renderPass());
}


void cala::backend::vulkan::CommandBuffer::bindProgram(const ShaderProgram &program) {
    if (_pipelineKey.layout != program._layout)
        _pipelineKey.layout = program._layout;

    for (u32 i = 0; i < MAX_SET_COUNT; i++) {
        if (_descriptorKey[i].setLayout != program._setLayout[i])
            _descriptorKey[i].setLayout = program._setLayout[i];
//        setLayout[i] = program._setLayout[i];
    }
    for (u32 i = 0; i < program._stages.size(); i++) {
//        if (_pipelineKey.shaders[i] != program._stages[i]) {
            _pipelineKey.shaders[i] = program._stages[i];

//            _dirty = true;
//        }
    }
    _pipelineKey.shaderCount = program._stages.size();
    _computeBound = program.stagePresent(ShaderStage::COMPUTE);
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
}

void cala::backend::vulkan::CommandBuffer::bindAttributeDescriptions(ende::Span<VkVertexInputAttributeDescription> attributes) {
    memset(_pipelineKey.attributes, 0, sizeof(_pipelineKey.attributes));
    memcpy(_pipelineKey.attributes, attributes.data(), attributes.size() * sizeof(VkVertexInputAttributeDescription));
}

void cala::backend::vulkan::CommandBuffer::bindVertexArray(ende::Span<VkVertexInputBindingDescription> bindings,
                                                           ende::Span<VkVertexInputAttributeDescription> attributes) {
    bindBindings(bindings);
    bindAttributeDescriptions(attributes);
}

void cala::backend::vulkan::CommandBuffer::bindRenderPass(RenderPass& renderPass) {
    if (_pipelineKey.renderPass != renderPass.renderPass()) {
        _pipelineKey.renderPass = renderPass.renderPass();
        _renderPass = &renderPass;
//        _dirty = true;
    }
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
        _pipelineKey.raster = state;
//        _dirty = true;
    }
}

void cala::backend::vulkan::CommandBuffer::bindDepthState(DepthState state) {
    if (ende::util::MurmurHash<DepthState>()(_pipelineKey.depth) != ende::util::MurmurHash<DepthState>()(state)) {
        _pipelineKey.depth = state;
    }
}

void cala::backend::vulkan::CommandBuffer::bindBlendState(BlendState state) {
    if (ende::util::MurmurHash<BlendState>()(_pipelineKey.blend) != ende::util::MurmurHash<BlendState>()(state)) {
        _pipelineKey.blend = state;
    }
}

void cala::backend::vulkan::CommandBuffer::bindPipeline() {
    //TODO: add dirty check so dont have to search to check if bound
    if (auto pipeline = getPipeline(); pipeline != _currentPipeline && pipeline != VK_NULL_HANDLE) {
        vkCmdBindPipeline(_buffer, _computeBound ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        _currentPipeline = pipeline;
    }
}


//void cala::backend::vulkan::CommandBuffer::bindBuffer(u32 set, u32 slot, VkBuffer buffer, u32 offset, u32 range) {
//    assert(set < MAX_SET_COUNT && "set is greater than valid number of descriptor sets");
//    _descriptorKey[set].buffers[slot] = {buffer, offset, range};
//}

void cala::backend::vulkan::CommandBuffer::bindBuffer(u32 set, u32 binding, Buffer &buffer, u32 offset, u32 range) {
    assert(set < MAX_SET_COUNT && "set is greater than valid number of descriptor sets");
    _descriptorKey[set].buffers[binding] = { &buffer, offset, range == 0 ? (buffer.size() - offset) : range };
//    bindBuffer(set, slot, buffer.buffer(), offset, range == 0 ? buffer.size() : range);
}

void cala::backend::vulkan::CommandBuffer::bindImage(u32 set, u32 binding, Image::View& image, Sampler& sampler, bool storage) {
    _descriptorKey[set].images[binding] = { image.parent(), image.view, sampler.sampler(), storage ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, storage };
}

void cala::backend::vulkan::CommandBuffer::pushConstants(ende::Span<const void> data, u32 offset) {
    vkCmdPushConstants(_buffer, _pipelineKey.layout, _pipelineKey.shaderCount > 1 ? VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT : VK_SHADER_STAGE_VERTEX_BIT, offset, data.size(), data.data());
}

void cala::backend::vulkan::CommandBuffer::bindDescriptors() {

    u32 setCount = 0;
    // find descriptors with key
    for (u32 i = 0; i < MAX_SET_COUNT; i++) {
        auto descriptor = getDescriptorSet(i);
        _currentSets[i] = descriptor;
        setCount++;
    }

    // bind descriptor
    vkCmdBindDescriptorSets(_buffer, _computeBound ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS, _pipelineKey.layout, 0, setCount, _currentSets, 0, nullptr);

}

void cala::backend::vulkan::CommandBuffer::clearDescriptors() {
    for (auto& setKey : _descriptorKey)
        setKey = DescriptorKey{};
}


void cala::backend::vulkan::CommandBuffer::bindVertexBuffer(u32 first, VkBuffer buffer, u32 offset) {
    VkDeviceSize offsets = offset;
    vkCmdBindVertexBuffers(_buffer, first, 1, &buffer, &offsets);
    _indexBuffer = nullptr; //TODO: add support for multiple draws from single buffer
}

void cala::backend::vulkan::CommandBuffer::bindVertexBuffers(u32 first, ende::Span<VkBuffer> buffers, ende::Span<VkDeviceSize> offsets) {
    assert(buffers.size() == offsets.size());
    vkCmdBindVertexBuffers(_buffer, first, buffers.size(), buffers.data(), offsets.data());
    _indexBuffer = nullptr;
}

void cala::backend::vulkan::CommandBuffer::bindVertexBuffer(u32 first, Buffer &buffer, u32 offset) {
    bindVertexBuffer(first, buffer.buffer(), offset);
}

void cala::backend::vulkan::CommandBuffer::bindIndexBuffer(Buffer& buffer, u32 offset) {
    vkCmdBindIndexBuffer(_buffer, buffer.buffer(), offset, VK_INDEX_TYPE_UINT32);
    _indexBuffer = &buffer;
}


void cala::backend::vulkan::CommandBuffer::draw(u32 count, u32 instanceCount, u32 first, u32 firstInstance) {
    if (_computeBound) throw std::runtime_error("Trying to draw when compute pipeline is bound");
    if (_indexBuffer)
        vkCmdDrawIndexed(_buffer, count, instanceCount, first, 0, firstInstance);
    else
        vkCmdDraw(_buffer, count, instanceCount, first, firstInstance);
}

void cala::backend::vulkan::CommandBuffer::drawIndirect(Buffer &buffer, u32 offset, u32 drawCount, u32 stride) {
    if (_computeBound) throw std::runtime_error("Trying to draw when compute pipeline is bound");

    if (stride == 0)
        stride = sizeof(u32) * 4;
    vkCmdDrawIndirect(_buffer, buffer.buffer(), offset, drawCount, stride);
}

void cala::backend::vulkan::CommandBuffer::dispatchCompute(u32 x, u32 y, u32 z) {
    if (!_computeBound) throw std::runtime_error("Trying to dispatch compute when graphics pipeline is bound");
    vkCmdDispatch(_buffer, x, y, z);
}

void cala::backend::vulkan::CommandBuffer::pipelineBarrier(PipelineStage srcStage, PipelineStage dstStage, VkDependencyFlags dependencyFlags, ende::Span<VkBufferMemoryBarrier> bufferBarriers, ende::Span<VkImageMemoryBarrier> imageBarriers) {
    vkCmdPipelineBarrier(_buffer, getPipelineStage(srcStage), getPipelineStage(dstStage), dependencyFlags, 0, nullptr, bufferBarriers.size(), bufferBarriers.data(), imageBarriers.size(), imageBarriers.data());
}

void cala::backend::vulkan::CommandBuffer::pushDebugLabel(std::string_view label, std::array<f32, 4> colour) {
#ifndef NDEBUG
    _debugLabels.push(label);
    _driver.context().beginDebugLabel(_buffer, label, colour);
#endif
}

void cala::backend::vulkan::CommandBuffer::popDebugLabel() {
#ifndef NDEBUG
    _driver.context().endDebugLabel(_buffer);
    _debugLabels.pop();
#endif
}

void cala::backend::vulkan::CommandBuffer::startPipelineStatistics() {
    vkCmdResetQueryPool(_buffer, _driver.context().pipelineStatisticsPool(),0, 6);
    vkCmdBeginQuery(_buffer, _driver.context().pipelineStatisticsPool(), 0, 0);
}

void cala::backend::vulkan::CommandBuffer::stopPipelineStatistics() {
    vkCmdEndQuery(_buffer, _driver.context().pipelineStatisticsPool(), 0);
}





bool cala::backend::vulkan::CommandBuffer::submit(ende::Span<VkSemaphore> wait, VkFence fence) {
    PROFILE

    end();

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    assert(wait.size() <= 2);
    VkPipelineStageFlags waitDstStageMask[2] = { VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT };
//    VkSemaphore waitSemaphore[wait] = { wait };

//    submitInfo.waitSemaphoreCount = wait == VK_NULL_HANDLE ? 0 : 1;
//    submitInfo.pWaitSemaphores = waitSemaphore;
    submitInfo.pWaitDstStageMask = waitDstStageMask;

    submitInfo.waitSemaphoreCount = wait.size();
    submitInfo.pWaitSemaphores = wait.data();

    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &_signal;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &_buffer;

    vkQueueSubmit(_queue, 1, &submitInfo, fence);
    return true;
}


VkPipeline cala::backend::vulkan::CommandBuffer::getPipeline() {
    PROFILE
    // check if exists in cache
    auto it = _pipelines.find(_pipelineKey);
    if (it != _pipelines.end())
        return it->second;

    // create new pipeline
    VkPipeline pipeline;
    if (!_computeBound) {

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

        pipelineInfo.stageCount = _pipelineKey.shaderCount;
        pipelineInfo.pStages = _pipelineKey.shaders;

        pipelineInfo.renderPass = _pipelineKey.renderPass;
        pipelineInfo.subpass = 0;

        u32 countVertexBinding = 0;
        u32 countVertexAttribute = 0;
        for (u32 binding = 0; binding < MAX_VERTEX_INPUT_BINDINGS; binding++) {
            if (_pipelineKey.bindings[binding].stride > 0)
                countVertexBinding++;
        }
        for (u32 attribute = 0; attribute < MAX_VERTEX_INPUT_ATTRIBUTES; attribute++) {
            if (_pipelineKey.attributes[attribute].format > 0)
                countVertexAttribute++;
        }

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = countVertexBinding;
        vertexInputInfo.pVertexBindingDescriptions = _pipelineKey.bindings;
        vertexInputInfo.vertexAttributeDescriptionCount = countVertexAttribute;
        vertexInputInfo.pVertexAttributeDescriptions = _pipelineKey.attributes;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        auto frameSize = _framebuffer->extent();

        VkViewport viewport{};

        //default viewport
        if (_pipelineKey.viewPort.x == 0 &&
            _pipelineKey.viewPort.y == 0 &&
            _pipelineKey.viewPort.width == 0 &&
            _pipelineKey.viewPort.height == 0 &&
            _pipelineKey.viewPort.minDepth == 0.f &&
            _pipelineKey.viewPort.maxDepth == 1.f) {
            viewport.x = 0.f;
            viewport.y = 0.f;
            viewport.width = frameSize.first;
            viewport.height = frameSize.second;
            viewport.minDepth = 0.f;
            viewport.maxDepth = 1.f;
        } else { //custom viewport
            viewport.x = _pipelineKey.viewPort.x;
            viewport.y = _pipelineKey.viewPort.y;
            viewport.width = _pipelineKey.viewPort.width;
            viewport.height = _pipelineKey.viewPort.height;
            viewport.minDepth = _pipelineKey.viewPort.minDepth;
            viewport.maxDepth = _pipelineKey.viewPort.maxDepth;
        }

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = {.width=frameSize.first, .height=frameSize.second};

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = _pipelineKey.raster.depthClamp;
        rasterizer.rasterizerDiscardEnable = _pipelineKey.raster.rasterDiscard;
        rasterizer.polygonMode = getPolygonMode(_pipelineKey.raster.polygonMode);
        rasterizer.lineWidth = _pipelineKey.raster.lineWidth;
        rasterizer.cullMode = getCullMode(_pipelineKey.raster.cullMode);
        rasterizer.frontFace = getFrontFace(_pipelineKey.raster.frontFace);
        rasterizer.depthBiasEnable = _pipelineKey.raster.depthBias;
        rasterizer.depthBiasConstantFactor = 0.f;
        rasterizer.depthBiasClamp = 0.f;
        rasterizer.depthBiasSlopeFactor = 0.f;

        VkPipelineMultisampleStateCreateInfo multisample{};
        multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample.sampleShadingEnable = VK_FALSE;
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisample.minSampleShading = 1.f;
        multisample.pSampleMask = nullptr;
        multisample.alphaToCoverageEnable = VK_FALSE;
        multisample.alphaToOneEnable = VK_FALSE;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = _pipelineKey.depth.test;
        depthStencil.depthWriteEnable = _pipelineKey.depth.write;
        depthStencil.depthCompareOp = getCompareOp(_pipelineKey.depth.compareOp);
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.minDepthBounds = 0.f;
        depthStencil.maxDepthBounds = 1.f;
        depthStencil.stencilTestEnable = VK_FALSE;
        depthStencil.front = {};
        depthStencil.back = {};

        //TODO: set up so can be configured by user
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = _pipelineKey.blend.blend;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcColorBlendFactor = getBlendFactor(_pipelineKey.blend.srcFactor);
        colorBlendAttachment.dstColorBlendFactor = getBlendFactor(_pipelineKey.blend.dstFactor);
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;

        VkPipelineColorBlendAttachmentState colourBlendAttachments[3] {};
        for (auto& attachment : colourBlendAttachments)
            attachment = colorBlendAttachment;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY;
        colorBlending.attachmentCount = _renderPass->colourAttachmentCount();
        colorBlending.pAttachments = colourBlendAttachments;
        colorBlending.blendConstants[0] = 0.f;
        colorBlending.blendConstants[1] = 0.f;
        colorBlending.blendConstants[2] = 0.f;
        colorBlending.blendConstants[3] = 0.f;

        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisample;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.layout = _pipelineKey.layout;

        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex = -1;

//        VkPipeline pipeline;
        if (vkCreateGraphicsPipelines(_driver.context().device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS)
            throw std::runtime_error("Error creating pipeline");
    } else {
        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.stage = _pipelineKey.shaders[0];
        pipelineInfo.layout = _pipelineKey.layout;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex = -1;

        if (vkCreateComputePipelines(_driver.context().device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS)
            throw std::runtime_error("Error creating compute pipeline");
    }

    _pipelines.emplace(std::make_pair(_pipelineKey, pipeline));

    return pipeline;
}

VkDescriptorSet cala::backend::vulkan::CommandBuffer::getDescriptorSet(u32 set) {
    PROFILE
    assert(set < MAX_SET_COUNT && "set is greater than allowed descriptor count");
    auto key = _descriptorKey[set];

    if (key.setLayout == VK_NULL_HANDLE)
        return VK_NULL_HANDLE;

    auto it = _descriptorSets.find(key);
    if (it != _descriptorSets.end()) {
        return it->second;
    }


    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = _descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &key.setLayout;

    VkDescriptorSet descriptorSet;
    vkAllocateDescriptorSets(_driver.context().device(), &allocInfo, &descriptorSet);

    for (u32 i = 0; i < MAX_BINDING_PER_SET; i++) {

//        if (key.buffers[i].buffer != VK_NULL_HANDLE) {
        if (key.buffers[i].buffer != nullptr) {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = key.buffers[i].buffer->buffer();
            bufferInfo.offset = key.buffers[i].offset;
            bufferInfo.range = key.buffers[i].range;

            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = descriptorSet;
            descriptorWrite.dstBinding = i;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = key.buffers[i].buffer->usage() == BufferUsage::UNIFORM ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pBufferInfo = &bufferInfo;
            descriptorWrite.pImageInfo = nullptr;
            descriptorWrite.pTexelBufferView = nullptr;

            //TODO: batch writes
            vkUpdateDescriptorSets(_driver.context().device(), 1, &descriptorWrite, 0, nullptr);
//        } else if (key.images[i].image != VK_NULL_HANDLE) {
        } else if (key.images[i].image != nullptr) {
            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = static_cast<VkImageLayout>(key.images[i].flags);
            imageInfo.imageView = key.images[i].view;
            imageInfo.sampler = key.images[i].sampler;

            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = descriptorSet;
            descriptorWrite.dstBinding = i;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = key.images[i].storage ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
//            descriptorWrite.descriptorType = (key.images[i].image->usage() & ImageUsage::STORAGE) == ImageUsage::STORAGE ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pBufferInfo = nullptr;
            descriptorWrite.pImageInfo = &imageInfo;
            descriptorWrite.pTexelBufferView = nullptr;

            //TODO: batch writes
            vkUpdateDescriptorSets(_driver.context().device(), 1, &descriptorWrite, 0, nullptr);
        }
    }

    _descriptorSets.emplace(std::make_pair(key, descriptorSet));
    return descriptorSet;
}