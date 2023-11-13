#include "Cala/backend/vulkan/ShaderModuleInterface.h"
#include <SPIRV-Cross/spirv_cross.hpp>

cala::backend::vulkan::ShaderModuleInterface::ShaderModuleInterface(std::span<u32> spirv, cala::backend::ShaderStage stage)
    : _stage(stage)
{
    if (spirv.empty())
        return;
    spirv_cross::Compiler compiler(spirv.data(), spirv.size());
    spirv_cross::ShaderResources resources = compiler.get_shader_resources();

    for (u32 i = 0; i < resources.push_constant_buffers.size() && i < 5; i++) {
        auto& pushConstant = resources.push_constant_buffers[i];
        const spirv_cross::SPIRType& type = compiler.get_type(pushConstant.base_type_id);

        u32 size = compiler.get_declared_struct_size(type);

        u32 offset = 1000;

        for (u32 member = 0; member < type.member_types.size(); member++) {
            u32 memberOffset = compiler.type_struct_member_offset(type, i);
            offset = std::min(offset, memberOffset);
        }

        PushConstant range { size, offset };

        _pushConstantRanges[i] = range;
        _pushConstantRangeCount = i + 1;
    }

    for (u32 i = 0; i < resources.uniform_buffers.size(); i++) {
        auto& uniformBuffer = resources.uniform_buffers[i];

        u32 set = compiler.get_decoration(uniformBuffer.id, spv::DecorationDescriptorSet);
        u32 binding = compiler.get_decoration(uniformBuffer.id, spv::DecorationBinding);
        assert(set < MAX_SET_COUNT);
        assert(binding < MAX_BINDING_PER_SET);

        const spirv_cross::SPIRType &type = compiler.get_type(uniformBuffer.base_type_id);
        u32 size = compiler.get_declared_struct_size(type);

        _sets[set].bindings[binding].binding = binding;
        _sets[set].bindings[binding].size = size;
        _sets[set].bindings[binding].type = BindingType::UNIFORM_BUFFER;
        _sets[set].bindingCount = std::max(_sets[set].bindingCount, binding + 1);
        if (_sets[set].bindingCount > 0)
            _setCount = std::max(_setCount, _sets[set].bindingCount);
    }

    for (u32 i = 0; i < resources.storage_buffers.size(); i++) {
        auto& storageBuffer = resources.storage_buffers[i];

        u32 set = compiler.get_decoration(storageBuffer.id, spv::DecorationDescriptorSet);
        u32 binding = compiler.get_decoration(storageBuffer.id, spv::DecorationBinding);
        assert(set < MAX_SET_COUNT);
        assert(binding < MAX_BINDING_PER_SET);

        const spirv_cross::SPIRType &type = compiler.get_type(storageBuffer.base_type_id);
        u32 size = compiler.get_declared_struct_size(type);

        _sets[set].bindings[binding].binding = binding;
        _sets[set].bindings[binding].size = size;
        _sets[set].bindings[binding].type = BindingType::STORAGE_BUFFER;
        _sets[set].bindingCount = std::max(_sets[set].bindingCount, binding + 1);
        if (_sets[set].bindingCount > 0)
            _setCount = std::max(_setCount, _sets[set].bindingCount);
    }

    for (u32 i = 0; i < resources.sampled_images.size(); i++) {
        auto& sampledImage = resources.sampled_images[i];

        u32 set = compiler.get_decoration(sampledImage.id, spv::DecorationDescriptorSet);
        u32 binding = compiler.get_decoration(sampledImage.id, spv::DecorationBinding);
        assert(set < MAX_SET_COUNT);
        assert(binding < MAX_BINDING_PER_SET);

        const spirv_cross::SPIRType &type = compiler.get_type(sampledImage.base_type_id);

        _sets[set].bindings[binding].binding = binding;
        _sets[set].bindings[binding].size = 1;
        _sets[set].bindings[binding].type = BindingType::SAMPLED_IMAGE;
        _sets[set].bindingCount = std::max(_sets[set].bindingCount, binding + 1);
        if (_sets[set].bindingCount > 0)
            _setCount = std::max(_setCount, _sets[set].bindingCount);
    }

    for (u32 i = 0; i < resources.storage_images.size(); i++) {
        auto& storageImage = resources.storage_images[i];

        u32 set = compiler.get_decoration(storageImage.id, spv::DecorationDescriptorSet);
        u32 binding = compiler.get_decoration(storageImage.id, spv::DecorationBinding);
        assert(set < MAX_SET_COUNT);
        assert(binding < MAX_BINDING_PER_SET);

        const spirv_cross::SPIRType &type = compiler.get_type(storageImage.base_type_id);

        _sets[set].bindings[binding].binding = binding;
        _sets[set].bindings[binding].size = 1;
        _sets[set].bindings[binding].type = BindingType::STORAGE_IMAGE;
        _sets[set].bindingCount = std::max(_sets[set].bindingCount, binding + 1);
        if (_sets[set].bindingCount > 0)
            _setCount = std::max(_setCount, _sets[set].bindingCount);
    }

}