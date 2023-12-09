#ifndef CALA_SHADERMODULE_H
#define CALA_SHADERMODULE_H

#include <vector>
#include <Cala/vulkan/primitives.h>
#include <volk/volk.h>
#include <string>
#include <Cala/vulkan/ShaderModuleInterface.h>
#include <Ende/math/Vec.h>

namespace cala::vk {

    class Device;

    class ShaderModule {
    public:

        ShaderModule(Device* device);

        ShaderStage stage() const { return _stage; }

        VkShaderModule module() const { return _module; }

        const ShaderModuleInterface& interface() const { return _interface; }

        const ende::math::Vec<3, u32>& localSize() const { return _localSize; }

    private:
        friend Device;

        Device* _device;
        VkShaderModule _module;
        ShaderStage _stage;
        std::string _main;
        ShaderModuleInterface _interface;
        ende::math::Vec<3, u32> _localSize = {0, 0, 0};

    };

}

#endif //CALA_SHADERMODULE_H
