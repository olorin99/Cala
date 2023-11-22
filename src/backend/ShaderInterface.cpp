#include "Cala/backend/ShaderInterface.h"

cala::backend::ShaderInterface::ShaderInterface(std::span<vulkan::ShaderModuleInterface> moduleInterfaces) {
    for (auto& interface : moduleInterfaces) {
        _stages |= interface._stage;
        for (u32 set = 0; set < MAX_SET_COUNT; set++) {
            _setCount = std::max(_setCount, interface._setCount);
            sets[set].bindingCount = std::max(sets[set].bindingCount, interface._sets[set].bindingCount);

            for (u32 binding = 0; binding < interface._sets[set].bindingCount; binding++) {
                sets[set].bindings[binding].binding = interface._sets[set].bindings[binding].binding;
                sets[set].bindings[binding].size = interface._sets[set].bindings[binding].size;
                sets[set].bindings[binding].type = interface._sets[set].bindings[binding].type;
                sets[set].bindings[binding].stages |= interface._stage;
                sets[set].bindings[binding].members = interface._sets[set].bindings[binding].members;
            }
        }

        for (u32 i = 0; i < interface._pushConstantRangeCount; i++) {
            pushConstantRanges.push_back({});
            pushConstantRanges.back().stages |= interface._stage;
            pushConstantRanges.back().size = interface._pushConstantRanges[i].size;
            pushConstantRanges.back().offset = interface._pushConstantRanges[i].offset;
        }
        pushConstantRangeCount = std::max(pushConstantRangeCount, interface._pushConstantRangeCount);
    }
}

cala::backend::ShaderInterface::~ShaderInterface() {
//    for (auto& set : sets) {
//        for (auto& binding : set.bindings) {
//            binding.members.clear();
//        }
//    }
}

bool cala::backend::ShaderInterface::bindingHasMember(u32 set, u32 binding, const std::string& memberName) const {
    auto& b = sets[set].bindings[binding];
    auto it = b.members.find(memberName);
    if (it != b.members.end())
        return true;
    return false;
}

cala::backend::vulkan::ShaderModuleInterface::Member cala::backend::ShaderInterface::getBindingMember(u32 set, u32 binding, const std::string& memberName) const {
    auto& b = sets[set].bindings[binding];
    auto it = b.members.find(memberName);
    if (it != b.members.end()) {
        return it->second;
    }
    return {};
}

std::vector<cala::backend::vulkan::ShaderModuleInterface::Member> cala::backend::ShaderInterface::getBindingMemberList(u32 set, u32 binding) const {
    std::vector<cala::backend::vulkan::ShaderModuleInterface::Member> members;
    for (auto [key, value] : sets[set].bindings[binding].members)
        members.push_back(value);
    return members;
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
