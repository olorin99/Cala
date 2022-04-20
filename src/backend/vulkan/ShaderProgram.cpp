#include "Cala/backend/vulkan/ShaderProgram.h"
#include <Cala/backend/vulkan/Driver.h>
#include <Cala/backend/vulkan/primitives.h>
#include "../third_party/SPIRV-Cross-master/spirv_cross.hpp"

cala::backend::vulkan::ShaderProgram::Builder cala::backend::vulkan::ShaderProgram::create() {
    return {};
}

cala::backend::vulkan::ShaderProgram::Builder &cala::backend::vulkan::ShaderProgram::Builder::addStage(ende::Span<u32> code, ShaderStage stage) {
    _stages.push({code, stage});
    return *this;
}

cala::backend::vulkan::ShaderProgram cala::backend::vulkan::ShaderProgram::Builder::compile(Driver& driver) {
    ShaderProgram program(driver.context().device());

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
            assert(set < MAX_SET_COUNT && "supplied set count is greater than valid set count per shader program");
            assert(set < MAX_BINDING_PER_SET && "supplied binding count is greater than valid binding count per shader program");

            const spirv_cross::SPIRType &type = comp.get_type(resource.base_type_id);
            u32 size = comp.get_declared_struct_size(type);
            u32 memberCount = type.member_types.size();


            program._interface.sets[set].id = set;
            program._interface.sets[set].byteSize = size;

            program._interface.sets[set].bindings[binding].id = binding;
            program._interface.sets[set].bindings[binding].type = ShaderInterface::BindingType::UNIFORM;
            program._interface.sets[set].bindings[binding].byteSize = size;

            for (u32 i = 0; i < memberCount; i++) {
                const std::string name = comp.get_member_name(type.self, i);
                u32 offset = comp.type_struct_member_offset(type, i);
                u32 memberSize = comp.get_declared_struct_member_size(type, i);
                program._interface.getMemberList(set, binding)[name] = {offset, memberSize};
            }

            bindings[set][bindingCount[set]].binding = binding;
            bindings[set][bindingCount[set]].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            bindings[set][bindingCount[set]].descriptorCount = 1;
            bindings[set][bindingCount[set]].stageFlags = getShaderStage(stage.second);
            bindings[set][bindingCount[set]].pImmutableSamplers = nullptr;
            bindingCount[set]++;
        }
        for (auto& resource : resources.storage_buffers) {
            u32 set = comp.get_decoration(resource.id, spv::DecorationDescriptorSet);
            u32 binding = comp.get_decoration(resource.id, spv::DecorationBinding);
            assert(set < MAX_SET_COUNT && "supplied set count is greater than valid set count per shader program");
            assert(set < MAX_BINDING_PER_SET && "supplied binding count is greater than valid binding count per shader program");

            const spirv_cross::SPIRType &type = comp.get_type(resource.base_type_id);
            u32 size = comp.get_declared_struct_size(type);
            u32 memberCount = type.member_types.size();


            program._interface.sets[set].id = set;
            program._interface.sets[set].byteSize = size;

            program._interface.sets[set].bindings[binding].id = binding;
            program._interface.sets[set].bindings[binding].type = ShaderInterface::BindingType::STORAGE_BUFFER;
            program._interface.sets[set].bindings[binding].byteSize = size;

            for (u32 i = 0; i < memberCount; i++) {
                const std::string name = comp.get_member_name(type.self, i);
                u32 offset = comp.type_struct_member_offset(type, i);
                u32 memberSize = comp.get_declared_struct_member_size(type, i);
                program._interface.getMemberList(set, binding)[name] = {offset, memberSize};
            }

            bindings[set][bindingCount[set]].binding = binding;
            bindings[set][bindingCount[set]].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            bindings[set][bindingCount[set]].descriptorCount = 1;
            bindings[set][bindingCount[set]].stageFlags = getShaderStage(stage.second);
            bindings[set][bindingCount[set]].pImmutableSamplers = nullptr;
            bindingCount[set]++;
        }
        for (auto& resource : resources.sampled_images) {
            u32 set = comp.get_decoration(resource.id, spv::DecorationDescriptorSet);
            u32 binding = comp.get_decoration(resource.id, spv::DecorationBinding);
            assert(set < MAX_SET_COUNT && "supplied set count is greater than valid set count per shader program");
            assert(set < MAX_BINDING_PER_SET && "supplied binding count is greater than valid binding count per shader program");

            const spirv_cross::SPIRType &type = comp.get_type(resource.base_type_id);
            program._interface.sets[set].id = set;

            program._interface.sets[set].bindings[binding].id = binding;
            program._interface.sets[set].bindings[binding].type = ShaderInterface::BindingType::SAMPLER;
            program._interface.sets[set].bindings[binding].dimensions = 2;
            program._interface.sets[set].bindings[binding].name = comp.get_name(resource.id);

            bindings[set][bindingCount[set]].binding = binding;
            bindings[set][bindingCount[set]].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            bindings[set][bindingCount[set]].descriptorCount = 1;
            bindings[set][bindingCount[set]].stageFlags = getShaderStage(stage.second);
            bindings[set][bindingCount[set]].pImmutableSamplers = nullptr;
            bindingCount[set]++;
        }
        for (auto& resource : resources.storage_images) {
            u32 set = comp.get_decoration(resource.id, spv::DecorationDescriptorSet);
            u32 binding = comp.get_decoration(resource.id, spv::DecorationBinding);
            assert(set < MAX_SET_COUNT && "supplied set count is greater than valid set count per shader program");
            assert(set < MAX_BINDING_PER_SET && "supplied binding count is greater than valid binding count per shader program");

            const spirv_cross::SPIRType &type = comp.get_type(resource.base_type_id);
            program._interface.sets[set].id = set;

            program._interface.sets[set].bindings[binding].id = binding;
            program._interface.sets[set].bindings[binding].type = ShaderInterface::BindingType::STORAGE_IMAGE;
            program._interface.sets[set].bindings[binding].dimensions = 2;
            program._interface.sets[set].bindings[binding].name = comp.get_name(resource.id);

            bindings[set][bindingCount[set]].binding = binding;
            bindings[set][bindingCount[set]].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            bindings[set][bindingCount[set]].descriptorCount = 1;
            bindings[set][bindingCount[set]].stageFlags = getShaderStage(stage.second);
            bindings[set][bindingCount[set]].pImmutableSamplers = nullptr;
            bindingCount[set]++;
        }

        for (u32 i = 0; i < MAX_SET_COUNT; i++) {
            program._interface.sets[i].id = i;
            u32 size = 0;
            for (u32 j = 0; j < MAX_BINDING_PER_SET; j++) {
                if (program._interface.sets[i].bindings[j].type == ShaderInterface::BindingType::UNIFORM)
                    size += program._interface.sets[i].bindings[j].byteSize;
            }
            program._interface.sets[i].byteSize = size;
        }

        VkShaderModule shader;
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = stage.first.size() * sizeof(u32);
        createInfo.pCode = stage.first.data();

        if (vkCreateShaderModule(driver.context().device(), &createInfo, nullptr, &shader) != VK_SUCCESS)
            throw std::runtime_error("Unable to create shader");

        VkPipelineShaderStageCreateInfo stageCreateInfo{};
        stageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageCreateInfo.stage = static_cast<VkShaderStageFlagBits>(stage.second);
        stageCreateInfo.module = shader;
        stageCreateInfo.pName = "main";

        program._stages.push(stageCreateInfo);
        program._stageFlags |= stage.second;
    }


