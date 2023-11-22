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

//        u32 setSize(u32 set) const {
//            return sets[set].s;
//        }

        u32 bindingSize(u32 set, u32 binding) const {
            return sets[set].bindings[binding].size;
        }

        bool bindingHasMember(u32 set, u32 binding, const std::string& memberName) const;

        vulkan::ShaderModuleInterface::Member getBindingMember(u32 set, u32 binding, const std::string& memberName) const;

        std::vector<vulkan::ShaderModuleInterface::Member> getBindingMemberList(u32 set, u32 binding) const;

        bool setPresent(u32 set) const {
            return sets[set].bindingCount > 0;
        }

        u32 setCount() const { return _setCount; }

        bool stagePresent(ShaderStage stage) const { return (_stages & stage) == stage; }

//    private:
        friend cala::backend::vulkan::ShaderProgram;

        u32 _setCount = 0;
        vulkan::ShaderModuleInterface::Set sets[MAX_SET_COUNT]{};
        u32 pushConstantRangeCount = 0;
        std::vector<vulkan::ShaderModuleInterface::PushConstant> pushConstantRanges;

        ShaderStage _stages = ShaderStage::NONE;

    };

}

#endif //CALA_SHADERINTERFACE_H
