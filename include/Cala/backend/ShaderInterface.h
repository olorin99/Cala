#ifndef CALA_SHADERINTERFACE_H
#define CALA_SHADERINTERFACE_H

#include "Ende/platform.h"
#include <unordered_map>

namespace cala::backend {

    class ShaderInterface {
    public:

        i32 getUniformOffset(u32 set, u32 binding, const char* name);

        i32 getUniformSize(u32 set, u32 binding, const char* name);

        i32 getUniformOffset(u32 set, const char* name);

        i32 getUniformSize(u32 set, const char* name);

        i32 getSamplerBinding(u32 set, const char* name);

//    private:

        enum class BindingType {
            UNIFORM,
            SAMPLER
        };

        struct BindingMember {
            u32 offset;
            u32 size;
        };

        std::unordered_map<std::string, BindingMember>& getMemberList(u32 set, u32 binding);

        struct {
            u32 id = 0;
            u32 byteSize = 0;
            u32 bindingCount = 0;
            struct {
                u32 id = 0;
                BindingType type = BindingType::UNIFORM;
                u32 byteSize = 0;
                std::unordered_map<std::string, BindingMember> members;
                u32 dimensions;
                std::string name;
            } bindings[8];
        } sets[4];

    };

}

#endif //CALA_SHADERINTERFACE_H
