#include "Cala/backend/vulkan/ShaderProgram.h"
#include <Cala/backend/vulkan/Driver.h>
#include "../third_party/SPIRV-Cross-master/spirv_cross.hpp"

cala::backend::vulkan::ShaderProgram::Builder cala::backend::vulkan::ShaderProgram::create() {
    return {};
}

cala::backend::vulkan::ShaderProgram::Builder &cala::backend::vulkan::ShaderProgram::Builder::addStage(ende::Span<u32> code, u32 flags) {
    _stages.push({code, flags});
    return *this;
}

cala::backend::vulkan::ShaderProgram cala::backend::vulkan::ShaderProgram::Builder::compile(Driver& driver) {
    ShaderProgram program(driver._context._device);

    u32 bindingCount[4] = {0};
    VkDescriptorSetLayoutBinding bindings[4][8]; // 4 sets 4 buffers each stage TODO: find proper value

    for (auto& stage : _stages) {
        // reflection
        spirv_cross::Compiler comp(stage.first.data(), stage.first.size());
        spirv_cross::ShaderResources resources = comp.get_shader_resources();

        // uniform buffers
        for (auto& resource : resources.uniform_buffers) {
            u32 set = comp.get_decoration(resource.id, spv::DecorationDescriptorSet);
            u32 binding = comp.get_decoration(resource.id, spv::DecorationBinding);

            bindings[set][bindingCount[set]].binding = binding;
            bindings[set][bindingCount[set]].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            bindings[set][bindingCount[set]].descriptorCount = 1;
            bindings[set][bindingCount[set]].stageFlags = stage.second;
            bindings[set][bindingCount[set]].pImmutableSamplers = nullptr;
            bindingCount[set]++;
        }
        for (auto& resource : resources.sampled_images) {
            u32 set = comp.get_decoration(resource.id, spv::DecorationDescriptorSet);
            u32 binding = comp.get_decoration(resource.id, spv::DecorationBinding);

            bindings[set][bindingCount[set]].binding = binding;
            bindings[set][bindingCount[set]].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            bindings[set][bindingCount[set]].descriptorCount = 1;
            bindings[set][bindingCount[set]].stageFlags = stage.second;
            bindings[set][bindingCount[set]].pImmutableSamplers = nullptr;
            bindingCount[set]++;
        }

        VkShaderModule shader;
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = stage.first.size() * sizeof(u32);
        createInfo.pCode = stage.first.data();

        if (vkCreateShaderModule(driver._context._device, &createInfo, nullptr, &shader) != VK_SUCCESS)
            throw "Unable to create shader";

        VkPipelineShaderStageCreateInfo stageCreateInfo{};
        stageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageCreateInfo.stage = static_cast<VkShaderStageFlagBits>(stage.second);
        stageCreateInfo.module = shader;
        stageCreateInfo.pName = "main";

        program._stages.push(stageCreateInfo);
    }


//    u32 setCount = 0;

    VkDescriptorSetLayout setLayouts[4] = {};
//    VkDescriptorSetLayoutCreateInfo setLayoutInfo[4] = {};
    for (u32 i = 0; i < 4; i++) {
        setLayouts[i] = driver.getSetLayout({bindings[i], bindingCount[i]});
//        setLayoutInfo[i].sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
//        setLayoutInfo[i].bindingCount = bindingCount[i];
//        setLayoutInfo[i].pBindings = bindings[i];
//        vkCreateDescriptorSetLayout(driver._context._device, &setLayoutInfo[i], nullptr, &setLayouts[i]);
    }

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 4;
    pipelineLayoutInfo.pSetLayouts = setLayouts;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;

    VkPipelineLayout  pipelineLayout;
    vkCreatePipelineLayout(driver._context._device, &pipelineLayoutInfo, nullptr, &pipelineLayout);


    program._layout = pipelineLayout;
    for (u32 i = 0; i < 4; i++)
        program._setLayout[i] = setLayouts[i];

    return program;
}


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