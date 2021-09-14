#include "Cala/backend/vulkan/ShaderProgram.h"

cala::backend::vulkan::ShaderProgram::ShaderProgram(Context &context)
    : _context(context)
{}

cala::backend::vulkan::ShaderProgram::~ShaderProgram() {
    for (auto& stage : _stages)
        vkDestroyShaderModule(_context._device, stage.module, nullptr);
}

bool cala::backend::vulkan::ShaderProgram::addStage(ende::Span<u32> code, u32 flags) {

    VkShaderModule shader;
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = code.data();

    if (vkCreateShaderModule(_context._device, &createInfo, nullptr, &shader) != VK_SUCCESS)
        return false;

    VkPipelineShaderStageCreateInfo stageCreateInfo{};
    stageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageCreateInfo.stage = static_cast<VkShaderStageFlagBits>(flags);
    stageCreateInfo.module = shader;
    stageCreateInfo.pName = "main";

    _stages.push(stageCreateInfo);

    return true;
}