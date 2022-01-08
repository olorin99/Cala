#include "Cala/backend/vulkan/CommandBuffer.h"

cala::backend::vulkan::CommandBuffer::CommandBuffer(VkDevice device, VkQueue queue, VkCommandBuffer buffer)
    : _buffer(buffer),
    _signal(VK_NULL_HANDLE),
    _device(device),
    _queue(queue),
    _currentPipeline(VK_NULL_HANDLE),
    _currentSets{VK_NULL_HANDLE}
{
    VkSemaphoreCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    vkCreateSemaphore(_device, &createInfo, nullptr, &_signal);

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = 1000;

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{};
    descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCreateInfo.poolSizeCount = 1;
    descriptorPoolCreateInfo.pPoolSizes = &poolSize;
    descriptorPoolCreateInfo.maxSets = 1000;
    vkCreateDescriptorPool(_device, &descriptorPoolCreateInfo, nullptr, &_descriptorPool);
}

cala::backend::vulkan::CommandBuffer::~CommandBuffer() {
    vkDestroySemaphore(_device, _signal, nullptr);

    for (auto& pipeline : _pipelines) {
        vkDestroyPipeline(_device, pipeline.second, nullptr);
    }

    vkDestroyDescriptorPool(_device, _descriptorPool, nullptr);
}

bool cala::backend::vulkan::CommandBuffer::begin() {
    vkResetCommandBuffer(_buffer, 0);
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    return vkBeginCommandBuffer(_buffer, &beginInfo) == VK_SUCCESS;
}

bool cala::backend::vulkan::CommandBuffer::end() {
    return vkEndCommandBuffer(_buffer) == VK_SUCCESS;
}

