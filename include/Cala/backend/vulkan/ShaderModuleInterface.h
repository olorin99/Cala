#ifndef CALA_SHADERMODULEINTERFACE_H
#define CALA_SHADERMODULEINTERFACE_H

#include <Ende/platform.h>
#include <Cala/backend/primitives.h>
#include <span>

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

        struct Binding {
            u32 binding = 0;
            u32 size = 0;
            BindingType type = BindingType::NONE;
        };

        struct Set {
            u32 set = 0;
            u32 bindingCount = 0;
            Binding bindings[MAX_BINDING_PER_SET]{};
        };

        struct PushConstant {
            u32 size = 0;
            u32 offset = 0;
        };

        u32 _setCount = 0;
        Set _sets[MAX_SET_COUNT]{};
        u32 _pushConstantRangeCount = 0;
        PushConstant _pushConstantRanges[5]{};

        ShaderStage _stage;

    };

}

#endif //CALA_SHADERMODULEINTERFACE_H
