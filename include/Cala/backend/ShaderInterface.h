#ifndef CALA_SHADERINTERFACE_H
#define CALA_SHADERINTERFACE_H

#include "Ende/platform.h"
#include <unordered_map>
#include <string>
#include <Cala/backend/primitives.h>

namespace cala::backend {

    namespace vulkan {
        class ShaderProgram;
    }

    class ShaderInterface {
    public:

        i32 getUniformOffset(u32 set, u32 binding, const char* name) const;

        i32 getUniformSize(u32 set, u32 binding, const char* name) const;

        i32 getUniformOffset(u32 set, const char* name) const;

        i32 getUniformSize(u32 set, const char* name) const;

        i32 getSamplerBinding(u32 set, const char* name) const;

        u32 setSize(u32 set) const {
            return sets[set].byteSize;
        }

        bool setPresent(u32 set) const {
            return sets[set].bindingCount > 0;
        }

        u32 setCount() const { return _setCount; }

//    private:
        friend cala::backend::vulkan::ShaderProgram;

        enum class BindingType {
            PUSH_CONSTANT,
            UNIFORM,
            SAMPLER,
            STORAGE_IMAGE,
            STORAGE_BUFFER
        };

        struct BindingMember {
            u32 offset;
            u32 size;
        };

        std::unordered_map<std::string, BindingMember>& getMemberList(u32 set, u32 binding);
        const std::unordered_map<std::string, BindingMember>& getMemberList(u32 set, u32 binding) const;

        struct {
            u32 id = 0;
            u32 byteSize = 0;
            u32 bindingCount = 0;
            struct {
                u32 id = 0;
                BindingType type = BindingType::UNIFORM;
                u32 byteSize = 0;
                std::unordered_map<std::string, BindingMember> members;
                u32 dimensions = 0;
                std::string name;
                ShaderStage stage;
            } bindings[MAX_BINDING_PER_SET];
        } sets[MAX_SET_COUNT];
        u32 _setCount = 0;
        u32 _bindlessIndex = 0;

        struct {
            u32 byteSize = 0;
            ShaderStage stage;
            std::unordered_map<std::string, BindingMember> members;
        } pushConstants;

    };

}

#endif //CALA_SHADERINTERFACE_H
