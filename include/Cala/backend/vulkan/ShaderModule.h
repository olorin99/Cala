#ifndef CALA_SHADERMODULE_H
#define CALA_SHADERMODULE_H

#include <vector>
#include <Cala/backend/primitives.h>
#include <volk.h>
#include <string>

namespace cala::backend::vulkan {

    class Device;

    class ShaderModule {
    public:

        ShaderModule(Device* device);

        ShaderStage stage() const { return _stage; }

        VkShaderModule module() const { return _module; }

    private:
        friend Device;

        Device* _device;
        VkShaderModule _module;
        ShaderStage _stage;
        std::string _main;

    };

}

#endif //CALA_SHADERMODULE_H
