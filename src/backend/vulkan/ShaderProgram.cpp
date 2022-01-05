#include "Cala/backend/vulkan/ShaderProgram.h"

cala::backend::vulkan::ShaderProgram::ShaderProgram(VkDevice device)
    : _device(device),
    _layout(VK_NULL_HANDLE),
    _setLayout{}
{}

cala::backend::vulkan::ShaderProgram::~ShaderProgram() {
    for (auto& stage : _stages)
        vkDestroyShaderModule(_device, stage.module, nullptr);

    for (auto& setLayout : _setLayout)
        vkDestroyDescriptorSetLayout(_device, setLayout, nullptr);

    vkDestroyPipelineLayout(_device, _layout, nullptr);
}

bool cala::backend::vulkan::ShaderProgram::addStage(ende::Span<u32> code, u32 flags) {

    VkShaderModule shader;
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = code.data();

    if (vkCreateShaderModule(_device, &createInfo, nullptr, &shader) != VK_SUCCESS)
        return false;

    VkPipelineShaderStageCreateInfo stageCreateInfo{};
    stageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageCreateInfo.stage = static_cast<VkShaderStageFlagBits>(flags);
    stageCreateInfo.module = shader;
    stageCreateInfo.pName = "main";

    _stages.push(stageCreateInfo);

    return true;
}

VkPipelineLayout cala::backend::vulkan::ShaderProgram::layout() {
    if (_layout != VK_NULL_HANDLE)
        return _layout;




    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pSetLayouts = nullptr;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;

    vkCreatePipelineLayout(_device, &pipelineLayoutInfo, nullptr, &_layout);
    return _layout;
}