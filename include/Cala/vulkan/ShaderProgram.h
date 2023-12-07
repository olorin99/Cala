#ifndef CALA_SHADERPROGRAM_H
#define CALA_SHADERPROGRAM_H

#include "volk/volk.h"
#include "ShaderInterface.h"
#include "ShaderModule.h"
#include <Cala/vulkan/primitives.h>
#include <vector>
#include "Handle.h"
#include <filesystem>
#include "PipelineLayout.h"

namespace cala::vk {

    class Device;
    class CommandBuffer;

    class ShaderProgram {
    public:

        ShaderProgram();

        ShaderProgram(Device* device, std::span<const ShaderModuleHandle> modules);

        ShaderProgram(const ShaderProgram& rhs) = delete;

        ShaderProgram(ShaderProgram&& rhs) noexcept;

        ShaderProgram& operator=(const ShaderProgram& rhs) = delete;

        ShaderProgram& operator=(ShaderProgram&& rhs) noexcept;

        operator bool() const noexcept { return _device && !_modules.empty(); }


        VkPipelineLayout layout() const;

        VkDescriptorSetLayout setLayout(u32 set) const;

        bool stagePresent(ShaderStage stageFlags) const { return interface().stagePresent(stageFlags); }

        const ShaderInterface& interface() const;

//    private:
//        friend Builder;
        friend CommandBuffer;

        Device* _device;
        std::vector<ShaderModuleHandle> _modules;

        PipelineLayoutHandle _pipelineLayout;

    };

}

#endif //CALA_SHADERPROGRAM_H