void cala::backend::vulkan::CommandBuffer::begin(RenderPass &renderPass, VkFramebuffer framebuffer, std::pair<u32, u32> extent) {

    VkRenderPassBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.renderPass = renderPass.renderPass();
    beginInfo.framebuffer = framebuffer;
    beginInfo.renderArea.offset = {0, 0};
    beginInfo.renderArea.extent = {extent.first, extent.second};

    VkClearValue clear = {{{0.f, 0.f, 0.f, 1.f}}};
    beginInfo.clearValueCount = 1;
    beginInfo.pClearValues = &clear;

    vkCmdBeginRenderPass(_buffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
    bindRenderPass(renderPass);
}

void cala::backend::vulkan::CommandBuffer::end(RenderPass &renderPass) {
    vkCmdEndRenderPass(_buffer);
}


void cala::backend::vulkan::CommandBuffer::bindProgram(ShaderProgram &program) {
    if (_pipelineKey.layout != program._layout)
        _pipelineKey.layout = program._layout;

    for (u32 i = 0; i < 4; i++) {
        if (_descriptorKey[i].setLayout != program._setLayout[i])
            _descriptorKey[i].setLayout = program._setLayout[i];
    }
    for (u32 i = 0; i < program._stages.size(); i++) {
        if (_pipelineKey.shaders[i] != program._stages[i].module) {
            _pipelineKey.shaders[i] = program._stages[i].module;

//            _dirty = true;
        }
    }
}

void cala::backend::vulkan::CommandBuffer::bindVertexArray(ende::Span<VkVertexInputBindingDescription> bindings,
                                                           ende::Span<VkVertexInputAttributeDescription> attributes) {
    memset(_pipelineKey.bindings, 0, sizeof(_pipelineKey.bindings));
    memcpy(_pipelineKey.bindings, bindings.data(), bindings.size() * sizeof(VkVertexInputBindingDescription));
    memset(_pipelineKey.attributes, 0, sizeof(_pipelineKey.attributes));
    memcpy(_pipelineKey.attributes, attributes.data(), attributes.size() * sizeof(VkVertexInputAttributeDescription));
}

void cala::backend::vulkan::CommandBuffer::bindRenderPass(const RenderPass& renderPass) {
    if (_pipelineKey.renderPass != renderPass.renderPass()) {
        _pipelineKey.renderPass = renderPass.renderPass();
//        _dirty = true;
    }
}

void cala::backend::vulkan::CommandBuffer::bindRasterState(RasterState state) {
    if (ende::util::MurmurHash<RasterState>()(_pipelineKey.raster) != ende::util::MurmurHash<RasterState>()(state)) {
        _pipelineKey.raster = state;
//        _dirty = true;
    }
}

void cala::backend::vulkan::CommandBuffer::bindPipeline() {
    //TODO: add dirty check so dont have to search to check if bound
    if (auto pipeline = getPipeline(); pipeline != _currentPipeline && pipeline != VK_NULL_HANDLE) {
        vkCmdBindPipeline(_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        _currentPipeline = pipeline;
    }
}


void cala::backend::vulkan::CommandBuffer::bindBuffer(u32 set, u32 slot, VkBuffer buffer, u32 offset, u32 range) {
    assert(set < SET_COUNT && "set is greater than valid number of descriptor sets");
    _descriptorKey[set].buffers[slot] = {buffer, offset, range};
}


void cala::backend::vulkan::CommandBuffer::bindDescriptors() {

    u32 setCount = 0;
    // find descriptors with key
    for (u32 i = 0; i < SET_COUNT; i++) {
        auto descriptor = getDescriptorSet(i);
//        if (descriptor == VK_NULL_HANDLE) {
//            setCount = i + 1;
//            break;
//        }
        _currentSets[i] = descriptor;
        setCount++;
    }

    // if not available create new

    // bind descriptor
    vkCmdBindDescriptorSets(_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipelineKey.layout, 0, setCount, _currentSets, 0, nullptr);

}





bool cala::backend::vulkan::CommandBuffer::submit(VkSemaphore wait, VkFence fence) {

    end();

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkPipelineStageFlags waitDstStageMask[1] = { VK_PIPELINE_STAGE_ALL_COMMANDS_BIT };
    VkSemaphore waitSemaphore[1] = { wait };

    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphore;
    submitInfo.pWaitDstStageMask = waitDstStageMask;

    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &_signal;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &_buffer;

    vkQueueSubmit(_queue, 1, &submitInfo, fence);
    return true;
}


VkPipeline cala::backend::vulkan::CommandBuffer::getPipeline() {
    // check if exists in cache
    auto it = _pipelines.find(_pipelineKey);
    if (it != _pipelines.end())
        return it->second;

    // create new pipeline

    VkPipelineShaderStageCreateInfo shaderStages[2] = {{}, {}};
    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].pName = "main";
    shaderStages[0].module = _pipelineKey.shaders[0];

    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].pName = "main";
    shaderStages[1].module = _pipelineKey.shaders[1];


    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;

    pipelineInfo.renderPass = _pipelineKey.renderPass;
    pipelineInfo.subpass = 0;



    u32 countVertexBinding = 0;
    u32 countVertexAttribute = 0;
    for (u32 i = 0; i < 10; i++) {
        if (_pipelineKey.bindings[i].stride > 0)
            countVertexBinding++;
        if (_pipelineKey.attributes[i].format > 0)
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

    VkViewport viewport{};
    viewport.x = 0.f;
    viewport.y = 0.f;
    viewport.width = 800.f;
    viewport.height = 600.f;
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {.width=800, .height=600};

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
    rasterizer.polygonMode = _pipelineKey.raster.polygonMode;
    rasterizer.lineWidth = _pipelineKey.raster.lineWidth;
    rasterizer.cullMode = _pipelineKey.raster.cullMode;
    rasterizer.frontFace = _pipelineKey.raster.frontFace;
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

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.f;
    colorBlending.blendConstants[1] = 0.f;
    colorBlending.blendConstants[2] = 0.f;
    colorBlending.blendConstants[3] = 0.f;

    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisample;
    pipelineInfo.pDepthStencilState = nullptr;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = _pipelineKey.layout;

    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    VkPipeline pipeline;
    if (vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS)
        throw "Error creating pipeline";

    _pipelines.emplace(std::make_pair(_pipelineKey, pipeline));

    return pipeline;
}

VkDescriptorSet cala::backend::vulkan::CommandBuffer::getDescriptorSet(u32 set) {
    assert(set < SET_COUNT && "set is greater than allowed descriptor count");
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
    vkAllocateDescriptorSets(_device, &allocInfo, &descriptorSet);

    for (u32 i = 0; i < 4; i++) {
        if (key.buffers[i].buffer != VK_NULL_HANDLE) {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = key.buffers[i].buffer;
            bufferInfo.offset = key.buffers[i].offset;
            bufferInfo.range = key.buffers[i].range;

            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = descriptorSet;
            descriptorWrite.dstBinding = i;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pBufferInfo = &bufferInfo;
            descriptorWrite.pImageInfo = nullptr;
            descriptorWrite.pTexelBufferView = nullptr;

            //TODO: batch writes
            vkUpdateDescriptorSets(_device, 1, &descriptorWrite, 0, nullptr);
        }
    }

    _descriptorSets.emplace(std::make_pair(key, descriptorSet));
    return descriptorSet;
}