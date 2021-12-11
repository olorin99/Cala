#include "Cala/backend/vulkan/PipelineList.h"
#include <Ende/util/hash.h>

#include <Ende/math/Vec.h>
struct Vertex {
    ende::math::Vec<2, f32> position;
    ende::math::Vec3f colour;
};


cala::backend::vulkan::PipelineList::PipelineList(VkDevice device)
    : _device(device),
    _dirty(true),
    _current(VK_NULL_HANDLE)
{}

cala::backend::vulkan::PipelineList::~PipelineList() {
    for (auto& pipeline : _pipelines) {
        vkDestroyPipelineLayout(_device, pipeline.first.layout, nullptr);
        vkDestroyPipeline(_device, pipeline.second, nullptr);
    }
}

void cala::backend::vulkan::PipelineList::bindProgram(ShaderProgram &program) {
    if (_key.layout != program._layout)
        _key.layout = program._layout;
    for (u32 i = 0; i < 4; i++) {
        if (_setKey.layout[i] != program._setLayout[i])
            _setKey.layout[i] = program._setLayout[i];
    }
    for (u32 i = 0; i < program._stages.size(); i++) {
        if (_key.shaders[i] != program._stages[i].module) {
            _key.shaders[i] = program._stages[i].module;

            _dirty = true;
        }
    }
}

void cala::backend::vulkan::PipelineList::bindVertexArray(ende::Span<VkVertexInputBindingDescription> bindings, ende::Span<VkVertexInputAttributeDescription> attributes) {
    memset(_key.bindings, 0, sizeof(_key.bindings));
    memcpy(_key.bindings, bindings.data(), bindings.size() * sizeof(VkVertexInputBindingDescription));
    memset(_key.attributes, 0, sizeof(_key.attributes));
    memcpy(_key.attributes, attributes.data(), attributes.size() * sizeof(VkVertexInputAttributeDescription));
}

void cala::backend::vulkan::PipelineList::bindRenderPass(VkRenderPass renderPass) {
    if (_key.renderPass != renderPass) {
        _key.renderPass = renderPass;
        _dirty = true;
    }
}

void cala::backend::vulkan::PipelineList::bindRasterState(RasterState state) {
    if (ende::util::MurmurHash<RasterState>()(_key.raster) != ende::util::MurmurHash<RasterState>()(state)) {
        _key.raster = state;
        _dirty = true;
    }
}


void cala::backend::vulkan::PipelineList::bindPipeline(VkCommandBuffer cmdBuffer) {
    VkPipeline pipeline;
    if (get(&pipeline))
        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
}


void cala::backend::vulkan::PipelineList::bindBuffer(VkBuffer buffer, u32 set, u32 binding, u32 offset, u32 range) {
    _setKey.buffers[set][binding] = {buffer, offset, range};
}



bool cala::backend::vulkan::PipelineList::get(VkPipeline *pipeline) {
//    if (!_dirty)
//        return false;

    auto it = _pipelines.find(_key);
    if (it != _pipelines.end()) {
        _current = it->second;
        *pipeline = _current;
        _dirty = false;
        return true;
    }


    VkPipelineShaderStageCreateInfo shaderStages[2] = {{}, {}};
    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].pName = "main";
    shaderStages[0].module = _key.shaders[0];

    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].pName = "main";
    shaderStages[1].module = _key.shaders[1];


    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;

    pipelineInfo.renderPass = _key.renderPass;
    pipelineInfo.subpass = 0;



    u32 countVertexBinding = 0;
    u32 countVertexAttribute = 0;
    for (u32 i = 0; i < 10; i++) {
        if (_key.bindings[i].stride > 0)
            countVertexBinding++;
        if (_key.attributes[i].format > 0)
            countVertexAttribute++;
    }

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = countVertexBinding;
    vertexInputInfo.pVertexBindingDescriptions = _key.bindings;
    vertexInputInfo.vertexAttributeDescriptionCount = countVertexAttribute;
    vertexInputInfo.pVertexAttributeDescriptions = _key.attributes;

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
    rasterizer.depthClampEnable = _key.raster.depthClamp;
    rasterizer.rasterizerDiscardEnable = _key.raster.rasterDiscard;
    rasterizer.polygonMode = _key.raster.polygonMode;
    rasterizer.lineWidth = _key.raster.lineWidth;
    rasterizer.cullMode = _key.raster.cullMode;
    rasterizer.frontFace = _key.raster.frontFace;
    rasterizer.depthBiasEnable = _key.raster.depthBias;
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

//    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
//    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
//    pipelineLayoutInfo.setLayoutCount = 1;
//    pipelineLayoutInfo.pSetLayouts = &_tmp;
//    pipelineLayoutInfo.pushConstantRangeCount = 0;
//    pipelineLayoutInfo.pPushConstantRanges = nullptr;
//
//    VkPipelineLayout pipelineLayout;
//    VkResult result = vkCreatePipelineLayout(_device, &pipelineLayoutInfo, nullptr, &pipelineLayout);
//    _key.layout = pipelineLayout;


    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisample;
    pipelineInfo.pDepthStencilState = nullptr;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = _key.layout;

    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    if (vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, pipeline) != VK_SUCCESS)
        throw "Error creating pipeline";

    _pipelines.emplace(std::make_pair(_key, *pipeline));

    _current = *pipeline;
    _dirty = false;
    return true;
}

void cala::backend::vulkan::PipelineList::bindDescriptors(VkCommandBuffer cmdBuffer) {

//    auto it = _descriptors.find(_setKey);
//    if (it != _descriptors.end()) {
//        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _key.layout, 0, 1, &it->second, 0, nullptr);
//        return;
//    }


    u32 setCount = 0;
    for (u32 i = 0; i < 4; i++) {
        if (_setKey.layout[i] == VK_NULL_HANDLE) {
            setCount = i;
            break;
        }
    }

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
//    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = setCount;
    allocInfo.pSetLayouts = _setKey.layout;

    VkDescriptorSet descriptorSet;
    vkAllocateDescriptorSets(_device, &allocInfo, &descriptorSet);


    for (u32 i = 0; i < setCount; i++) {

        VkDescriptorBufferInfo bufferInfo[4];
        for (u32 j = 0; j < 4; j++) {
            bufferInfo[j].buffer = _setKey.buffers[i][j].buffer;
            bufferInfo[j].offset = _setKey.buffers[i][j].offset;
            bufferInfo[j].range = _setKey.buffers[i][j].range;

        }

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = descriptorSet;
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = bufferInfo;
        descriptorWrite.pImageInfo = nullptr;
        descriptorWrite.pTexelBufferView = nullptr;

        vkUpdateDescriptorSets(_device, 1, &descriptorWrite, 0, nullptr);
    }

//    _descriptors.insert(_setKey, descriptorSet);

}