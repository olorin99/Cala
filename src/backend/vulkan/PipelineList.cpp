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


void cala::backend::vulkan::PipelineList::bindProgram(ShaderProgram &program) {
    for (u32 i = 0; i < program._stages.size(); i++) {
        if (_key.shaders[i] != program._stages[i].module) {
            _key.shaders[i] = program._stages[i].module;

            _dirty = true;
        }
    }
}

void cala::backend::vulkan::PipelineList::bindRenderPass(VkRenderPass renderPass) {
    if (_key.renderPass != renderPass) {
        _key.renderPass = renderPass;
        _dirty = true;
    }
}


void cala::backend::vulkan::PipelineList::bindPipeline(VkCommandBuffer cmdBuffer) {
    VkPipeline pipeline;
    if (get(&pipeline))
        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
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


    VkVertexInputBindingDescription vertexBinding{};
    vertexBinding.binding = 0;
    vertexBinding.stride = sizeof(f32) * 5;
    vertexBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription vertexAttributes[] = {
            { .location=0, .binding=0, .format=VK_FORMAT_R32G32_SFLOAT, .offset=0 },
            { .location=1, .binding=0, .format=VK_FORMAT_R32G32B32_SFLOAT, .offset=8 }
    };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &vertexBinding;
    vertexInputInfo.vertexAttributeDescriptionCount = 2;
    vertexInputInfo.pVertexAttributeDescriptions = vertexAttributes;

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
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
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

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pSetLayouts = nullptr;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;

    VkPipelineLayout pipelineLayout;
    vkCreatePipelineLayout(_device, &pipelineLayoutInfo, nullptr, &pipelineLayout);



    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisample;
    pipelineInfo.pDepthStencilState = nullptr;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = pipelineLayout;

    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    if (vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, pipeline) != VK_SUCCESS)
        throw "Error creating pipeline";

    _pipelines.emplace(std::make_pair(_key, *pipeline));

    _current = *pipeline;
    _dirty = false;
    return true;
}