//    u32 setCount = 0;

    VkDescriptorSetLayout setLayouts[4] = {};

    for (u32 i = 0; i < MAX_SET_COUNT; i++) {
        setLayouts[i] = driver.getSetLayout({bindings[i], bindingCount[i]});
    }

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = MAX_SET_COUNT;
    pipelineLayoutInfo.pSetLayouts = setLayouts;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;

    VkPipelineLayout  pipelineLayout;
    vkCreatePipelineLayout(driver.context().device(), &pipelineLayoutInfo, nullptr, &pipelineLayout);


    program._layout = pipelineLayout;
    for (u32 i = 0; i < 4; i++)
        program._setLayout[i] = setLayouts[i];

    return program;
}


cala::backend::vulkan::ShaderProgram::ShaderProgram(VkDevice device)
    : _device(device),
    _layout(VK_NULL_HANDLE),
    _setLayout{},
    _interface{},
    _stageFlags(ShaderStage::NONE)
{}

cala::backend::vulkan::ShaderProgram::~ShaderProgram() {
    if (_device == VK_NULL_HANDLE) return;

    for (auto& stage : _stages)
        vkDestroyShaderModule(_device, stage.module, nullptr);

    vkDestroyPipelineLayout(_device, _layout, nullptr);
}

cala::backend::vulkan::ShaderProgram::ShaderProgram(ShaderProgram &&rhs)
    : _device(VK_NULL_HANDLE),
    _layout(VK_NULL_HANDLE)
{
    std::swap(_device, rhs._device);
    std::swap(_stages, rhs._stages);
    for (u32 i = 0; i < MAX_SET_COUNT; i++)
        _setLayout[i] = rhs._setLayout[i];
    std::swap(_layout, rhs._layout);
    std::swap(_interface, rhs._interface);
    std::swap(_stageFlags, rhs._stageFlags);
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

bool cala::backend::vulkan::ShaderProgram::stagePresent(ShaderStage stageFlags) const {
    return (_stageFlags & stageFlags) == stageFlags;
}