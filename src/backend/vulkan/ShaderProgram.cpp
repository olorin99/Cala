#include "Cala/backend/vulkan/ShaderProgram.h"
#include <Cala/backend/vulkan/Device.h>
#include <Cala/backend/vulkan/primitives.h>
#include <SPIRV-Cross/spirv_cross.hpp>

cala::backend::vulkan::ShaderProgram::Builder cala::backend::vulkan::ShaderProgram::create() {
    return {};
}

cala::backend::vulkan::ShaderProgram::Builder &cala::backend::vulkan::ShaderProgram::Builder::addStage(ende::Span<u32> code, ShaderStage stage) {
    _stages.push({code, stage});
    return *this;
}

cala::backend::vulkan::ShaderProgram cala::backend::vulkan::ShaderProgram::Builder::compile(Device& driver) {
    ShaderProgram program(driver.context().device());

    bool hasPushConstant = false;
    VkPushConstantRange pushConstant{};

    for (auto& stage : _stages) {
        // reflection
        spirv_cross::Compiler comp(stage.first.data(), stage.first.size());
        spirv_cross::ShaderResources resources = comp.get_shader_resources();

        for (auto& resource : resources.push_constant_buffers) {
            const spirv_cross::SPIRType &type = comp.get_type(resource.base_type_id);
            u32 size = comp.get_declared_struct_size(type);
            u32 memberCount = type.member_types.size();

            program._interface.pushConstants.byteSize = size;
            program._interface.pushConstants.stage |= stage.second;

            for (u32 i = 0; i < memberCount; i++) {
                const std::string& name = comp.get_member_name(type.self, i);
                u32 offset = comp.type_struct_member_offset(type, i);
                u32 memberSize = comp.get_declared_struct_member_size(type, i);
                program._interface.pushConstants.members[name] = {offset, memberSize};
            }
            pushConstant.offset = 0;
            pushConstant.size = size;
            pushConstant.stageFlags |= getShaderStage(stage.second);
            hasPushConstant = true;
        }

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
            program._interface.sets[set].byteSize = std::max(size, program._interface.sets[set].byteSize);
            program._interface.sets[set].bindingCount++;

            program._interface.sets[set].bindings[binding].id = binding;
            program._interface.sets[set].bindings[binding].type = ShaderInterface::BindingType::UNIFORM;
            program._interface.sets[set].bindings[binding].byteSize = std::max(size, program._interface.sets[set].bindings[binding].byteSize);
            program._interface.sets[set].bindings[binding].stage |= stage.second;

            for (u32 i = 0; i < memberCount; i++) {
                const std::string& name = comp.get_member_name(type.self, i);
                u32 offset = comp.type_struct_member_offset(type, i);
                u32 memberSize = comp.get_declared_struct_member_size(type, i);
                program._interface.getMemberList(set, binding)[name] = {offset, memberSize};
            }
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
            program._interface.sets[set].bindingCount++;

            program._interface.sets[set].bindings[binding].id = binding;
            program._interface.sets[set].bindings[binding].type = ShaderInterface::BindingType::STORAGE_BUFFER;
            program._interface.sets[set].bindings[binding].byteSize = size;
            program._interface.sets[set].bindings[binding].stage |= stage.second;

            for (u32 i = 0; i < memberCount; i++) {
                const std::string name = comp.get_member_name(type.self, i);
                u32 offset = comp.type_struct_member_offset(type, i);
                u32 memberSize = comp.get_declared_struct_member_size(type, i);
                program._interface.getMemberList(set, binding)[name] = {offset, memberSize};
            }
        }
        for (auto& resource : resources.sampled_images) {
            u32 set = comp.get_decoration(resource.id, spv::DecorationDescriptorSet);
            u32 binding = comp.get_decoration(resource.id, spv::DecorationBinding);
            assert(set < MAX_SET_COUNT && "supplied set count is greater than valid set count per shader program");
            assert(set < MAX_BINDING_PER_SET && "supplied binding count is greater than valid binding count per shader program");

            const spirv_cross::SPIRType &type = comp.get_type(resource.base_type_id);
            program._interface.sets[set].id = set;
            program._interface.sets[set].bindingCount++;

            program._interface.sets[set].bindings[binding].id = binding;
            program._interface.sets[set].bindings[binding].type = ShaderInterface::BindingType::SAMPLER;
            program._interface.sets[set].bindings[binding].dimensions = 2;
            program._interface.sets[set].bindings[binding].name = comp.get_name(resource.id);
            program._interface.sets[set].bindings[binding].stage |= stage.second;
        }
        for (auto& resource : resources.storage_images) {
            u32 set = comp.get_decoration(resource.id, spv::DecorationDescriptorSet);
            u32 binding = comp.get_decoration(resource.id, spv::DecorationBinding);
            assert(set < MAX_SET_COUNT && "supplied set count is greater than valid set count per shader program");
            assert(set < MAX_BINDING_PER_SET && "supplied binding count is greater than valid binding count per shader program");

            const spirv_cross::SPIRType &type = comp.get_type(resource.base_type_id);
            program._interface.sets[set].id = set;
            program._interface.sets[set].bindingCount++;

            program._interface.sets[set].bindings[binding].id = binding;
            program._interface.sets[set].bindings[binding].type = ShaderInterface::BindingType::STORAGE_IMAGE;
            program._interface.sets[set].bindings[binding].dimensions = 2;
            program._interface.sets[set].bindings[binding].name = comp.get_name(resource.id);
            program._interface.sets[set].bindings[binding].stage |= stage.second;
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

//    if (_stages.size() > 1)
//        pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayout setLayouts[MAX_SET_COUNT] = {};
    u32 setCount = 0;
    for (u32 i = 0; i < MAX_SET_COUNT; i++) {
        if (driver.getBindlessIndex() == i) {
            setLayouts[i] = driver.bindlessLayout();
            continue;
        }
        VkDescriptorSetLayoutBinding layoutBinding[MAX_BINDING_PER_SET];
        u32 layoutBindingCount = 0;
        auto& set = program._interface.sets[i];
        if (set.bindingCount > 0)
            setCount = i;
        for (u32 j = 0; set.bindingCount > 0 && j < MAX_BINDING_PER_SET; j++) {
            auto& binding = set.bindings[j];

            if (binding.type == ShaderInterface::BindingType::UNIFORM || binding.type == ShaderInterface::BindingType::SAMPLER) {
                if (binding.byteSize == 0 && binding.dimensions == 0)
                    break;
            }

            layoutBinding[j].binding = j;
            VkDescriptorType type;
            switch (binding.type) {
                case ShaderInterface::BindingType::UNIFORM:
                    type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                    break;
                case ShaderInterface::BindingType::SAMPLER:
                    type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    break;
                case ShaderInterface::BindingType::STORAGE_IMAGE:
                    type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                    break;
                case ShaderInterface::BindingType::STORAGE_BUFFER:
                    type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                    break;
                default:
                    type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            }
            layoutBinding[j].descriptorType = type;
            layoutBinding[j].descriptorCount = 1;
            layoutBinding[j].stageFlags = getShaderStage(binding.stage);
            layoutBinding[j].pImmutableSamplers = nullptr;
            layoutBindingCount++;
        }

        setLayouts[i] = driver.getSetLayout({layoutBinding, layoutBindingCount});
    }

    program._interface._setCount = setCount + 1;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = MAX_SET_COUNT;
    pipelineLayoutInfo.pSetLayouts = setLayouts;

    pipelineLayoutInfo.pushConstantRangeCount = hasPushConstant ? 1 : 0;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstant;

    VkPipelineLayout  pipelineLayout;
    vkCreatePipelineLayout(driver.context().device(), &pipelineLayoutInfo, nullptr, &pipelineLayout);

    program._layout = pipelineLayout;
    for (u32 i = 0; i < MAX_SET_COUNT; i++)
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
    if (this == &rhs)
        return;
    std::swap(_device, rhs._device);
    std::swap(_stages, rhs._stages);
    for (u32 i = 0; i < MAX_SET_COUNT; i++)
        _setLayout[i] = rhs._setLayout[i];
    std::swap(_layout, rhs._layout);
    std::swap(_interface, rhs._interface);
    std::swap(_stageFlags, rhs._stageFlags);
}

cala::backend::vulkan::ShaderProgram &cala::backend::vulkan::ShaderProgram::operator=(ShaderProgram &&rhs) noexcept {
    if (this == &rhs)
        return *this;
    std::swap(_device, rhs._device);
    std::swap(_device, rhs._device);
    std::swap(_stages, rhs._stages);
    for (u32 i = 0; i < MAX_SET_COUNT; i++)
        _setLayout[i] = rhs._setLayout[i];
    std::swap(_layout, rhs._layout);
    std::swap(_interface, rhs._interface);
    std::swap(_stageFlags, rhs._stageFlags);
    return *this;
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