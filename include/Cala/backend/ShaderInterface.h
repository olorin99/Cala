#ifndef CALA_SHADERINTERFACE_H
#define CALA_SHADERINTERFACE_H

#include "Ende/platform.h"
#include <unordered_map>
#include <string>
#include <Cala/backend/primitives.h>
#include <Cala/backend/vulkan/ShaderModuleInterface.h>

namespace cala::backend {

    namespace vulkan {
        class ShaderProgram;
    }

    class ShaderInterface {
    public:

        ShaderInterface(std::span<vulkan::ShaderModuleInterface> moduleInterfaces);

        ~ShaderInterface();

//        i32 getUniformOffset(u32 set, u32 binding, const char* name) const;

//        i32 getUniformSize(u32 set, u32 binding, const char* name) const;

//        i32 getUniformOffset(u32 set, const char* name) const;

//        i32 getUniformSize(u32 set, const char* name) const;

//        i32 getSamplerBinding(u32 set, const char* name) const;

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
            NONE = 0,
            PUSH_CONSTANT = 1,
            UNIFORM_BUFFER = 2,
            STORAGE_BUFFER = 3,
            SAMPLED_IMAGE = 4,
            STORAGE_IMAGE = 5
        };

        struct BindingMember {
            u32 offset;
            u32 size;
        };

//        std::unordered_map<std::string, BindingMember>& getMemberList(u32 set, u32 binding);
//        const std::unordered_map<std::string, BindingMember>& getMemberList(u32 set, u32 binding) const;

        struct {
            u32 id = 0;
            u32 byteSize = 0;
            u32 bindingCount = 0;
            struct {
                u32 id = 0;
                BindingType type = BindingType::NONE;
                u32 byteSize = 0;
//                std::unordered_map<std::string, BindingMember> members;
//                u32 dimensions = 0;
//                std::string name;
                ShaderStage stage = ShaderStage::NONE;
            } bindings[MAX_BINDING_PER_SET] {};
        } sets[MAX_SET_COUNT] {};
        u32 _setCount = 0;

        struct {
            u32 byteSize = 0;
            u32 offset = 0;
            ShaderStage stage = ShaderStage::NONE;
//            std::unordered_map<std::string, BindingMember> members;
        } pushConstants[10] {};
        u32 pushConstantRanges = 0;

    };

}

#endif //CALA_SHADERINTERFACE_H
