#include "Cala/backend/ShaderInterface.h"

cala::backend::ShaderInterface::ShaderInterface(std::span<vulkan::ShaderModuleInterface> moduleInterfaces) {
    for (auto& interface : moduleInterfaces) {
        _stages |= interface._stage;
        for (u32 set = 0; set < MAX_SET_COUNT; set++) {
            _setCount = std::max(_setCount, interface._setCount);
            sets[set].bindingCount = std::max(sets[set].bindingCount, interface._sets[set].bindingCount);

            for (u32 binding = 0; binding < interface._sets[set].bindingCount; binding++) {
                sets[set].bindings[binding].id = interface._sets[set].bindings[binding].binding;
                sets[set].bindings[binding].stage |= interface._stage;
                sets[set].bindings[binding].byteSize = interface._sets[set].bindings[binding].size;
                sets[set].bindings[binding].type = static_cast<BindingType>(interface._sets[set].bindings[binding].type);
            }
        }

        for (u32 i = 0; i < interface._pushConstantRangeCount; i++) {
            pushConstants[i].stage |= interface._stage;
            pushConstants[i].byteSize = interface._pushConstantRanges[i].size;
            pushConstants[i].offset = interface._pushConstantRanges[i].offset;
        }
        pushConstantRanges = std::max(pushConstantRanges, interface._pushConstantRangeCount);
    }
}

cala::backend::ShaderInterface::~ShaderInterface() {
//    for (auto& set : sets) {
//        for (auto& binding : set.bindings) {
//            binding.members.clear();
//        }
//    }
}

//i32 cala::backend::ShaderInterface::getUniformOffset(u32 set, u32 binding, const char *name) const {
//    auto it = getMemberList(set, binding).find(name);
//    if (it == getMemberList(set, binding).end())
//        return -1;
//    return it->second.offset;
//}
//
//i32 cala::backend::ShaderInterface::getUniformSize(u32 set, u32 binding, const char *name) const {
//    auto it = getMemberList(set, binding).find(name);
//    if (it == getMemberList(set, binding).end())
//        return -1;
//    return it->second.size;
//}
//
//i32 cala::backend::ShaderInterface::getUniformOffset(u32 set, const char *name) const {
//    for (u32 binding = 0; binding < 8; binding++) {
//        i32 offset = getUniformOffset(set, binding, name);
//        if (offset >= 0)
//            return offset;
//    }
//    return -1;
//}
//
//i32 cala::backend::ShaderInterface::getUniformSize(u32 set, const char *name) const {
//    for (u32 binding = 0; binding < 8; binding++) {
//        i32 offset = getUniformSize(set, binding, name);
//        if (offset >= 0)
//            return offset;
//    }
//    return -1;
//}
//
//i32 cala::backend::ShaderInterface::getSamplerBinding(u32 set, const char *name) const {
//    for (u32 binding = 0; binding < 8; binding++) {
//        auto& bindingRef = sets[set].bindings[binding];
//        if (bindingRef.type == BindingType::SAMPLED_IMAGE) {
////            if (bindingRef.name == name)
////                return bindingRef.id;
//        }
//    }
//    return -1;
//}
//
//std::unordered_map<std::string, cala::backend::ShaderInterface::BindingMember> &cala::backend::ShaderInterface::getMemberList(u32 set, u32 binding) {
//    return sets[set].bindings[binding].members;
//}
//
//const std::unordered_map<std::string, cala::backend::ShaderInterface::BindingMember> &cala::backend::ShaderInterface::getMemberList(u32 set, u32 binding) const {
//    return sets[set].bindings[binding].members;
//}
