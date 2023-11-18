#include "Cala/backend/vulkan/ShaderModuleInterface.h"
#include <SPIRV-Cross/spirv_cross.hpp>

auto typeToMemberType(const spirv_cross::SPIRType& type) {
    switch (type.basetype) {
        case spirv_cross::SPIRType::BaseType::Struct:
            return cala::backend::vulkan::ShaderModuleInterface::MemberType::STRUCT;
        case spirv_cross::SPIRType::Unknown:
            break;
        case spirv_cross::SPIRType::Void:
            break;
        case spirv_cross::SPIRType::Boolean:
            return cala::backend::vulkan::ShaderModuleInterface::MemberType::BOOL;
        case spirv_cross::SPIRType::SByte:
            break;
        case spirv_cross::SPIRType::UByte:
            break;
        case spirv_cross::SPIRType::Short:
            break;
        case spirv_cross::SPIRType::UShort:
            break;
        case spirv_cross::SPIRType::Int:
            return cala::backend::vulkan::ShaderModuleInterface::MemberType::INT;
        case spirv_cross::SPIRType::UInt:
            return cala::backend::vulkan::ShaderModuleInterface::MemberType::UINT;
        case spirv_cross::SPIRType::Int64:
            break;
        case spirv_cross::SPIRType::UInt64:
            break;
        case spirv_cross::SPIRType::AtomicCounter:
            break;
        case spirv_cross::SPIRType::Half:
            break;
        case spirv_cross::SPIRType::Float:
            return cala::backend::vulkan::ShaderModuleInterface::MemberType::FLOAT;
        case spirv_cross::SPIRType::Double:
            break;
        case spirv_cross::SPIRType::Image:
            break;
        case spirv_cross::SPIRType::SampledImage:
            break;
        case spirv_cross::SPIRType::Sampler:
            break;
        case spirv_cross::SPIRType::AccelerationStructure:
            break;
        case spirv_cross::SPIRType::RayQuery:
            break;
        case spirv_cross::SPIRType::ControlPointArray:
            break;
        case spirv_cross::SPIRType::Interpolant:
            break;
        case spirv_cross::SPIRType::Char:
            break;
    }
    return cala::backend::vulkan::ShaderModuleInterface::MemberType::STRUCT;
};


std::vector<cala::backend::vulkan::ShaderModuleInterface::Member> getStructMembers(const spirv_cross::SPIRType& type, spirv_cross::Compiler& compiler) {
    std::vector<cala::backend::vulkan::ShaderModuleInterface::Member> members;
    u32 memberCount = type.member_types.size();
    for (u32 memberIndex = 0; memberIndex < memberCount; memberIndex++) {
        auto& memberType = compiler.get_type(type.member_types[memberIndex]);
        const std::string& name = compiler.get_member_name(type.self, memberIndex);
        u32 a = compiler.get_declared_struct_size_runtime_array(type, 1);
        u32 memberSize = compiler.get_declared_struct_member_size(type, memberIndex);
        u32 memberOffset = compiler.type_struct_member_offset(type, memberIndex);
        members.push_back({ name, typeToMemberType(memberType), memberSize, memberOffset });
    }
    return members;
}

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


        u32 memberCount = type.member_types.size();
        for (u32 memberIndex = 0; memberIndex < memberCount; memberIndex++) {
            auto& memberType = compiler.get_type(type.member_types[memberIndex]);
            const std::string& name = compiler.get_member_name(type.self, memberIndex);
            u32 a = compiler.get_declared_struct_size_runtime_array(type, 1);
            u32 memberSize = compiler.get_declared_struct_member_size(type, memberIndex);
            u32 memberOffset = compiler.type_struct_member_offset(type, memberIndex);

            if (memberType.basetype == spirv_cross::SPIRType::Struct) {
                auto structMembers = getStructMembers(memberType, compiler);
                for (auto& member : structMembers) {
                    member.offset += memberOffset;
                    _sets[set].bindings[binding].members[member.name] = member;
                }
            } else {
                _sets[set].bindings[binding].members[name] = { name, typeToMemberType(memberType), memberSize, memberOffset };
            }
        }

        _sets[set].bindings[binding].binding = binding;
        _sets[set].bindings[binding].size = size;
        _sets[set].bindings[binding].type = BindingType::UNIFORM_BUFFER;
        _sets[set].bindingCount = std::max(_sets[set].bindingCount, binding + 1);
        if (_sets[set].bindingCount > 0)
            _setCount = std::max(_setCount, set + 1);
    }

    for (u32 i = 0; i < resources.storage_buffers.size(); i++) {
        auto& storageBuffer = resources.storage_buffers[i];

        u32 set = compiler.get_decoration(storageBuffer.id, spv::DecorationDescriptorSet);
        u32 binding = compiler.get_decoration(storageBuffer.id, spv::DecorationBinding);
        assert(set < MAX_SET_COUNT);
        assert(binding < MAX_BINDING_PER_SET);

        const spirv_cross::SPIRType &type = compiler.get_type(storageBuffer.base_type_id);
        u32 size = compiler.get_declared_struct_size(type);
        if (!type.array.empty()) { // if array then size is the stride of the array e.g. size of element
            size = compiler.get_declared_struct_size_runtime_array(type, 1);
        }

        u32 totalMemberSize = 0;
        u32 memberCount = type.member_types.size();
        for (u32 memberIndex = 0; memberIndex < memberCount; memberIndex++) {
            auto& memberType = compiler.get_type(type.member_types[memberIndex]);
            const std::string& name = compiler.get_member_name(type.self, memberIndex);
            u32 memberSize = compiler.get_declared_struct_member_size(type, memberIndex);
            if (!memberType.array.empty()) { // if array then size is the stride of the array e.g. size of element
                memberSize = compiler.get_declared_struct_size_runtime_array(type, 1);
            }
            u32 memberOffset = compiler.type_struct_member_offset(type, memberIndex);
            totalMemberSize += memberSize;

            if (memberType.basetype == spirv_cross::SPIRType::Struct) {
                auto structMembers = getStructMembers(memberType, compiler);
                for (auto& member : structMembers) { // flatten members so all on same level with adjusted offset
                    member.offset += memberOffset;
                    _sets[set].bindings[binding].members[member.name] = member;
                }
            } else {
                _sets[set].bindings[binding].members[name] = { name, typeToMemberType(memberType), memberSize, memberOffset };
            }
        }
        if (size == 0)
            size = totalMemberSize;

        _sets[set].bindings[binding].binding = binding;
        _sets[set].bindings[binding].size = size;
        _sets[set].bindings[binding].type = BindingType::STORAGE_BUFFER;
        _sets[set].bindingCount = std::max(_sets[set].bindingCount, binding + 1);
        if (_sets[set].bindingCount > 0)
            _setCount = std::max(_setCount, set + 1);
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
            _setCount = std::max(_setCount, set + 1);
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
            _setCount = std::max(_setCount, set + 1);
    }

}