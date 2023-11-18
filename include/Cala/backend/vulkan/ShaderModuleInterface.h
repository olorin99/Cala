#ifndef CALA_SHADERMODULEINTERFACE_H
#define CALA_SHADERMODULEINTERFACE_H

#include <Ende/platform.h>
#include <Cala/backend/primitives.h>
#include <span>
#include "../third_party/tsl/robin_map.h"

namespace cala::backend::vulkan {

    class ShaderModuleInterface {
    public:

        ShaderModuleInterface(std::span<u32> spirv, ShaderStage stage);

        enum class BindingType {
            NONE = 0,
            PUSH_CONSTANT = 1,
            UNIFORM_BUFFER = 2,
            STORAGE_BUFFER = 3,
            SAMPLED_IMAGE = 4,
            STORAGE_IMAGE = 5
        };

//    private:

        enum class MemberType {
            STRUCT,
            BOOL,
            INT,
            UINT,
            FLOAT,
            VEC3F,
            VEC4F,
            IVEC3,
            IVEC4,
            UVEC3,
            UVEC4,
            MAT3F,
            MAT4F
        };

        struct Member {
            std::string name;
            MemberType type = MemberType::STRUCT;
            u32 size = 0;
            u32 offset = 0;
        };

        struct Binding {
            u32 binding = 0;
            u32 size = 0;
            i32 arrayLength = 1;
            BindingType type = BindingType::NONE;
            ShaderStage stages = ShaderStage::NONE;
            tsl::robin_map<std::string, Member> members = {};
        };

        struct Set {
            u32 set = 0;
            u32 bindingCount = 0;
            u32 size = 0;
            Binding bindings[MAX_BINDING_PER_SET]{};
        };

        struct PushConstant {
            u32 size = 0;
            u32 offset = 0;
            ShaderStage stages = ShaderStage::NONE;
        };

        u32 _setCount = 0;
        Set _sets[MAX_SET_COUNT]{};
        u32 _pushConstantRangeCount = 0;
        PushConstant _pushConstantRanges[5]{};

        ShaderStage _stage;

    };

}

#endif //CALA_SHADERMODULEINTERFACE_H
