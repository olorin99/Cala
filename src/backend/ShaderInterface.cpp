#include "Cala/backend/ShaderInterface.h"

i32 cala::backend::ShaderInterface::getUniformOffset(u32 set, u32 binding, const char *name) {
    auto it = getMemberList(set, binding).find(name);
    if (it == getMemberList(set, binding).end())
        return -1;
    return it->second.offset;
}

i32 cala::backend::ShaderInterface::getUniformSize(u32 set, u32 binding, const char *name) {
    auto it = getMemberList(set, binding).find(name);
    if (it == getMemberList(set, binding).end())
        return -1;
    return it->second.size;
}

i32 cala::backend::ShaderInterface::getUniformOffset(u32 set, const char *name) {
    for (u32 binding = 0; binding < 8; binding++) {
        i32 offset = getUniformOffset(set, binding, name);
        if (offset >= 0)
            return offset;
    }
    return -1;
}

i32 cala::backend::ShaderInterface::getUniformSize(u32 set, const char *name) {
    for (u32 binding = 0; binding < 8; binding++) {
        i32 offset = getUniformSize(set, binding, name);
        if (offset >= 0)
            return offset;
    }
    return -1;
}

std::unordered_map<std::string, cala::backend::ShaderInterface::BindingMember> &cala::backend::ShaderInterface::getMemberList(u32 set, u32 binding) {
    return sets[set].bindings[binding].members;
}
