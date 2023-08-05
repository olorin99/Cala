#ifndef CALA_SHADERPROGRAM_H
#define CALA_SHADERPROGRAM_H

#include <vulkan/vulkan.h>
#include <Ende/Vector.h>
#include <Cala/backend/ShaderInterface.h>
#include <Cala/backend/primitives.h>
#include <vector>
#include <Ende/filesystem/Path.h>

namespace cala::backend::vulkan {

    class Device;
    class CommandBuffer;

    class ShaderProgram {
    public:

        class Builder {
        public:

            Builder& addStageSPV(const std::vector<u32>& code, ShaderStage stage);

            Builder& addStageGLSL(const ende::fs::Path& path, ShaderStage stage, const std::vector<std::pair<const char*, std::string>>& macros = {});

            ShaderProgram compile(Device& driver);

        private:

            ende::Vector<std::pair<std::vector<u32>, ShaderStage>> _stages;

        };

        static Builder create();


        ShaderProgram(VkDevice device);

        ~ShaderProgram();

        ShaderProgram(const ShaderProgram& rhs) = delete;

        ShaderProgram(ShaderProgram&& rhs) noexcept;

        ShaderProgram& operator=(const ShaderProgram& rhs) = delete;

        ShaderProgram& operator=(ShaderProgram&& rhs) noexcept;


        VkPipelineLayout layout();

        bool stagePresent(ShaderStage stageFlags) const;

        const ShaderInterface& interface() const { return _interface; }

    private:
        friend Builder;
        friend CommandBuffer;

        VkDevice _device;
        ende::Vector<VkPipelineShaderStageCreateInfo> _stages;
        VkDescriptorSetLayout _setLayout[MAX_SET_COUNT];
        VkPipelineLayout _layout;

        ShaderInterface _interface;
        ShaderStage _stageFlags;

    };

}

#endif //CALA_SHADERPROGRAM_H
