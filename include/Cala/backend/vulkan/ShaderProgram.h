#ifndef CALA_SHADERPROGRAM_H
#define CALA_SHADERPROGRAM_H

#include <volk.h>
#include <Cala/backend/ShaderInterface.h>
#include <Cala/backend/vulkan/ShaderModule.h>
#include <Cala/backend/primitives.h>
#include <vector>
#include <Cala/backend/vulkan/Handle.h>
#include <filesystem>
#include <Cala/backend/vulkan/PipelineLayout.h>

namespace cala::backend::vulkan {

    class Device;
    class CommandBuffer;

    class ShaderProgram {
    public:

        ShaderProgram(Device* device, std::span<const ShaderModuleHandle> modules);

        ShaderProgram(Device* device);

        ShaderProgram(const ShaderProgram& rhs) = delete;

        ShaderProgram(ShaderProgram&& rhs) noexcept;

        ShaderProgram& operator=(const ShaderProgram& rhs) = delete;

        ShaderProgram& operator=(ShaderProgram&& rhs) noexcept;


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
